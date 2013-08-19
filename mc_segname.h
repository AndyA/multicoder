/* mc_segname.h */

#ifndef MC_SEGNAME_H_
#define MC_SEGNAME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

  typedef struct mc_segname_field {
    struct mc_segname_field *next;
    const char *frag;
    unsigned frag_len, field_len;
    unsigned pos;
    uint64_t seq;
  } mc_segname_field;

  typedef struct {
    mc_segname_field *fld;
    const char *fmt;
    unsigned len;
    char *prefix;
    char *uri;
    char *cur_name;
    char *tmp_name;
  } mc_segname;

  mc_segname *mc_segname_new_prefixed(const char *fmt, const char *prefix);
  mc_segname *mc_segname_new(const char *fmt);
  void mc_segname_free(mc_segname *sn);
  int mc_segname_parse(mc_segname *sn, const char *name);
  int mc_segname_inc(mc_segname *sn);
  char *mc_segname_format(mc_segname *sn);
  char *mc_segname_next(mc_segname *sn);
  char *mc_segname_uri(mc_segname *sn);
  char *mc_segname_name(mc_segname *sn);
  char *mc_segname_temp(mc_segname *sn);
  void mc_segname_rename(mc_segname *sn);
  char *mc_segname_prefix(mc_segname *sn, const char *name);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
