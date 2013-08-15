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

static void send_frame(mc_buffer *out, const uint8_t *buf,
                       unsigned wrap, unsigned xsize, unsigned ysize) {
  mc_debug("Send frame [%u x %u]", xsize, ysize);
  for (unsigned y = 0; y < ysize; y++)
    mc_buffer_send_input(out, buf + y * wrap, xsize);
}

static unsigned decode_and_send(mc_buffer *out,
                                AVCodecContext *avctx,
                                AVFrame *frame,
                                AVPacket *pkt) {

  int len, got_frame;

  mc_debug("decode_and_send(%p, %u)", pkt->data, (unsigned) pkt->size);

  len = avcodec_decode_video2(avctx, frame, &got_frame, pkt);
  if (len < 0) {
    /*    jd_throw("Decode error");*/
    mc_error("Decode error");
  }

  if (got_frame)
    send_frame(out, frame->data[0], frame->linesize[0],
               avctx->width, avctx->height);

  if (pkt->data) {
    pkt->size -= len;
    pkt->data += len;
  }

  return got_frame ? 1 : 0;
}

void mc_h264_decoder(mc_buffer_reader *in, mc_buffer *out) {
  AVCodec *codec;
  AVCodecContext *c;
  AVFrame *frame;
  AVPacket avpkt;
  unsigned frame_count;
  mc_buffer_iov iiov;

  av_init_packet(&avpkt);

  /*  codec = avcodec_find_decoder_by_name("libx264");*/
  codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) jd_throw("Codec not found");

  c = avcodec_alloc_context3(codec);
  if (!c) jd_throw("Can't allocate video codec context");

  if (codec->capabilities & CODEC_CAP_TRUNCATED)
    c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

  /* For some codecs, such as msmpeg4 and mpeg4, width and height
     MUST be initialized there because this information is not
     available in the bitstream. */

  /* open it */
  if (avcodec_open2(c, codec, NULL) < 0)
    jd_throw("Can't open codec");

  frame = avcodec_alloc_frame();
  if (!frame) jd_throw("Can't allocate frame");

  for (;;) {
    size_t got = mc_buffer_wait_output(in, &iiov);
    if (got == 0) break;
    for (unsigned i = 0; i < iiov.iovcnt; i++) {
      avpkt.size = iiov.iov[i].iov_len;
      avpkt.data = iiov.iov[i].iov_base;
      while (avpkt.size > 0)
        decode_and_send(out, c, frame, &avpkt);
    }
    mc_buffer_commit_output(in, got);
  }

  avpkt.data = NULL;
  avpkt.size = 0;
  decode_and_send(out, c, frame, &avpkt);

  avcodec_close(c);
  av_free(c);
  avcodec_free_frame(&frame);
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
