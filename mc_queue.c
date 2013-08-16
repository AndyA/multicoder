/* mc_queue.c */

#include <jd_pretty.h>
#include <pthread.h>

#include "mc_queue.h"

static void *alloc(size_t sz) {
  void *m = malloc(sz);
  if (!m) abort();
  memset(m, 0, sz);
  return m;
}

mc_queue *mc_queue_new(size_t size) {
  mc_queue *q = alloc(sizeof(mc_queue));
  q->max_size = size;
  q->used = 0;
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->can_get, NULL);
  pthread_cond_init(&q->can_put, NULL);
  return q;
}

mc_queue *mc_queue_hook(mc_queue *q, mc_queue *nq) {
  nq->next = q->next;
  q->next = nq;
  return q;
}

static void free_entries(mc_queue_entry *qe) {
  for (mc_queue_entry *next = qe; next; qe = next) {
    next = qe->next;
    av_free_packet(&qe->pkt);
    free(qe);
  }
}

void mc_queue_free(mc_queue *q) {
  if (q) {
    free_entries(q->head);
    free_entries(q->free);
    mc_queue_free(q->next);
    free(q);
  }
}

static mc_queue_entry *get_entry(mc_queue *q) {
  if (q->free) {
    mc_queue_entry *ent = q->free;
    q->free = ent->next;
    return ent;
  }

  return alloc(sizeof(mc_queue_entry));
}

void mc_queue_put(mc_queue *q, AVPacket *pkt) {
  mc_queue_entry *qe;

  pthread_mutex_lock(&q->mutex);

  if (q->tail && q->tail->eof) jd_throw("EOF already seen on queue");

  while (q->used == q->max_size)
    pthread_cond_wait(&q->can_put, &q->mutex);

  qe = get_entry(q);

  if (pkt) {
    av_copy_packet(&qe->pkt, pkt);
    qe->eof = 0;
  }
  else {
    qe->eof = 1;
  }

  qe->next = NULL;
  if (q->tail) q->tail->next = qe;
  else q->head = qe;
  q->tail = qe;
  q->used++;

  pthread_cond_broadcast(&q->can_get);

  if (q->can_get_multi) {
    pthread_mutex_lock(q->mutex_multi);
    pthread_cond_signal(q->can_get_multi);
    pthread_mutex_unlock(q->mutex_multi);
  }

  pthread_mutex_unlock(&q->mutex);
}

void mc_queue_multi_put(mc_queue *q, AVPacket *pkt) {
  for (; q; q = q->next) mc_queue_put(q, pkt);
}

int mc_queue_get(mc_queue *q, AVPacket *pkt) {
  mc_queue_entry *qe;
  int eof;

  pthread_mutex_lock(&q->mutex);
  while (!q->head)
    pthread_cond_wait(&q->can_get, &q->mutex);

  qe = q->head;
  q->head = qe->next;
  if (q->head == NULL) q->tail = NULL;

  if (eof = qe->eof, !eof) * pkt = qe->pkt;

  memset(qe, 0, sizeof(*qe)); /* trample cloned pkt ref */
  qe->next = q->free;
  q->free = qe;
  q->used--;

  pthread_cond_broadcast(&q->can_put);
  pthread_mutex_unlock(&q->mutex);

  return !eof;
}

AVPacket *mc_queue_peek(mc_queue *q) {
  /* probably don't need the lock here */
  pthread_mutex_lock(&q->mutex);
  mc_queue_entry *head = q->head;
  pthread_mutex_unlock(&q->mutex);
  return head ? &head->pkt : NULL;
}

int mc_queue_ready(mc_queue *q) {
  pthread_mutex_lock(&q->mutex);
  int ready = (q->tail && q->tail->eof) || q->head;
  pthread_mutex_unlock(&q->mutex);
  return ready;
}

int mc_queue_full(mc_queue *q) {
  pthread_mutex_lock(&q->mutex);
  int full = q->used == q->max_size;
  pthread_mutex_unlock(&q->mutex);
  return full;
}

int mc_queue_multi_get_nb(mc_queue *qs[], AVPacket *pkt,
                          mc_queue_comparator qc, void *ctx, int *got) {
  unsigned pos = 0;
  unsigned nready = 0, nfull = 0;

  mc_queue *nq, *bq = NULL;
  AVPacket *bp = NULL;

  *got = 0;

  while ((nq = qs[pos++])) {
    if (mc_queue_ready(nq)) nready++;
    if (mc_queue_full(nq)) nfull++;

    AVPacket *np = mc_queue_peek(nq);

    if (bq == NULL) {
      bq = nq;
      bp = np;
    }
    else if (bp == NULL || (np && qc(bp, np, ctx) > 0)) {
      bp = np;
      bq = nq;
    }
  }

  if (bp && (nfull || nready == pos)) {
    *got = 1;
    return mc_queue_get(bq, pkt);
  }

  return 1; /* not eof, just nothing to read */
}

int mc_queue_multi_get(mc_queue *qs[], AVPacket *pkt,
                       mc_queue_comparator qc, void *ctx) {
  int pos, got = 0;
  int eof = mc_queue_multi_get_nb(qs, pkt, qc, ctx, &got);
  if (got) return eof;

  pthread_cond_t can_get_multi = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t mutex_multi = PTHREAD_MUTEX_INITIALIZER;

  for (pos = 0; qs[pos]; pos++) {
    pthread_mutex_lock(&qs[pos]->mutex);
    qs[pos]->can_get_multi = &can_get_multi;
    qs[pos]->mutex_multi = &mutex_multi;
    pthread_mutex_unlock(&qs[pos]->mutex);
  }

  pthread_mutex_lock(&mutex_multi);

  for (;;) {
    eof = mc_queue_multi_get_nb(qs, pkt, qc, ctx, &got);
    if (got) break;
    pthread_cond_wait(&can_get_multi, &mutex_multi);
  }

  pthread_mutex_unlock(&mutex_multi);

  for (pos = 0; qs[pos]; pos++) {
    pthread_mutex_lock(&qs[pos]->mutex);
    qs[pos]->can_get_multi = NULL;
    qs[pos]->mutex_multi = NULL;
    pthread_mutex_unlock(&qs[pos]->mutex);
  }

  return eof;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
