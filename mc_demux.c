/* mc_demux.c */

#include <jd_pretty.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#include "multicoder.h"

void mc_demux(AVFormatContext *ic, jd_var *cfg, mc_queue *aq, mc_queue *vq) {
  AVPacket pkt;

  (void) cfg;

  int aud = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  int vid = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

  if (aud < 0 && vid < 0) jd_throw("Can't find audio or video");

  av_init_packet(&pkt);
  pkt.data = NULL;
  pkt.size = 0;

  while (av_read_frame(ic, &pkt) >= 0) {
    av_dup_packet(&pkt); /* force refcount */
    if (pkt.stream_index == aud)
      mc_queue_packet_put(aq, &pkt);
    else if (pkt.stream_index == vid)
      mc_queue_packet_put(vq, &pkt);

    av_free_packet(&pkt);
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
