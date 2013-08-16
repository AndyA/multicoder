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
  mc_queue *q = alloc(sizeof(*q));
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
  for (mc_queue *next = q; next; q = next) {
    free_entries(q->head);
    free_entries(q->free);
    next = q->next;
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

  if (q->m)
    pthread_mutex_lock(&q->m->mutex);

  pthread_mutex_lock(&q->mutex);

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

  pthread_mutex_unlock(&q->mutex);

  if (q->m) {
    pthread_cond_signal(&q->m->can_get);
    pthread_mutex_unlock(&q->m->mutex);
  }
}

void mc_queue_multi_put(mc_queue *q, AVPacket *pkt) {
  for (; q; q = q->next) mc_queue_put(q, pkt);
}

int mc_queue_get(mc_queue *q, AVPacket *pkt) {
  mc_queue_entry *qe;

  pthread_mutex_lock(&q->mutex);
  while (!q->head)
    pthread_cond_wait(&q->can_get, &q->mutex);

  qe = q->head;
  q->head = qe->next;
  if (q->head == NULL) q->tail = NULL;

  if (qe->eof) q->eof = 1;
  else *pkt = qe->pkt;

  memset(qe, 0, sizeof(*qe)); /* trample cloned pkt ref */
  qe->next = q->free;
  q->free = qe;
  q->used--;

  pthread_cond_broadcast(&q->can_put);
  pthread_mutex_unlock(&q->mutex);

  return !q->eof;
}

AVPacket *mc_queue_peek(mc_queue *q) {
  /* probably don't need the lock here */
  pthread_mutex_lock(&q->mutex);
  mc_queue_entry *head = q->head;
  pthread_mutex_unlock(&q->mutex);
  return head ? &head->pkt : NULL;
}

mc_queue_merger *mc_queue_merger_new(mc_queue_comparator qc, void *ctx) {
  mc_queue_merger *qm = alloc(sizeof(*qm));
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

void mc_queue_merger_empty(mc_queue_merger *qm) {
  for (mc_queue *q = qm->head; q; q = q->mnext) q->m = NULL;
  qm->head = NULL;
}

void mc_queue_merger_free(mc_queue_merger *qm) {
  mc_queue_merger_empty(qm);
  free(qm);
}

int mc_queue_merger_get_nb(mc_queue_merger *qm, AVPacket *pkt, int *got) {
  unsigned nready = 0, nfull = 0, neof = 0, nqueue = 0;

  mc_queue *nq, *bq = NULL;
  AVPacket *bp = NULL;

  *got = 0;

  for (nq = qm->head; nq; nq = nq->mnext) {
    mc_queue_entry *head;

    pthread_mutex_lock(&nq->mutex);
    if (nq->used == nq->max_size) nfull++;
    if (nq->eof) neof++;
    head = nq->head;
    pthread_mutex_unlock(&nq->mutex);
    nqueue++;
    AVPacket *np = head ? &head->pkt : NULL;

    if (np) nready++;

    if (bq == NULL) {
      bq = nq;
      bp = np;
    }
    else if (bp == NULL || (np && qm->qc(bp, np, qm->ctx) > 0)) {
      bp = np;
      bq = nq;
    }
  }

  if (bp && (nfull || nready + neof == nqueue))
    *got = mc_queue_get(bq, pkt);
  else if (neof == nqueue)
    *got = 1;

  return neof < nqueue;
}

int mc_queue_merger_get(mc_queue_merger *qm, AVPacket *pkt) {
  int got = 0;
  int more = mc_queue_merger_get_nb(qm, pkt, &got);
  if (got) return more;

  pthread_mutex_lock(&qm->mutex);

  for (;;) {
    more = mc_queue_merger_get_nb(qm, pkt, &got);
    if (got) break;
    pthread_cond_wait(&qm->can_get, &qm->mutex);
  }

  pthread_mutex_unlock(&qm->mutex);

  return more;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
