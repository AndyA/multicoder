/* mc_mux_hls.c */

#include <jd_pretty.h>
#include <math.h>
#include <stdio.h>

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

typedef struct {
  mc_segname *sn;
  int open;
} seg_file;

static void seg_close(seg_file *sf, AVFormatContext *oc) {
  if (sf->open) {
    av_write_trailer(oc);
    avio_flush(oc->pb);
    avio_close(oc->pb);
    sf->open = 0;

    mc_segname_rename(sf->sn);
    mc_segname_inc(sf->sn);
  }
}

static void seg_open(seg_file *sf, AVFormatContext *oc) {
  if (!sf->open) {
    const char *name = mc_segname_name(sf->sn);
    const char *temp = mc_segname_temp(sf->sn);

    mc_debug("writing %s (as %s)", name, temp);
    mc_mkfilepath(temp, 0777);

    if (avio_open(&oc->pb, temp, AVIO_FLAG_WRITE) < 0)
      jd_throw("Can't write %s: %m", temp);
    if (avformat_write_header(oc, NULL))
      jd_throw("Can't write header");
    sf->open = 1;
  }
}

static const char *cfg_need(jd_var *cfg, const char *path) {
  jd_var *v = jd_rv(cfg, path);
  if (!v) jd_throw("Missing %s", path);
  return jd_bytes(v, NULL);
}

static jd_var *m3u8_init(jd_var *m3u8, mc_segname *sn) {
  hls_m3u8_init(m3u8);
  char *name = mc_segname_name(sn);
  if (mc_is_file(name)) {
    mc_info("Attempting to load existing %s", name);
    hls_m3u8_load(m3u8, name);
    /* TODO conditional? */
    hls_m3u8_push_discontinuity(m3u8);
  }
  return m3u8;
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

static jd_var *m3u8_push_segment(jd_var *m3u8,
                                 jd_var *cfg,
                                 mc_segname *sn,
                                 const char *uri,
                                 double duration,
                                 const char *title) {
  scope {
    jd_var *seg = make_segment(jd_nv(), uri, duration, title);
    hls_m3u8_push_segment(m3u8, seg);
    /* TODO rotate */

    hls_m3u8_save(m3u8, mc_segname_temp(sn));
    mc_segname_rename(sn);
    mc_debug("Updated %s", mc_segname_name(sn));
    mc_segname_inc(sn);
  }
}

static void parse_previous(jd_var *m3u8, mc_segname *sn) {
  jd_var *last = hls_m3u8_last_seg(m3u8);
  if (last) {
    jd_var *uri = jd_get_ks(last, "uri", 0);
    if (uri && mc_segname_parse(sn, jd_bytes(uri, NULL)))
      mc_segname_inc(sn);
  }
}

static void push_segment(seg_file *sf,
                         AVFormatContext *oc,
                         jd_var *m3u8,
                         jd_var *cfg,
                         mc_segname *pl_name,
                         double duration) {
  char *name = mc_strdup(mc_segname_name(sf->sn));
  seg_close(sf, oc);
  m3u8_push_segment(m3u8, cfg, pl_name, name, duration, "");
  free(name);
}

void mc_mux_hls(AVFormatContext *ic, jd_var *cfg, mc_queue_merger *qm) {
  scope {
    AVFormatContext *oc;
    AVPacket pkt;
    AVStream *vs = NULL, *as = NULL;
    seg_file sf;
    int vi = -1;

    sf.open = 0;
    sf.sn = mc_segname_new(cfg_need(cfg, "$.output.segment"));

    mc_segname *pl_name = mc_segname_new(cfg_need(cfg, "$.output.playlist"));
    jd_var *m3u8 = m3u8_init(jd_nv(), pl_name);
    parse_previous(m3u8, sf.sn);
    mc_info("Next segment is %s", mc_segname_name(sf.sn));

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

    double gop_time = NAN;
    double min_gop = mc_model_get_real(cfg, 4, "$.output.min_gop");
    double last_duration = 0;

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
          push_segment(&sf, oc, m3u8, cfg, pl_name, last_duration);
          gop_time = st;
        }
      }

      seg_open(&sf, oc);

      if (av_interleaved_write_frame(oc, &pkt))
        jd_throw("Can't write frame");

      av_free_packet(&pkt);
    }

    push_segment(&sf, oc, m3u8, cfg, pl_name, last_duration);

    for (unsigned i = 0; i < oc->nb_streams; i++) {
      av_freep(&oc->streams[i]->codec);
      av_freep(&oc->streams[i]);
    }

    av_free(oc);

    mc_segname_free(sf.sn);

    mc_debug("HLS EOF");
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
