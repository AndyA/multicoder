/* mc_sequence.c */

#include <jd_pretty.h>

#include <string.h>
#include <stdlib.h>

#include "mc_sequence.h"
#include "mc_util.h"

static const char *mc_sequence_sym_num =
  "0123456789";

static const char *mc_sequence_sym_alnum =
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789";

static void with_format(const char *fmt,
                        void (*cb)(void *ctx, const char *fmt,
                                   const char *lit, unsigned lit_len,
                                   unsigned fld_len, const char *sym), void *ctx) {
  const char *fp = fmt;
  const char *lp = fmt;

  while (*fp) {
    if (*fp == '%') {
      fp++;
      if (*fp == '%') {
        fp++;
      }
      else {
        char *ep;
        const char *sym;
        unsigned long fl = strtoul(fp, &ep, 10);
        if (ep == fp) jd_throw("Expected field length in \"%s\"", fmt);
        switch (*ep++) {
        case 'a':
          sym = mc_sequence_sym_alnum;
          break;
        case 'n':
          sym = mc_sequence_sym_num;
          break;
        default:
          jd_throw("Bad sequence type in \"%s\"", fmt);
        }
        cb(ctx, fmt, lp, fp - lp - 1, fl, sym);
        lp = fp = ep;
      }
    }
    else {
      fp++;
    }
  }
  if (fp > lp) cb(ctx, fmt, lp, fp - lp, 0, NULL);
}

struct format_spec {
  unsigned seq_len;
  unsigned lit_len;
  const char *sym;
};

static void spec_cb(void *ctx, const char *fmt,
                    const char *lit, unsigned lit_len,
                    unsigned fld_len, const char *sym) {
  (void) lit;
  struct format_spec *spec = ctx;
  if (spec->sym && sym && spec->sym != sym)
    jd_throw("Can't mix multiple sequence types in \"%s\"", fmt);
  if (sym) spec->sym = sym;
  spec->seq_len += fld_len;
  spec->lit_len += lit_len;
}

mc_sequence *mc_sequence_new(const char *fmt) {
  struct format_spec spec = { 0, 0, NULL };
  with_format(fmt, spec_cb, &spec);
  if (!spec.seq_len || !spec.sym) jd_throw("No sequence specifiers in \"%s\"", fmt);

  mc_sequence *s = mc_alloc(sizeof(*s));
  s->fmt = fmt;
  s->radix = strlen(spec.sym);
  s->length = spec.seq_len;
  s->sym = spec.sym;
  s->count = mc_alloc(spec.seq_len);
  s->fmt_len = spec.lit_len + spec.seq_len;

  return s;
}

struct format_context {
  mc_sequence *s;
  char *buf;
  unsigned sp;
};

static void format_cb(void *ctx, const char *fmt,
                      const char *lit, unsigned lit_len,
                      unsigned fld_len, const char *sym) {
  (void) fmt;
  (void) sym;
  struct format_context *c = ctx;
  memcpy(c->buf, lit, lit_len);
  c->buf += lit_len;
  for (unsigned i = 0; i != fld_len; i++)
    *(c->buf)++ = c->s->sym[c->s->count[c->sp++]];
}

void mc_sequence_free(mc_sequence *s) {
  if (s) {
    free(s->count);
    free(s);
  }
}

int mc_sequence_inc(mc_sequence *s) {
  for (unsigned pos = s->length; pos > 0; pos--) {
    if (++s->count[pos - 1] < s->radix) return 0;
    s->count[pos - 1] = 0;
  }
  return 1;
}

int mc_sequence_compare(mc_sequence *sa, mc_sequence *sb) {
  if (sb->length > sa->length)
    return -mc_sequence_compare(sb, sa);

  for (unsigned pos = 0; pos < sa->length - sb->length; pos++)
    if (sa->count[pos]) return 1;

  return memcmp(sa->count + sa->length - sb->length,
                sb->count, sb->length);
}

char *mc_sequence_format(mc_sequence *s) {
  struct format_context fc;
  fc.s = s;
  fc.sp = 0;
  char *buf = fc.buf = mc_alloc(s->fmt_len + 1);
  with_format(s->fmt, format_cb, &fc);
  return buf;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
