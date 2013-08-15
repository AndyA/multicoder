/* multicoder.h */

#ifndef __MULTICODER_H
#define __MULTICODER_H

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "jd_pretty.h"

#define MC_ERROR_LEVELS \
  X(ERROR)    \
  X(FATAL)    \
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

void mc_debug(const char *msg, ...);
void mc_info(const char *msg, ...);
void mc_warning(const char *msg, ...);
void mc_error(const char *msg, ...);
void mc_fatal(const char *msg, ...);

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
