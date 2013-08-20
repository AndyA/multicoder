/* queue.c */

#include <pthread.h>
#include <libavformat/avformat.h>

#include "framework.h"
#include "tap.h"

#include "jd_pretty.h"

#include "mc_queue.h"

static int drain(mc_queue *q) {
  AVPacket pkt;
  int count = 0;
  while (mc_queue_peek(q)) {
    mc_queue_packet_get(q, &pkt);
    count++;
  }
  return count;
}

static void test_non_full(void) {
  AVPacket pkt;
  mc_queue *q = mc_queue_new(10);

  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;

  for (unsigned t = 0; t < 5; t++) {
    if (!ok(q->used == 0, "try %d: q->used == 0 (before)", t))
      diag("q->used = %d", q->used);
    for (unsigned i = 0; i < 5; i++)
      mc_queue_packet_put(q, &pkt);

    if (!ok(q->used == 5, "try %d: q->used == 5 (during)", t))
      diag("q->used = %d", q->used);

    int count = drain(q);

    if (!ok(count == 5, "try %d: five packets queued, retrieved", t))
      diag("count = %d", count);

    if (!ok(q->used == 0, "try %d: q->used == 0 (after)", t))
      diag("q->used = %d", q->used);
  }

  mc_queue_free(q);
}

static void test_multi(void) {
  AVPacket pkt;
  mc_queue *head = mc_queue_new(0);
  mc_queue *q1 = mc_queue_new(10);
  mc_queue *q2 = mc_queue_new(10);

  mc_queue_hook(head, q1);
  mc_queue_hook(head, q2);

  for (unsigned i = 0; i < 5; i++)
    mc_queue_multi_packet_put(head, &pkt);

  ok(!mc_queue_peek(head), "nothing in head");

  ok(drain(q1) == 5, "5 in q1");
  ok(drain(q2) == 5, "5 in q2");

  mc_queue_free(head);
  mc_queue_free(q1);
  mc_queue_free(q2);
}

void test_main(void) {
  scope {
    test_non_full();
    test_multi();
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
