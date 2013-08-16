/* mc_util.c */

#include <stdlib.h>
#include <string.h>

#include "mc_util.h"

#define TMP_PREFIX_LEN 32

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

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
