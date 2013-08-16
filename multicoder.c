/* multicoder.c */

#include <jd_pretty.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "multicoder.h"

typedef struct {
  jd_var *cfg;
  mc_queue_merger *qm;
  pthread_t t;
} muxer;

static void *hls_muxer(void *ctx) {
  muxer *c = ctx;
  mc_mux_hls(c->cfg, c->qm);
  return NULL;
}

static int dts_compare(AVPacket *a, AVPacket *b, void *ctx) {
  return a->dts < b->dts ? -1 : a->dts > b->dts ? 1 : 0;
}

static int pts_compare(AVPacket *a, AVPacket *b, void *ctx) {
  return a->pts < b->pts ? -1 : a->pts > b->pts ? 1 : 0;
}

int main(int argc, char *argv[]) {
  AVFormatContext *fcx = NULL;
  muxer hls;

  mc_queue *aq = mc_queue_new(100);
  mc_queue *vq = mc_queue_new(100);

  hls.qm = mc_queue_merger_new(dts_compare, NULL);
  mc_queue_merger_add(hls.qm, aq);
  mc_queue_merger_add(hls.qm, vq);

  mc_info("Starting multicoder");

  av_register_all();
  avcodec_register_all();
  avformat_network_init();

  if (argc != 2) jd_throw("Syntax: multicoder <input>");
  if (avformat_open_input(&fcx, argv[1], NULL, NULL) < 0)
    jd_throw("Can't open %s", argv[1]);

  pthread_create(&hls.t, NULL, hls_muxer, &hls);

  mc_demux(fcx, aq, vq);
  mc_queue_put(aq, NULL);
  mc_queue_put(vq, NULL);

  pthread_join(hls.t, NULL);

  avformat_close_input(&fcx);
  mc_queue_merger_free(hls.qm);
  mc_queue_free(aq);
  mc_queue_free(vq);

  return 0;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
