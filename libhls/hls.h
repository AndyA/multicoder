/* hls.h */

#ifndef HLS_H_
#define HLS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <jd_pretty.h>

  jd_var *hls__get_syntax(jd_var *m3u8);

  jd_var *hls_m3u8_init(jd_var *out);
  jd_var *hls_m3u8_last_seg(jd_var *m3u8);

  jd_var *hls_m3u8_parse(jd_var *out, jd_var *m3u8);
  jd_var *hls_m3u8_format(jd_var *out, jd_var *m3u8);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
