/* mc_queue.h */

#ifndef MC_QUEUE_H_
#define MC_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

  typedef enum {
    MC_UNKNOWN,
    MC_PACKET,
    MC_FRAME
  }
  mc_queue_type;

  typedef struct mc_queue_entry {
    struct mc_queue_entry *next;
    union {
      AVPacket pkt;
      AVFrame frame;
    } d;
    AVPacket pkt;
    int eof;
  } mc_queue_entry;

  typedef int (*mc_queue_packet_comparator)(AVPacket *a, AVPacket *b, void *ctx);

  typedef struct mc_queue_merger mc_queue_merger;

  typedef struct mc_queue {
    struct mc_queue *pnext, *mnext;

    mc_queue_type t;
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

    mc_queue_packet_comparator qc;
    void *ctx;

    pthread_mutex_t mutex;
    pthread_cond_t can_get;
  };

  mc_queue *mc_queue_new(size_t size);
  void mc_queue_free(mc_queue *q);
  mc_queue *mc_queue_hook(mc_queue *q, mc_queue *nq);
  void mc_queue_packet_put(mc_queue *q, AVPacket *pkt);
  void mc_queue_multi_packet_put(mc_queue *q, AVPacket *pkt);
  int mc_queue_packet_get(mc_queue *q, AVPacket *pkt);
  AVPacket *mc_queue_packet_peek(mc_queue *q);

  mc_queue_merger *mc_queue_merger_new(mc_queue_packet_comparator qc, void *ctx);
  void mc_queue_merger_add(mc_queue_merger *qm, mc_queue *q);
  void mc_queue_merger_empty(mc_queue_merger *qm);
  void mc_queue_merger_free(mc_queue_merger *qm);
  int mc_queue_merger_packet_get_nb(mc_queue_merger *qm, AVPacket *pkt, int *got);
  int mc_queue_merger_packet_get(mc_queue_merger *qm, AVPacket *pkt);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
