/* mc_sequence.h */

#ifndef MC_SEQUENCE_H_
#define MC_SEQUENCE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

  typedef struct {
    const char *fmt;
    unsigned radix;
    unsigned length;
    unsigned fmt_len;
    const char *sym;
    uint8_t *count;
  } mc_sequence;

  extern const char *mc_sequence_sym_num;
  extern const char *mc_sequence_sym_alnum;

  mc_sequence *mc_sequence_new(const char *fmt);
  void mc_sequence_free(mc_sequence *s);
  int mc_sequence_inc(mc_sequence *s);
  int mc_sequence_compare(mc_sequence *sa, mc_sequence *sb);
  char *mc_sequence_format(mc_sequence *s);

#ifdef __cplusplus
}
#endif

#endif

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
