/* mc_queue.h */

#ifndef MC_QUEUE_H_
#define MC_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <libavformat/avformat.h>

  typedef struct mc_queue_entry {
    struct mc_queue_entry *next;
    AVPacket pkt;
    int eof;
  } mc_queue_entry;

  typedef int (*mc_queue_comparator)(AVPacket *a, AVPacket *b, void *ctx);

  typedef struct mc_queue_merger mc_queue_merger;

  typedef struct mc_queue {
    struct mc_queue *next, *mnext;

    size_t used;
    size_t max_size;
    int eof;

    mc_queue_entry *head, *tail, *free;
    mc_queue_merger *m;

    pthread_mutex_t mutex;
    pthread_cond_t can_get;
    pthread_cond_t can_put;
  } mc_queue;

  struct mc_queue_merger {
    mc_queue *head;

    mc_queue_comparator qc;
    void *ctx;

    pthread_mutex_t mutex;
    pthread_cond_t can_get;
  };

  mc_queue *mc_queue_new(size_t size);
  void mc_queue_free(mc_queue *q);
  mc_queue *mc_queue_hook(mc_queue *q, mc_queue *nq);
  void mc_queue_put(mc_queue *q, AVPacket *pkt);
  void mc_queue_multi_put(mc_queue *q, AVPacket *pkt);
  int mc_queue_get(mc_queue *q, AVPacket *pkt);
  AVPacket *mc_queue_peek(mc_queue *q);

  mc_queue_merger *mc_queue_merger_new(mc_queue_comparator qc, void *ctx);
  void mc_queue_merger_add(mc_queue_merger *qm, mc_queue *q);
  void mc_queue_merger_empty(mc_queue_merger *qm);
  void mc_queue_merger_free(mc_queue_merger *qm);
  int mc_queue_merger_get_nb(mc_queue_merger *qm, AVPacket *pkt, int *got);
  int mc_queue_merger_get(mc_queue_merger *qm, AVPacket *pkt);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
