/* mc_queue.c */

#include <jd_pretty.h>
#include <pthread.h>

#include "mc_queue.h"
#include "mc_util.h"

mc_queue *mc_queue_new(size_t size) {
  mc_queue *q = mc_alloc(sizeof(*q));
  q->pprev = q->pnext = q;
  q->max_size = size;
  q->used = 0;
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->can_get, NULL);
  pthread_cond_init(&q->can_put, NULL);
  return q;
}

mc_queue *mc_queue_unhook(mc_queue *q) {
  q->pprev->pnext = q->pnext;
  q->pnext->pprev = q->pprev;
  q->pprev = q->pnext = q;
  return q;
}

mc_queue *mc_queue_hook(mc_queue *q, mc_queue *nq) {
  mc_queue *qn = q->pnext;
  mc_queue_unhook(nq);
  nq->pprev = q;
  nq->pnext = qn;
  nq->pnext->pprev = nq;
  nq->pprev->pnext = nq;
  return q;
}


static void free_entries(mc_queue_entry *qe) {
  for (mc_queue_entry *next = qe; next; qe = next) {
    next = qe->next;
    /* TODO!! polymorphism! */
    av_free_packet(&qe->d.pkt);
    free(qe);
  }
}

void mc_queue_free(mc_queue *q) {
  if (q) {
    mc_queue_unhook(q);
    free_entries(q->head);
    free_entries(q->free);
    free(q);
  }
}

static mc_queue_entry *get_entry(mc_queue *q) {
  if (q->free) {
    mc_queue_entry *ent = q->free;
    q->free = ent->next;
    ent->eof = 0;
    return ent;
  }

  return mc_alloc(sizeof(mc_queue_entry));
}

typedef void (*put_func)(mc_queue *q, mc_queue_entry *qe, void *ctx);
typedef void (*get_func)(mc_queue *q, mc_queue_entry *qe, void *ctx);

static const char *t_name[] = { "unknown", "packet", "frame" };

static void check_type(mc_queue *q, mc_queue_type t) {
  if (q->t == MC_UNKNOWN)
    q->t = t;
  else if (q->t != t)
    jd_throw("Unexpected %s call on %s queue", t_name[t], t_name[q->t]);
}

static void queue_put(mc_queue *q, put_func pf, void *ctx) {
  mc_queue_entry *qe;

  pthread_mutex_lock(&q->mutex);

  /* A zero sized queue is a dummy - used as the head of a multi-queue set */
  if (!q->max_size) {
    pthread_mutex_unlock(&q->mutex);
    return;
  }

  while (q->used == q->max_size)
    pthread_cond_wait(&q->can_put, &q->mutex);

  qe = get_entry(q);

  if (ctx == NULL) qe->eof = 1;
  else pf(q, qe, ctx);

  qe->next = NULL;
  if (q->tail) q->tail->next = qe;
  else q->head = qe;
  q->tail = qe;
  q->used++;

  pthread_cond_broadcast(&q->can_get);

  pthread_mutex_unlock(&q->mutex);

  if (q->m) {
    pthread_mutex_lock(&q->m->mutex);
    pthread_cond_broadcast(&q->m->can_get);
    pthread_mutex_unlock(&q->m->mutex);
  }
}

static void queue_multi_put(mc_queue *q, put_func pf, void *ctx) {
  mc_queue *nq = q;
  do {
    queue_put(nq, pf, ctx);
    nq = nq->pnext;
  }
  while (nq != q);
}

static int queue_get(mc_queue *q, get_func gf, void *ctx) {
  mc_queue_entry *qe;

  pthread_mutex_lock(&q->mutex);

  /* dummy queue */
  if (!q->max_size) {
    pthread_mutex_unlock(&q->mutex);
    return 0;
  }

  while (!q->head)
    pthread_cond_wait(&q->can_get, &q->mutex);

  qe = q->head;
  q->head = qe->next;
  if (q->head == NULL) q->tail = NULL;

  if (qe->eof) q->eof = 1;
  else gf(q, qe, ctx);

  memset(qe, 0, sizeof(*qe)); /* trample cloned pkt ref */
  qe->next = q->free;
  q->free = qe;
  q->used--;

  pthread_cond_broadcast(&q->can_put);
  pthread_mutex_unlock(&q->mutex);

  return !q->eof;
}

mc_queue_entry *mc_queue_peek(mc_queue *q) {
  /* probably don't need the lock here */
  pthread_mutex_lock(&q->mutex);
  mc_queue_entry *head = q->head;
  pthread_mutex_unlock(&q->mutex);
  return head;
}

mc_queue_merger *mc_queue_merger_new(mc_queue_packet_comparator qc, void *ctx) {
  mc_queue_merger *qm = mc_alloc(sizeof(*qm));
  qm->qc = qc;
  qm->ctx = ctx;
  pthread_mutex_init(&qm->mutex, NULL);
  pthread_cond_init(&qm->can_get, NULL);
  return qm;
}

void mc_queue_merger_add(mc_queue_merger *qm, mc_queue *q) {
  q->mnext = qm->head;
  qm->head = q;
  q->m = qm;
}

static void free_list(mc_queue *q) {
  for (mc_queue *next = q; next; q = next) {
    next = q->mnext;
    mc_queue_free(q);
  }
}

void mc_queue_merger_empty(mc_queue_merger *qm) {
  free_list(qm->head);
  qm->head = NULL;
}

void mc_queue_merger_free(mc_queue_merger *qm) {
  if (qm) {
    mc_queue_merger_empty(qm);
    free(qm);
  }
}

static mc_queue *rotate(mc_queue *lq) {
  if (!lq) return NULL;

  mc_queue *fq, *nq;
  fq = nq = lq->mnext;

  if (!nq) return lq;

  while (nq->mnext) nq = nq->mnext;

  nq->mnext = lq;
  lq->mnext = NULL;

  return fq;
}

static int better_ent(mc_queue_merger *qm, mc_queue_entry *a, mc_queue_entry *b) {
  if (a == NULL) return 1;
  if (b == NULL) return 0;
  if (a->eof) return 0;
  if (b->eof) return 1;
  return qm->qc(a, b, qm->ctx) > 0;
}

static int merger_get_nb(mc_queue_merger *qm, get_func gf, void *ctx, int *got) {
  unsigned nready = 0, nfull = 0, neof = 0, nqueue = 0;

  mc_queue *nq, *bq = NULL;
  mc_queue_entry *be = NULL;

  *got = 0;

  qm->head = rotate(qm->head);

  for (nq = qm->head; nq; nq = nq->mnext) {

    pthread_mutex_lock(&nq->mutex);
    if (nq->used == nq->max_size) nfull++;
    if (nq->eof) neof++;
    mc_queue_entry *ne = nq->head;
    if (ne) nready++;
    nqueue++;
    pthread_mutex_unlock(&nq->mutex);

    if (!bq || better_ent(qm, be, ne)) {
      bq = nq;
      be = ne;
    }
  }

  /* read from the best queue if all the queues were
   * either ready or at eof or if at least one of the
   * queues is full.
   */

  if (be && (nfull || nready + neof == nqueue)) {
    *got = queue_get(bq, gf, ctx);
    if (!*got) return merger_get_nb(qm, gf, ctx, got);
  }
  else if (neof == nqueue) {
    *got = 1; /* synthetic eof */
  }

  return neof < nqueue;
}

static int merger_get(mc_queue_merger *qm, get_func gf, void *ctx) {
  int got = 0;
  int more = merger_get_nb(qm, gf, ctx, &got);
  if (got) return more;

  pthread_mutex_lock(&qm->mutex);

  for (;;) {
    more = merger_get_nb(qm, gf, ctx, &got);
    if (got) break;
    pthread_cond_wait(&qm->can_get, &qm->mutex);
  }

  pthread_mutex_unlock(&qm->mutex);

  return more;
}

/****************************************************
 *                                                  *
 * Packet wrapper                                   *
 *                                                  *
 ****************************************************/

static void put_packet(mc_queue *q, mc_queue_entry *qe, void *ctx) {
  AVPacket *pkt = (AVPacket *) ctx;
  check_type(q, MC_PACKET);
  av_copy_packet(&qe->d.pkt, pkt);
}

static void get_packet(mc_queue *q, mc_queue_entry *qe, void *ctx) {
  AVPacket *pkt = (AVPacket *) ctx;
  check_type(q, MC_PACKET);
  *pkt = qe->d.pkt;
}


void mc_queue_packet_put(mc_queue *q, AVPacket *pkt) {
  queue_put(q, put_packet, pkt);
}

void mc_queue_multi_packet_put(mc_queue *q, AVPacket *pkt) {
  queue_multi_put(q, put_packet, pkt);
}

int mc_queue_packet_get(mc_queue *q, AVPacket *pkt) {
  return queue_get(q, get_packet, pkt);
}

int mc_queue_merger_packet_get(mc_queue_merger *qm, AVPacket *pkt) {
  return merger_get(qm, get_packet, pkt);
}

/****************************************************
 *                                                  *
 * Frame wrapper                                    *
 *                                                  *
 ****************************************************/

static void put_frame(mc_queue *q, mc_queue_entry *qe, void *ctx) {
  AVFrame *frame = (AVFrame *) ctx;
  check_type(q, MC_FRAME);
  av_frame_ref(&qe->d.frame, frame);
}

static void get_frame(mc_queue *q, mc_queue_entry *qe, void *ctx) {
  AVFrame *frame = (AVFrame *) ctx;
  check_type(q, MC_FRAME);
  *frame = qe->d.frame;
}

void mc_queue_frame_put(mc_queue *q, AVFrame *frame) {
  queue_put(q, put_frame, frame);
}

void mc_queue_multi_frame_put(mc_queue *q, AVFrame *frame) {
  queue_multi_put(q, put_frame, frame);
}

int mc_queue_frame_get(mc_queue *q, AVFrame *frame) {
  return queue_get(q, get_frame, frame);
}

int mc_queue_merger_frame_get(mc_queue_merger *qm, AVFrame *frame) {
  return merger_get(qm, get_frame, frame);
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
