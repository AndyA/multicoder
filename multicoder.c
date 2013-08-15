/* multicoder.c */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "multicoder.h"

typedef struct {
  int fd;
  mc_buffer *b;
  pthread_t t;
} rd_ctx;

typedef struct {
  int fd;
  mc_buffer_reader *br;
  pthread_t t;
} wr_ctx;

static void *reader(void *ctx) {
  rd_ctx *cx = (rd_ctx *) ctx;
  for (;;) {
    mc_buffer_iov iov;
    mc_buffer_wait_input(cx->b, &iov);
    ssize_t got = readv(cx->fd, iov.iov, iov.iovcnt);
    if (got < 0) {
      fprintf(stderr, "Read failed: %m\n");
      exit(1);
    }
    if (got == 0) break;
    mc_buffer_commit_input(cx->b, got);
  }
  return NULL;
}

static void *writer(void *ctx) {
  wr_ctx *cx = (wr_ctx *) ctx;
  for (;;) {
    mc_buffer_iov iov;
    size_t spc = mc_buffer_wait_output(cx->br, &iov);
    if (spc == 0) break;
    mc_debug("writev (spc=%lu)", (unsigned long) spc);
    for (unsigned i = 0; i < iov.iovcnt; i++)
      mc_debug("  %p %lu", iov.iov[i].iov_base, (unsigned long) iov.iov[i].iov_len);
    ssize_t put = writev(cx->fd, iov.iov, iov.iovcnt);
    if (put < 0) {
      fprintf(stderr, "Write failed: %m\n");
      exit(1);
    }
    mc_buffer_commit_output(cx->br, put);
  }
  return NULL;
}

int main() {
  rd_ctx rcx;
  wr_ctx wcx;

  mc_info("Starting multicoder");

  rcx.fd = 0;
  rcx.b = mc_buffer_new(1024 * 1024);
  mc_buffer_reader *in = mc_buffer_add_reader(rcx.b);
  mc_buffer *out = mc_buffer_new(1024 * 1024);
  wcx.fd = 1;
  wcx.br = mc_buffer_add_reader(out);

  pthread_create(&rcx.t, NULL, reader, &rcx);
  mc_debug("Created reader");

  pthread_create(&wcx.t, NULL, writer, &wcx);
  mc_debug("Created writer");

  avcodec_register_all();

  mc_debug("Decoding");
  mc_h264_decoder(in, out);

  pthread_join(wcx.t, NULL);
  pthread_join(rcx.t, NULL);

  mc_buffer_free(rcx.b);
  mc_buffer_free(out);

  return 0;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
