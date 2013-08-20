/* multicoder.h */

#ifndef __MULTICODER_H
#define __MULTICODER_H

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include <libavformat/avformat.h>

#include "jd_pretty.h"

#include "mc_buffer.h"
#include "mc_model.h"
#include "mc_queue.h"
#include "mc_segname.h"
#include "mc_util.h"

#define MC_ERROR_LEVELS \
  X(FATAL)    \
  X(ERROR)    \
  X(WARNING)  \
  X(INFO)     \
  X(DEBUG)

/* Error levels */
#define X(x) x,
enum {
  MC_ERROR_LEVELS
  MAXLEVEL
};
#undef X

extern unsigned mc_log_level;
extern unsigned mc_log_colour;

void mc_log_avutil(void *ptr, int level, const char *msg, va_list ap);

unsigned mc_log_decode_level(const char *name);
void mc_log_set_thread(const char *name);
const char *mc_log_get_thread(void);

void mc_debug(const char *msg, ...);
void mc_info(const char *msg, ...);
void mc_warning(const char *msg, ...);
void mc_error(const char *msg, ...);
void mc_fatal(const char *msg, ...);

void mc_h264_decode(AVFormatContext *fcx, jd_var *cfg, mc_queue_merger *qi, mc_queue *qo);
void mc_demux(AVFormatContext *fcx, jd_var *cfg, mc_queue *aq, mc_queue *vq);
void mc_mux_hls(AVFormatContext *fcx, jd_var *cfg, mc_queue_merger *qm);

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
