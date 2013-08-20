/* mc_util.c */

#include <errno.h>
#include <jd_pretty.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "mc_util.h"

#define TMP_PREFIX_LEN 10

void *mc_alloc(size_t sz) {
  void *m = malloc(sz);
  if (!m) abort();
  memset(m, 0, sz);
  return m;
}

char *mc_random_chars(char *s, size_t len) {
  for (unsigned i = 0; i < len;) {
    int cc  = rand() & 0x3f;
    if (cc < 26 + 10) s[i++] = cc < 26 ? 'a' + cc : '0' + cc - 26;
  }
  return s;
}

char *mc_tmp_name(const char *filename) {
  size_t len = strlen(filename);
  char *tmp = mc_alloc(len + TMP_PREFIX_LEN + 2);
  const char *slash = strrchr(filename, '/');
  if (slash) slash++;
  else slash = filename;
  memcpy(tmp, filename, slash - filename);
  mc_random_chars(tmp + (slash - filename), TMP_PREFIX_LEN);
  tmp[(slash - filename) + TMP_PREFIX_LEN] = '.';
  memcpy(tmp + (slash - filename) + TMP_PREFIX_LEN + 1, slash, len + filename - slash + 1);
  return tmp;
}

static char *extract(const char *buf, size_t len) {
  char *out = mc_alloc(len + 1);
  memcpy(out, buf, len);
  return out;
}

char *mc_strdup(const char *in) {
  return in ? extract(in, strlen(in)) : NULL;
}

char *mc_prefix(const char *name, const char *prefix) {
  if (!prefix || !*prefix) return mc_strdup(name);
  size_t len = strlen(prefix);
  size_t plen = len;
  if (prefix[plen - 1] != '/') plen++;
  char *fn = mc_alloc(plen + strlen(name) + 1);
  memcpy(fn, prefix, len);
  if (len != plen) fn[len] = '/';
  strcpy(fn + plen, name);
  return fn;
}

char *mc_dirname(const char *filename) {
  char *slash = strrchr(filename, '/');
  if (!slash || slash == filename) return NULL;
  return extract(filename, slash - filename);
}

int mc_is_file(const char *path) {
  struct stat st;
  return !stat(path, &st) && S_ISREG(st.st_mode);
}

void mc_mkpath(const char *path, mode_t mode) {
  if (mkdir(path, mode) == 0 || errno == EEXIST) return;
  char *parent = mc_dirname(path);
  if (parent) {
    mc_mkpath(parent, mode);
    free(parent);
  }
  if (mkdir(path, mode) && errno != EEXIST)
    jd_throw("Can't create %s: %m", path);
}

void mc_mkfilepath(const char *filename, mode_t mode) {
  char *parent = mc_dirname(filename);
  if (parent) {
    mc_mkpath(parent, mode);
    free(parent);
  }
}

static pthread_mutex_t wait_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_cond = PTHREAD_COND_INITIALIZER;

void mc_usleep(uint64_t usec) {
  struct timespec ts;
  struct timeval now;

  gettimeofday(&now, NULL);

  usec += now.tv_usec;

  ts.tv_sec = now.tv_sec + (usec / 1000000);
  ts.tv_nsec = (usec % 1000000) * 1000;

  pthread_mutex_lock(&wait_mutex);
  pthread_cond_timedwait(&wait_cond, &wait_mutex, &ts);
  pthread_mutex_unlock(&wait_mutex);
}

jd_var *mc_hash_merge(jd_var *out, jd_var *a, jd_var *b) {
  if (a == NULL && b == NULL) jd_throw("Can't merge two nulls");
  if (b == NULL) return jd_assign(out, a);
  if (a == NULL || a->type != HASH || b->type != HASH) return jd_assign(out, b);

  scope {
    jd_clone(out, a, 1);
    jd_var *keys = jd_keys(jd_nv(), b);
    unsigned kc = jd_count(keys);
    for (unsigned i = 0; i < kc; i++) {
      jd_var *key = jd_get_idx(keys, i);
      mc_hash_merge(jd_get_key(out, key, 1), jd_get_key(a, key, 0), jd_get_key(b, key, 0));
    }
  }
  return out;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
