/* mc_buffer.h */

#ifndef __BUFFER_H
#define __BUFFER_H

#include <stdlib.h>
#include <sys/uio.h>
#include <pthread.h>

typedef struct mc_buffer mc_buffer;

typedef struct mc_buffer_reader {
  unsigned long pos;
  struct mc_buffer_reader *next;
  mc_buffer *b;
} mc_buffer_reader;

struct mc_buffer {
  unsigned char *b;
  size_t size;
  unsigned long pos;
  mc_buffer_reader *br;
  int eof;
  pthread_mutex_t mutex;
  pthread_cond_t can_read;
  pthread_cond_t can_write;
};

typedef struct {
  struct iovec iov[2];
  int iovcnt;
} mc_buffer_iov;

mc_buffer *mc_buffer_new(size_t sz);
void mc_buffer_free(mc_buffer *b);
mc_buffer_reader *mc_buffer_add_reader(mc_buffer *b);
void mc_buffer_eof(mc_buffer *b);

size_t mc_buffer_space(mc_buffer *b);
size_t mc_buffer_get_input(mc_buffer *b, mc_buffer_iov *bi);
void mc_buffer_commit_input(mc_buffer *b, size_t len);
size_t mc_buffer_wait_input(mc_buffer *b, mc_buffer_iov *bi);
size_t mc_buffer_send_input(mc_buffer *b, const void *base, size_t len);

size_t mc_buffer_available(mc_buffer_reader *br);
size_t mc_buffer_get_output(mc_buffer_reader *br, mc_buffer_iov *bi);
void mc_buffer_commit_output(mc_buffer_reader *br, size_t len);
size_t mc_buffer_wait_output(mc_buffer_reader *br, mc_buffer_iov *bi);

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
