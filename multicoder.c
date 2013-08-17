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
  AVFormatContext *fcx;
  mc_queue_merger *qm;
  pthread_t t;
} muxer;

static void *hls_muxer(void *ctx) {
  muxer *c = ctx;
  mc_mux_hls(c->fcx, c->cfg, c->qm);
  return NULL;
}

typedef struct {
  jd_var *cfg;
  AVFormatContext *fcx;
  mc_queue *in, *out;
  pthread_t t;
} decoder;

static void *h264_decoder(void *ctx) {
  decoder *c = ctx;
  mc_h264_decode(c->fcx, c->cfg, c->in, c->out);
  return NULL;
}

static int dts_compare(mc_queue_entry *a, mc_queue_entry *b, void *ctx) {
  (void) ctx;
  return a->d.pkt.dts < b->d.pkt.dts ? -1 : a->d.pkt.dts > b->d.pkt.dts ? 1 : 0;
}

int main(int argc, char *argv[]) {
  srand((unsigned) time(NULL));
  scope {
    AVFormatContext *fcx = NULL;

    av_register_all();
    avcodec_register_all();
    avformat_network_init();

    if (argc != 3) jd_throw("Syntax: multicoder <config.json> <input>");

    jd_var *cfg = mc_model_load_file(jd_nv(), argv[1]);

    if (avformat_open_input(&fcx, argv[2], NULL, NULL) < 0)
      jd_throw("Can't open %s", argv[2]);

    mc_info("Starting multicoder");

    muxer hls;

    hls.fcx = fcx;
    hls.qm = mc_queue_merger_new(dts_compare, NULL);

    mc_queue *haq = mc_queue_new(100);
    mc_queue *hvq = mc_queue_new(100);

    if (1) {
      hls.cfg = mc_model_get(cfg, NULL, "$.tasks.0.streams.1");
      mc_debug("hls.cfg = %lJ", hls.cfg);
      mc_queue_merger_add(hls.qm, haq);
      mc_queue_merger_add(hls.qm, hvq);

      pthread_create(&hls.t, NULL, hls_muxer, &hls);

      mc_demux(fcx, NULL, haq, hvq);
      mc_queue_packet_put(haq, NULL);
      mc_queue_packet_put(hvq, NULL);

      pthread_join(hls.t, NULL);

      mc_queue_merger_free(hls.qm);
    }
    else {
      decoder dec;

      mc_queue *dvq = mc_queue_new(100);

      dec.in = hvq;
      dec.out = dvq;

      mc_queue_merger_add(hls.qm, haq);
      mc_queue_merger_add(hls.qm, dvq);

      pthread_create(&hls.t, NULL, hls_muxer, &hls);
      pthread_create(&dec.t, NULL, h264_decoder, &dec);

      mc_demux(fcx, NULL, haq, hvq);
      mc_queue_packet_put(haq, NULL);
      mc_queue_packet_put(hvq, NULL);

      pthread_join(hls.t, NULL);
      pthread_join(dec.t, NULL);

      mc_queue_merger_free(hls.qm);
      mc_queue_free(dvq);
    }
    avformat_close_input(&fcx);

    mc_queue_free(haq);
    mc_queue_free(hvq);
  }

  return 0;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
