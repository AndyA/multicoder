/* mc_mux_hls.c */

#include <jd_pretty.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "hls.h"

#include "multicoder.h"

#define RETIRE 4

typedef struct {
  jd_var *cfg;
  jd_var *m3u8;
  mc_segname *segn;
  mc_segname *pln;
  int open;
  jd_int min_time;
  jd_var *retire_queue;
} context;

static AVStream *add_output(AVFormatContext *oc, AVStream *is) {

  AVStream *os = avformat_new_stream(oc, 0);
  if (!is) jd_throw("Can't allocate stream");

  AVCodecContext *icc = is->codec;
  AVCodecContext *occ = os->codec;

  occ->codec_id = icc->codec_id;
  occ->codec_type = icc->codec_type;
  occ->codec_tag = icc->codec_tag;
  occ->bit_rate = icc->bit_rate;
  /* not sure about extradata */
  occ->extradata = icc->extradata;
  occ->extradata_size = icc->extradata_size;

  /* hmm */
  if (av_q2d(icc->time_base) * icc->ticks_per_frame > av_q2d(is->time_base)
      && av_q2d(is->time_base) < 1.0 / 1000) {
    occ->time_base = icc->time_base;
    occ->time_base.num *= icc->ticks_per_frame;
  }
  else {
    occ->time_base = is->time_base;
  }

  switch (icc->codec_type) {
  case AVMEDIA_TYPE_AUDIO:
    occ->channel_layout = icc->channel_layout;
    occ->sample_rate = icc->sample_rate;
    occ->sample_fmt = icc->sample_fmt;
    occ->channels = icc->channels;
    occ->frame_size = icc->frame_size;
    occ->block_align = icc->block_align;
    if ((icc->block_align == 1 && icc->codec_id == CODEC_ID_MP3)
        || icc->codec_id == CODEC_ID_AC3)
      occ->block_align = 0;
    break;

  case AVMEDIA_TYPE_VIDEO:
    occ->pix_fmt = icc->pix_fmt;
    occ->width = icc->width;
    occ->height = icc->height;
    occ->has_b_frames = icc->has_b_frames;

    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
      occ->flags |= CODEC_FLAG_GLOBAL_HEADER;
    break;

  default:
    break;
  }

  return os;
}

static void seg_close(context *ctx, AVFormatContext *oc) {
  if (ctx->open) {
    av_write_trailer(oc);
    avio_flush(oc->pb);
    avio_close(oc->pb);
    ctx->open = 0;

    mc_segname_rename(ctx->segn);
    mc_segname_inc(ctx->segn);
  }
}

static void seg_open(context *ctx, AVFormatContext *oc) {
  if (!ctx->open) {
    const char *name = mc_segname_name(ctx->segn);
    const char *temp = mc_segname_temp(ctx->segn);

    mc_debug("writing %s (as %s)", name, temp);
    mc_mkfilepath(temp, 0777);

    if (avio_open(&oc->pb, temp, AVIO_FLAG_WRITE) < 0)
      jd_throw("Can't write %s: %m", temp);
    if (avformat_write_header(oc, NULL))
      jd_throw("Can't write header");
    ctx->open = 1;
  }
}

static const char *cfg_need(jd_var *cfg, const char *path) {
  jd_var *v = jd_rv(cfg, path);
  if (!v) jd_throw("Missing %s", path);
  return jd_bytes(v, NULL);
}

static jd_var *m3u8_init(context *ctx) {
  hls_m3u8_init(ctx->m3u8);
  char *name = mc_segname_name(ctx->pln);
  if (mc_is_file(name)) {
    mc_info("Attempting to load existing %s", name);
    hls_m3u8_load(ctx->m3u8, name);
    /* TODO conditional? */
    hls_m3u8_push_discontinuity(ctx->m3u8);
  }

  jd_var *meta = hls_m3u8_meta(ctx->m3u8);
  jd_set_int(jd_get_ks(meta, "EXT-X-TARGETDURATION", 1),
             mc_model_get_int(ctx->cfg, 8, "$.output.gop"));
  jd_set_string(jd_get_ks(meta, "EXT-X-PLAYLIST-TYPE", 1), "EVENT");
  jd_set_int(jd_get_ks(meta, "EXT-X-VERSION", 1), 3);

  hls_m3u8_set_closed(ctx->m3u8, 0);

  return ctx->m3u8;
}

static jd_var *make_segment(jd_var *out,
                            const char *uri,
                            double duration,
                            const char *title) {
  jd_set_hash(out, 4);
  jd_set_string(jd_lv(out, "$.uri"), uri);
  jd_set_real(jd_lv(out, "$.EXTINF.duration"), duration);
  jd_set_string(jd_lv(out, "$.EXTINF.title"), title ? title : "");
  return out;
}

static void cleanup(context *ctx) {
  jd_var *rq = ctx->retire_queue;
  if (jd_count(rq) < RETIRE) return;

  scope {
    jd_var *segs = jd_shift(rq, 1, jd_nv());
    size_t nsegs = jd_count(segs);
    for (unsigned i = 0; i < nsegs; i++) {
      jd_var *seg = jd_get_idx(segs, i);
      if (seg->type == HASH) {
        jd_var *uri = jd_get_ks(seg, "uri", 0);
        if (uri) {
          char *fn = mc_segname_prefix(ctx->segn, uri);
          mc_debug("Purging %s", fn);
          if (unlink(fn)) mc_warning("Failed to delete %s: %m", fn);
          free(fn);
        }
      }
    }
  }
}

static void m3u8_push_segment(context *ctx,
                              const char *uri,
                              double duration,
                              const char *title) {
  scope {
    jd_var *seg = make_segment(jd_nv(), uri, duration, title);
    hls_m3u8_push_segment(ctx->m3u8, seg);
    hls_m3u8_expire(ctx->m3u8, ctx->min_time);
    jd_assign(jd_push(ctx->retire_queue, 1), hls_m3u8_retired(ctx->m3u8));
    cleanup(ctx);
    hls_m3u8_save(ctx->m3u8, mc_segname_temp(ctx->pln));
    mc_segname_rename(ctx->pln);
    mc_debug("Updated %s", mc_segname_name(ctx->pln));
    mc_segname_inc(ctx->pln);
  }
}

static void parse_previous(context *ctx) {
  jd_var *last = hls_m3u8_last_seg(ctx->m3u8);
  if (last) {
    jd_var *uri = jd_get_ks(last, "uri", 0);
    if (uri && mc_segname_parse(ctx->segn, jd_bytes(uri, NULL)))
      mc_segname_inc(ctx->segn);
  }
}

static void push_segment(context *ctx,
                         AVFormatContext *oc,
                         double duration) {
  char *name = mc_strdup(mc_segname_uri(ctx->segn));
  seg_close(ctx, oc);
  m3u8_push_segment(ctx, name, duration, "");
  free(name);
}

void mc_mux_hls(AVFormatContext *ic, jd_var *cfg, mc_queue_merger *qm) {
  scope {
    AVFormatContext *oc;
    AVPacket pkt;
    AVStream *vs = NULL, *as = NULL;
    context ctx;
    int vi = -1;

    const char *prefix = cfg_need(cfg, "$.output.prefix");

    double min_gop = mc_model_get_real(cfg, 4, "$.output.min_gop");
    int gop = mc_model_get_int(cfg, 8, "$.output.gop");
    double last_duration = gop;
    double gop_time = NAN;

    ctx.cfg = cfg;
    ctx.open = 0;
    ctx.segn = mc_segname_new_prefixed(cfg_need(cfg, "$.output.segment"), prefix);
    ctx.pln = mc_segname_new_prefixed(cfg_need(cfg, "$.output.playlist"), prefix);
    ctx.retire_queue = jd_nav(RETIRE);
    ctx.min_time = mc_model_get_int(cfg, 3600, "$.output.min_time");
    ctx.m3u8 = jd_nv();

    m3u8_init(&ctx);
    parse_previous(&ctx);

    mc_info("Next segment is %s", mc_segname_name(ctx.segn));

    if (oc = avformat_alloc_context(), !oc)
      jd_throw("Can't allocate output context");

    if (oc->oformat = av_guess_format("mpegts", NULL, NULL), !oc->oformat)
      jd_throw("Can't find mpegts multiplexer");

    for (unsigned i = 0; i < ic->nb_streams; i++) {
      AVStream *is = ic->streams[i];
      is->discard = AVDISCARD_NONE;

      if (!vs && is->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
        vs = add_output(oc, is);
        vi = i;
        continue;
      }

      if (!as && is->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
        as = add_output(oc, is);
        continue;
      }

      is->discard = AVDISCARD_ALL;
    }

    ic->flags |= AVFMT_FLAG_IGNDTS;

    av_init_packet(&pkt);

    while (mc_queue_merger_packet_get(qm, &pkt)) {
      mc_debug("HLS got %d (flags=%08x, pts=%llu, dts=%llu, duration=%d)",
               pkt.stream_index, pkt.flags,
               (unsigned long long) pkt.pts,
               (unsigned long long) pkt.dts,
               pkt.duration);

      if ((pkt.stream_index == vi && (pkt.flags & AV_PKT_FLAG_KEY)) || vi == -1) {
        double st = pkt.pts * av_q2d((pkt.stream_index == vi ? vs : as)->time_base);
        if (isnan(gop_time)) {
          gop_time = st;
        }
        else if (st - gop_time >= min_gop) {
          last_duration = st - gop_time;
          push_segment(&ctx, oc, last_duration);
          gop_time = st;
        }
      }

      seg_open(&ctx, oc);

      if (av_interleaved_write_frame(oc, &pkt))
        jd_throw("Can't write frame");

      av_free_packet(&pkt);
    }

    push_segment(&ctx, oc, last_duration);

    for (unsigned i = 0; i < oc->nb_streams; i++) {
      av_freep(&oc->streams[i]->codec);
      av_freep(&oc->streams[i]);
    }

    av_free(oc);

    mc_segname_free(ctx.segn);
    mc_segname_free(ctx.pln);

    mc_debug("HLS EOF");
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
