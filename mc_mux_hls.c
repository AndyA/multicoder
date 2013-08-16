/* mc_mux_hls.c */

#include <jd_pretty.h>

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
  jd_var name, tmp_name;
  AVFormatContext *oc;
} output_context;

static void close_output(output_context *oc) {
}

static void open_output(jd_var *cfg, output_context *oc, uint64_t seg) {
  if (oc->oc) close_output(oc);

}

void mc_mux_hls(jd_var *cfg, mc_queue_merger *qm) {
  AVPacket pkt;
  AVFormatContext *oc;

  (void) cfg;

/*  avformat_alloc_output_context2(&oc, NULL, "mpegts", filename);*/

  av_init_packet(&pkt);

  while (mc_queue_merger_packet_get(qm, &pkt)) {
    mc_debug("HLS got %d (flags=%08x, pts=%llu, dts=%llu, duration=%d)",
             pkt.stream_index, pkt.flags,
             (unsigned long long) pkt.pts,
             (unsigned long long) pkt.dts,
             pkt.duration);
    av_free_packet(&pkt);
  }

  mc_debug("HLS EOF");
  avformat_free_context(oc);
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
