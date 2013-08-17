/* mc_util.c */

#include <jd_pretty.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

char *mc_dirname(const char *filename) {
  char *slash = strrchr(filename, '/');
  if (!slash || slash == filename) return NULL;
  return extract(filename, slash - filename);
}

void mc_mkpath(const char *path, mode_t mode) {
  struct stat st;
  if (!stat(path, &st) && S_ISDIR(st.st_mode)) return;
  char *parent = mc_dirname(path);
  if (parent) {
    mc_mkpath(parent, mode);
    free(parent);
  }
  if (mkdir(path, mode))
    jd_throw("Can't create %s: %m", path);
}

void mc_mkfilepath(const char *filename, mode_t mode) {
  char *parent = mc_dirname(filename);
  if (parent) {
    mc_mkpath(parent, mode);
    free(parent);
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
