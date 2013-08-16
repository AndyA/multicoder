/* mc_util.h */

#ifndef MC_UTIL_H_
#define MC_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

  void *mc_alloc(size_t sz);
  char *mc_random_chars(char *s, size_t len);
  char *mc_tmp_name(const char *filename);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
