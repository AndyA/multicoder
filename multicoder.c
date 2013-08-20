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
  AVFormatContext *ic;
  mc_queue_merger *in;
  mc_queue *out;
  pthread_t t;
} block;

typedef struct stream {
  struct stream *next;
  block dec, mux;
} stream;

static void *hls_muxer(void *ctx) {
  block *c = ctx;
  mc_mux_hls(c->ic, c->cfg, c->in);
  return NULL;
}

static void *h264_decoder(void *ctx) {
  block *c = ctx;
  mc_h264_decode(c->ic, c->cfg, c->in, c->out);
  return NULL;
}

static int dts_compare(mc_queue_entry *a, mc_queue_entry *b, void *ctx) {
  (void) ctx;
  return a->d.pkt.dts < b->d.pkt.dts ? -1 : a->d.pkt.dts > b->d.pkt.dts ? 1 : 0;
}

static int nop_compare(mc_queue_entry *a, mc_queue_entry *b, void *ctx) {
  (void) a;
  (void) b;
  (void) ctx;
  return 0;
}

int main(int argc, char *argv[]) {
  srand((unsigned) time(NULL));
  scope {
    AVFormatContext *ic = NULL;

    av_log_set_callback(mc_log_avutil);
    av_register_all();
    avcodec_register_all();
    avformat_network_init();

    if (argc != 3) jd_throw("Syntax: multicoder <config.json> <input>");

    mc_info("Starting multicoder");

    jd_var *cfg = mc_model_load_file(jd_nv(), argv[1]);

    mc_log_level = mc_log_decode_level(
      mc_model_get_str(cfg, "INFO", "$.global.log_level"));

    if (avformat_open_input(&ic, argv[2], NULL, NULL) < 0)
      jd_throw("Can't open %s", argv[2]);

    if (avformat_find_stream_info(ic, NULL) < 0)
      jd_throw("Can't read stream info");

    block hls;

    hls.ic = ic;
    hls.in = mc_queue_merger_new(dts_compare, NULL);

    mc_queue *haq = mc_queue_new(100);
    mc_queue *hvq = mc_queue_new(100);


    hls.cfg = mc_model_get(cfg, NULL, "$.streams.1");
    mc_debug("hls.cfg = %lJ", hls.cfg);
    mc_queue_merger_add(hls.in, haq);
    mc_queue_merger_add(hls.in, hvq);

    pthread_create(&hls.t, NULL, hls_muxer, &hls);

    mc_demux(ic, NULL, haq, hvq);
    mc_queue_packet_put(haq, NULL);
    mc_queue_packet_put(hvq, NULL);

    pthread_join(hls.t, NULL);

    mc_queue_merger_free(hls.in);
    avformat_close_input(&ic);

    av_free(ic);

    avformat_network_deinit();
  }

  return 0;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
