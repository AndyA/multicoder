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

  typedef struct mc_queue {
    size_t used;
    size_t max_size;
    mc_queue_entry *head, *tail, *free;
    struct mc_queue *next;
    pthread_mutex_t mutex;
    pthread_cond_t can_get;
    pthread_cond_t can_put;
    pthread_mutex_t *mutex_multi;
    pthread_cond_t *can_get_multi;
  } mc_queue;

  mc_queue *mc_queue_new(size_t size);
  void mc_queue_free(mc_queue *q);
  mc_queue *mc_queue_hook(mc_queue *q, mc_queue *nq);
  void mc_queue_put(mc_queue *q, AVPacket *pkt);
  void mc_queue_multi_put(mc_queue *q, AVPacket *pkt);
  int mc_queue_get(mc_queue *q, AVPacket *pkt);
  AVPacket *mc_queue_peek(mc_queue *q);
  int mc_queue_ready(mc_queue *q);
  int mc_queue_multi_get_nb(mc_queue *qs[], AVPacket *pkt,
                            mc_queue_comparator qc, void *ctx, int *got);
  int mc_queue_multi_get(mc_queue *qs[], AVPacket *pkt,
                         mc_queue_comparator qc, void *ctx);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
