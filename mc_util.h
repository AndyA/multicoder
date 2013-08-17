/* mc_util.h */

#ifndef MC_UTIL_H_
#define MC_UTIL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

  void *mc_alloc(size_t sz);
  char *mc_random_chars(char *s, size_t len);
  char *mc_tmp_name(const char *filename);
  char *mc_dirname(const char *filename);
  void mc_mkpath(const char *path, mode_t mode);
  void mc_mkfilepath(const char *filename, mode_t mode);
  void mc_usleep(uint64_t usec);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
