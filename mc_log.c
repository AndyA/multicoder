/* mc_log.c */

#include "multicoder.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <libavutil/log.h>

#define TS_FORMAT "%Y/%m/%d %H:%M:%S"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t tn_key;
static unsigned max_name = 0;

unsigned mc_log_level  = DEBUG;
unsigned mc_log_colour = 1;

#define X(x) #x,
static const char *lvl[] = {
  MC_ERROR_LEVELS
  NULL
};
#undef X

static const char *lvl_col[] = {
  "\x1b[41m" "\x1b[37m",  // ERROR    white on red
  "\x1b[41m" "\x1b[37m",  // FATAL    white on red
  "\x1b[31m",             // WARNING  red
  "\x1b[36m",             // INFO     cyan
  "\x1b[32m",             // DEBUG    green
};

#define COLOUR_RESET "\x1b[0m"

static void ts(char *buf, size_t sz) {
  struct timeval tv;
  struct tm *tmp;
  size_t len;
  gettimeofday(&tv, NULL);
  tmp = gmtime(&tv.tv_sec);
  if (!tmp) jd_throw("gmtime failed: %m");
  len = strftime(buf, sz, TS_FORMAT, tmp);
  snprintf(buf + len, sz - len, ".%06lu", (unsigned long) tv.tv_usec);
}

static pthread_key_t thread_key() {
  static int done_init = 0;
  if (!done_init) {
    pthread_key_create(&tn_key, free);
    done_init++;
  }
  return tn_key;
}

void mc_log_set_thread(const char *name) {
  pthread_key_t key = thread_key();
  char *old = pthread_getspecific(key);
  if (old) free(old);
  pthread_setspecific(key, mc_strdup(name));
  pthread_mutex_lock(&mutex);
  size_t len = strlen(name);
  if (len > max_name) max_name = len;
  pthread_mutex_unlock(&mutex);
}

const char *mc_log_get_thread(void) {
  return pthread_getspecific(thread_key());
}

unsigned mc_log_decode_level(const char *name) {
  for (unsigned i = 0; i < MAXLEVEL; i++)
    if (!strcmp(name, lvl[i])) return i;
  jd_throw("Bad log level: %s", name);
}

static void mc_log(unsigned level, const char *msg, va_list ap) {
  if (level <= mc_log_level) scope {
    const char *col_on = mc_log_colour ? lvl_col[level] : "";
    const char *col_off = mc_log_colour ? COLOUR_RESET : "";
    char tmp[30];
    unsigned i;
    size_t count;
    jd_var *ldr = jd_nv(), *str = jd_nv(), *ln = jd_nv();

    ts(tmp, sizeof(tmp));
    pthread_mutex_lock(&mutex);
    if (max_name) {
      jd_var *fmt = jd_sprintf(jd_nv(), "%%s | %%5lu | %%-%ds | %%-7s | ", max_name);
      jd_sprintvf(ldr, fmt, tmp, (unsigned long) getpid(),
      mc_log_get_thread(), lvl[level]);
    }
    else {
      jd_sprintf(ldr, "%s | %5lu | %-7s | ", tmp, (unsigned long) getpid(), lvl[level]);

    }
    jd_vsprintf(str, msg, ap);
    jd_split(ln, str, jd_nsv("\n"));
    count = jd_count(ln);
    jd_var *last = jd_get_idx(ln, count - 1);
    if (jd_length(last) == 0) count--;

    for (i = 0; i < count; i++) {
      jd_fprintf(stderr, "%s%V%V%s\n", col_on, ldr, jd_get_idx(ln, i), col_off);
    }
    pthread_mutex_unlock(&mutex);
  }
}

static unsigned avu2mc(int level) {
  struct {
    int avl;
    unsigned mcl;
  } lmap[] = {
    {AV_LOG_DEBUG, DEBUG},
    {AV_LOG_INFO, INFO},
    {AV_LOG_WARNING, WARNING},
    {AV_LOG_ERROR, ERROR},
    {AV_LOG_FATAL, FATAL},
  };

  for (unsigned i = 0; i < sizeof(lmap) / sizeof(lmap[0]); i++)
    if (level >= lmap[i].avl) return lmap[i].mcl;
  return FATAL;
}

void mc_log_avutil(void *ptr, int level, const char *msg, va_list ap) {
  (void) ptr;
  mc_log(avu2mc(level), msg, ap);
}

#define LOGGER(name, level)          \
  void name(const char *msg, ...) {  \
    va_list ap;                      \
    va_start(ap, msg);               \
    mc_log(level, msg, ap);          \
    va_end(ap);                      \
  }

LOGGER(mc_debug,    DEBUG)
LOGGER(mc_info,     INFO)
LOGGER(mc_warning,  WARNING)
LOGGER(mc_error,    ERROR)
LOGGER(mc_fatal,    FATAL)

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
