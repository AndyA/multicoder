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
  mc_queue *q;
  pthread_t t;
} muxer;

static void *hls_muxer(void *ctx) {
  muxer *mctx = ctx;
  mc_mux_hls(mctx->cfg, mctx->q);
  return NULL;
}

int main(int argc, char *argv[]) {
  AVFormatContext *fcx = NULL;
  muxer hls;

  hls.q = mc_queue_new(100);

  mc_info("Starting multicoder");

  av_register_all();
  avcodec_register_all();
  avformat_network_init();

  if (argc != 2) jd_throw("Syntax: multicoder <input>");
  if (avformat_open_input(&fcx, argv[1], NULL, NULL) < 0)
    jd_throw("Can't open %s", argv[1]);

  pthread_create(&hls.t, NULL, hls_muxer, &hls);

  mc_demux(fcx, hls.q, hls.q);
  mc_queue_put(hls.q, NULL);

  pthread_join(hls.t, NULL);

  avformat_close_input(&fcx);
  mc_queue_free(hls.q);

  return 0;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
