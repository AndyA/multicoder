/* mc_mux_hls.c */

#include <jd_pretty.h>
#include <stdio.h>

#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

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
  char *name;
  char *tmp_name;
  int open;
} seg_file;

static void seg_close(seg_file *sf, AVFormatContext *oc) {
  if (sf->open) {
    av_write_trailer(oc);
    avio_close(oc->pb);
    sf->open = 0;

    mc_debug("renaming %s as %s", sf->tmp_name, sf->name);
    if (rename(sf->tmp_name, sf->name))
      jd_throw("Can't rename %s as %s: %m", sf->tmp_name, sf->name);

    free(sf->name);
    free(sf->tmp_name);
  }
}

static void seg_open(seg_file *sf, AVFormatContext *oc) {
  if (!sf->open) {
    sf->name = mc_segname_next(sf->sn);
    sf->tmp_name = mc_tmp_name(sf->name);
    mc_debug("writing %s (as %s)", sf->name, sf->tmp_name);
    mc_mkfilepath(sf->tmp_name, 0777);

    if (avio_open(&oc->pb, sf->tmp_name, AVIO_FLAG_WRITE) < 0)
      jd_throw("Can't write %s: %m", sf->tmp_name);
    if (avformat_write_header(oc, NULL))
      jd_throw("Can't write header");
    sf->open = 1;
  }
}

void mc_mux_hls(AVFormatContext *ic, jd_var *cfg, mc_queue_merger *qm) {
  AVFormatContext *oc;
  AVPacket pkt;
  AVStream *vs = NULL, *as = NULL;
  unsigned vi = 0, ai = 0;
  seg_file sf;

  jd_var *seg_name = jd_rv(cfg, "$.output.segment");
  if (!seg_name) jd_throw("Missing segment field");

  sf.open = 0;
  sf.sn = mc_segname_new(jd_bytes(seg_name, NULL));

  if (oc = avformat_alloc_context(), !oc)
    jd_throw("Can't allocate output context");

  if (oc->oformat = av_guess_format("mpegts", NULL, NULL), !oc->oformat)
    jd_throw("Can't find mpegts mulitplexer");

  for (unsigned i = 0; i < ic->nb_streams; i++) {
    AVStream *is = ic->streams[i];
    is->discard = AVDISCARD_NONE;

    if (!vs && is->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      vi = i;
      vs = add_output(oc, is);
      continue;
    }

    if (!as && is->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
      ai = i;
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

    seg_open(&sf, oc);

    if (av_interleaved_write_frame(oc, &pkt))
      jd_throw("Can't write frame");

    av_free_packet(&pkt);
  }

  seg_close(&sf, oc);

  for (unsigned i = 0; i < oc->nb_streams; i++) {
    av_freep(&oc->streams[i]->codec);
    av_freep(&oc->streams[i]);
  }

  av_free(oc);

  mc_segname_free(sf.sn);

  mc_debug("HLS EOF");
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
