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

void mc_mux_hls(jd_var *cfg, mc_queue_merger *qm) {
  AVPacket pkt;

  (void) cfg;

  av_init_packet(&pkt);

  while (mc_queue_merger_get(qm, &pkt)) {
    mc_debug("HLS got %d (flags=%08x, pts=%llu, dts=%llu, duration=%d)",
             pkt.stream_index, pkt.flags,
             (unsigned long long) pkt.pts,
             (unsigned long long) pkt.dts,
             pkt.duration);
    av_free_packet(&pkt);
  }

  mc_debug("HLS EOF");

}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
