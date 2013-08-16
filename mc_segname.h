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
  } mc_segname;

  mc_segname *mc_segname_new(const char *fmt);
  void mc_segname_free(mc_segname *sn);
  int mc_segname_parse(mc_segname *sn, const char *name);
  int mc_segname_inc(mc_segname *sn);
  char *mc_segname_format(mc_segname *sn);
  char *mc_segname_next(mc_segname *sn);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
