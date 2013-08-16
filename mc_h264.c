/* mc_h264_decoder.c */

#include <jd_pretty.h>
#include <math.h>
#include <stdint.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

#include "multicoder.h"

#define INBUF_SIZE 4096

static unsigned decode(mc_queue *q,
                       AVCodecContext *avctx,
                       AVFrame *frame,
                       AVPacket *pkt) {

  int len, got_frame;

  mc_debug("decode(%p, %u)", pkt->data, (unsigned) pkt->size);

  len = avcodec_decode_video2(avctx, frame, &got_frame, pkt);
  if (len < 0) mc_error("Decode error");

  if (got_frame) {
    mc_queue_frame_put(q, frame);
    av_frame_unref(frame);
  }

  if (pkt->data && len > 0) {
    pkt->size -= len;
    pkt->data += len;
  }

  return got_frame ? 1 : 0;
}

void mc_h264_decode(jd_var *cfg, mc_queue *qi, mc_queue *qo) {
  AVCodec *codec;
  AVCodecContext *c;
  AVFrame *frame;
  AVPacket avpkt;

  (void) cfg;

  av_init_packet(&avpkt);

  codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) jd_throw("Codec not found");

  c = avcodec_alloc_context3(codec);
  if (!c) jd_throw("Can't allocate video codec context");

  if (codec->capabilities & CODEC_CAP_TRUNCATED)
    c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

  c->refcounted_frames = 1;

  /* For some codecs, such as msmpeg4 and mpeg4, width and height
     MUST be initialized there because this information is not
     available in the bitstream. */

  /* open it */
  if (avcodec_open2(c, codec, NULL) < 0)
    jd_throw("Can't open codec");

  frame = avcodec_alloc_frame();
  if (!frame) jd_throw("Can't allocate frame");

  while (mc_queue_packet_get(qi, &avpkt))
    decode(qo, c, frame, &avpkt);

  avpkt.data = NULL;
  avpkt.size = 0;
  decode(qo, c, frame, &avpkt);

  mc_queue_frame_put(qo, NULL);

  avcodec_close(c);
  av_free(c);
  avcodec_free_frame(&frame);
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
