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

typedef struct {
  AVFormatContext *oc;
  mc_segname *sn;
  char *name, *tmp_name;
} output_context;

static void close_output(output_context *oc) {
  avformat_free_context(oc->oc);
  oc->oc = NULL;

  mc_debug("renaming %s as %s", oc->tmp_name, oc->name);
  if (rename(oc->tmp_name, oc->name))
    jd_throw("Can't rename %s as %s: %m", oc->tmp_name, oc->name);

  free(oc->name);
  free(oc->tmp_name);
  oc->name = NULL;
  oc->tmp_name = NULL;
}

static void open_output(output_context *oc) {
  if (oc->oc) return;
  oc->name = mc_segname_next(oc->sn);
  oc->tmp_name = mc_tmp_name(oc->name);
  mc_debug("writing %s (as %s)", oc->name, oc->tmp_name);
  avformat_alloc_output_context2(&oc->oc, NULL, "mpegts", oc->tmp_name);
  if (!oc->oc) jd_throw("Can't create output context for %s", oc->tmp_name);


}

static void init_output(output_context *oc, mc_segname *sn) {
  oc->oc = NULL;
  oc->sn = sn;
  oc->name = NULL;
}

void mc_mux_hls(AVFormatContext *fcx, jd_var *cfg, mc_queue_merger *qm) {
  AVPacket pkt;
  output_context oc;

  (void) fcx;

  jd_var *seg_name = jd_rv(cfg, "$.output.segment");
  if (!seg_name) jd_throw("Missing segment field");

  init_output(&oc, mc_segname_new(jd_bytes(seg_name, NULL)));


  av_init_packet(&pkt);

  while (mc_queue_merger_packet_get(qm, &pkt)) {
    mc_debug("HLS got %d (flags=%08x, pts=%llu, dts=%llu, duration=%d)",
             pkt.stream_index, pkt.flags,
             (unsigned long long) pkt.pts,
             (unsigned long long) pkt.dts,
             pkt.duration);
    open_output(&oc);
    av_free_packet(&pkt);
  }

  close_output(&oc);

  mc_segname_free(oc.sn);

  mc_debug("HLS EOF");
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
