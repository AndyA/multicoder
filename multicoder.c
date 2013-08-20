/* multicoder.c */

#include <jd_pretty.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "multicoder.h"

typedef struct {
  pthread_t t;
  jd_var cfg;
  AVFormatContext *ic;
  mc_queue_merger *in;
} muxer_context;

static const char *kinds[] = { "audio", "video" };

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

static jd_var *make_key(jd_var *out, const char *kind, jd_var *spec) {
  scope {
    jd_var *hash = jd_nhv(1);
    jd_assign(jd_get_ks(hash, kind, 1), spec);
    jd_to_json(out, hash);
  }
  return out;
}

static jd_var *make_direct(jd_var *out, const char *kind) {
  jd_set_hash(out, 1);
  jd_set_string(jd_get_ks(out, "type", 1), "direct");
  return out;
}

static jd_var *get_queue(jd_var *ctx, const char *kind, jd_var *spec);

static jd_var *get_direct(jd_var *ctx, const char *kind) {
  scope JD_RETURN(get_queue(ctx, kind, make_direct(jd_nv(), kind)));
  return NULL;
}

static void queue_free(void *q) {
  mc_queue_free(q);
}

static jd_var *make_queue(jd_var *ctx, jd_var *out, const char *kind, jd_var *spec) {
  jd_var *type = jd_get_ks(spec, "type", 0);
  if (!type) jd_throw("Missing type in spec");
  const char *tn = jd_bytes(type, NULL);

  if (!strcmp(tn, "direct")) {
    jd_var *src = jd_rv(ctx, "$.sources.%s", kind);
    if (!src) jd_throw("Can't find %s source", kind);
    jd_assign(out, src);
  }
  else {
    jd_throw("Unhandled %s stream type: %V", kind, type);
  }

  return out;
}

static jd_var *get_queue(jd_var *ctx, const char *kind, jd_var *spec) {
  jd_var *slot = NULL;
  scope {
    jd_var *inputs = jd_get_ks(ctx, "inputs", 0);
    jd_var *key = make_key(jd_nv(), kind, spec);
    slot = jd_get_key(inputs, key, 1);
    if (slot->type == VOID)
      make_queue(ctx, slot, kind, spec);
  }
  return slot;
}

static void setup_stream(jd_var *ctx, jd_var *stm, const char *kind, jd_var *spec) {
  mc_debug("Configuring %V %s", jd_get_ks(stm, "name", 0), kind);
  mc_queue *head = jd_ptr(get_queue(ctx, kind, spec));
  mc_queue *q = mc_queue_new(200);
  mc_queue_hook(head, q);
  jd_set_object(jd_get_ks(spec, "source", 1), q, queue_free);
}

static void with_streams(
  jd_var *ctx, void (*cb)(jd_var *ctx, jd_var *stm, const char *kind, jd_var *spec)) {
  scope {
    jd_var *streams = jd_get_ks(ctx, "streams", 0);
    for (unsigned i = 0; i < jd_count(streams); i++) {
      jd_var *stm = jd_get_idx(streams, i);
      for (unsigned k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
        const char *kind = kinds[k];
        jd_var *spec = jd_get_ks(stm, kind, 0);
        if (spec) cb(ctx, stm, kind, spec);
      }
    }
  }
}

static void setup(jd_var *ctx) {
  with_streams(ctx, setup_stream);
}

static void *muxer(void *ctx) {
  muxer_context *mcx = ctx;
  scope {
    jd_var *name = jd_sprintf(jd_nv(), "mux.%V", jd_get_ks(&mcx->cfg, "name", 0));
    mc_log_set_thread(jd_bytes(name, NULL));
    mc_mux_hls(mcx->ic, &mcx->cfg, mcx->in);
  }
  return NULL;
}

static void free_muxer_context(void *ctx) {
  muxer_context *mcx = ctx;
  mc_queue_merger_free(mcx->in);
  jd_release(&mcx->cfg);
}

static void join_workers(jd_var *ctx) {
  jd_var *workers = jd_get_ks(ctx, "workers", 0);

  mc_debug("Waiting for workers to terminate");
  for (unsigned i = 0; i < jd_count(workers); i++) {
    muxer_context *mcx = jd_ptr(jd_get_idx(workers, i));
    pthread_join(mcx->t, NULL);
  }
}

static void startup_workers(jd_var *ctx) {
  scope {
    jd_var *streams = jd_get_ks(ctx, "streams", 0);
    for (unsigned i = 0; i < jd_count(streams); i++) {
      jd_var *stm = jd_get_idx(streams, i);
      muxer_context *mcx = mc_alloc(sizeof(*mcx));
      mcx->in = mc_queue_merger_new(dts_compare, NULL);
      mcx->ic = jd_ptr(jd_get_ks(ctx, "ic", 0));
      jd_clone(&mcx->cfg, stm, 1);
      for (unsigned k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
        const char *kind = kinds[k];
        jd_var *spec = jd_get_ks(stm, kind, 0);
        if (!spec) continue;
        mc_queue *q = jd_ptr(jd_get_ks(spec, "source", 0));
        mc_queue_merger_add(mcx->in, q);
      }
      jd_set_object(jd_push(jd_get_ks(ctx, "workers", 0), 1), mcx, free_muxer_context);
      mc_debug("Starting worker for %V", jd_get_ks(stm, "name", 0));
      pthread_create(&mcx->t, NULL, muxer, mcx);
    }
  }
}

static jd_var *merge_default(jd_var *out, jd_var *cfg) {
  scope {
    jd_var *dflt = jd_get_ks(cfg, "default", 0);
    jd_var *streams = jd_get_ks(cfg, "streams", 0);

    size_t count = jd_count(streams);
    jd_set_array(out, count);
    for (unsigned i = 0; i < count; i++) {
      jd_var *stm = jd_get_idx(streams, i);
      if (mc_model_get_int(stm, 1, "$.enabled"))
        mc_hash_merge(jd_push(out, 1), dflt, stm);
    }
  }
  return out;
}

static jd_var *build_context(jd_var *ctx, jd_var *cfg,
                             mc_queue *aq, mc_queue *vq) {
  scope {
    jd_set_hash(ctx, 10);
    jd_assign(jd_get_ks(ctx, "config", 1), cfg);
    merge_default(jd_get_ks(ctx, "streams", 1), cfg);
    jd_set_hash(jd_get_ks(ctx, "inputs", 1), 10);
    jd_set_array(jd_get_ks(ctx, "workers", 1), 10);
    jd_var *sources = jd_set_hash(jd_get_ks(ctx, "sources", 1), 10);
    jd_set_object(jd_get_ks(sources, "audio", 1), aq, NULL);
    jd_set_object(jd_get_ks(sources, "video", 1), vq, NULL);
  }
  return ctx;
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

    mc_log_set_thread("main");
    mc_info("Starting multicoder");

    mc_queue *aq = mc_queue_new(0);
    mc_queue *vq = mc_queue_new(0);

    jd_var *cfg = mc_model_load_file(jd_nv(), argv[1]);
    jd_var *ctx = build_context(jd_nv(), cfg, aq, vq);
    setup(ctx);

    mc_log_level = mc_log_decode_level(
      mc_model_get_str(cfg, "INFO", "$.global.log_level"));

    if (avformat_open_input(&ic, argv[2], NULL, NULL) < 0)
      jd_throw("Can't open %s", argv[2]);

    if (avformat_find_stream_info(ic, NULL) < 0)
      jd_throw("Can't read stream info");

    /* add ic to context */
    jd_set_object(jd_get_ks(ctx, "ic", 1), ic, NULL);

    mc_debug("Active context:\n%lJ", ctx);

    startup_workers(ctx);

    mc_demux(ic, NULL, aq, vq);

    mc_queue_packet_put(aq, NULL);
    mc_queue_packet_put(vq, NULL);

    join_workers(ctx);

    mc_queue_free(aq);
    mc_queue_free(vq);

    avformat_close_input(&ic);

    av_free(ic);

    avformat_network_deinit();
  }

  return 0;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
