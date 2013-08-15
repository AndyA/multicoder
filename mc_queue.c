/* mc_queue.c */

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
  pthread_cond_init(&q->can_read, NULL);
  pthread_cond_init(&q->can_write, NULL);
  return q;
}

void mc_queue_hook(mc_queue *q, mc_queue *nq) {
  q->next = nq;
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

  while (q->used == q->max_size)
    pthread_cond_wait(&q->can_write, &q->mutex);

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

  pthread_mutex_unlock(&q->mutex);
  pthread_cond_broadcast(&q->can_read);

  if (q->next) mc_queue_put(q->next, pkt);
}

int mc_queue_get(mc_queue *q, AVPacket *pkt) {
  mc_queue_entry *qe;
  int eof;

  pthread_mutex_lock(&q->mutex);
  while (!q->head)
    pthread_cond_wait(&q->can_read, &q->mutex);

  qe = q->head;
  q->head = qe->next;
  if (q->head == NULL) q->tail = NULL;

  if (eof = qe->eof, !eof) * pkt = qe->pkt;

  memset(qe, 0, sizeof(*qe)); /* trample cloned pkt ref */
  qe->next = q->free;
  q->free = qe;
  q->used--;

  pthread_mutex_unlock(&q->mutex);
  pthread_cond_broadcast(&q->can_write);

  return !eof;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
