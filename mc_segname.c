/* mc_segname.c */

#include <inttypes.h>
#include <jd_pretty.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mc_segname.h"
#include "mc_util.h"

static void freep(char **cp) {
  if (*cp) {
    free(*cp);
    *cp = NULL;
  }
}

static void free_names(mc_segname *sn) {
  freep(&sn->uri);
  freep(&sn->cur_name);
  freep(&sn->tmp_name);
}

static unsigned push_field(mc_segname *sn, const char *frag,
                           unsigned frag_len, unsigned field_len, unsigned pos) {
  mc_segname_field *snf = mc_alloc(sizeof(*snf));

  char fb[frag_len + 1];
  memcpy(fb, frag, frag_len);
  fb[frag_len] = '\0';

  snf->frag = frag;
  snf->frag_len = frag_len;
  snf->field_len = field_len;
  snf->pos = pos;

  snf->next = sn->fld;
  sn->fld = snf;
  return pos + frag_len + field_len;
}

static unsigned parse_format(mc_segname *sn) {
  unsigned pos = 0;
  const char *fmt = sn->fmt;
  const char *lp = fmt;
  while (*fmt) {
    if (*fmt++ == '%') {
      if (*fmt == '%') {
        fmt++;
        continue;
      }
      char *ep;
      unsigned long len = strtoul(fmt, &ep, 10);
      if (fmt == ep) jd_throw("Missing field width");
      if (len == 0) jd_throw("Field width must be non-zero");
      if (!*ep) jd_throw("Missing conversion character");
      if (*ep != 'd') jd_throw("The only legal conversion is %%Nd");

      pos = push_field(sn, lp, fmt - lp - 1, len, pos);
      fmt = lp = ep + 1;
    }
  }
  return push_field(sn, lp, fmt - lp, 0, pos);
}

mc_segname *mc_segname_new_prefixed(const char *fmt, const char *prefix) {
  mc_segname *sn = mc_alloc(sizeof(*sn));
  sn->fmt = fmt;
  sn->len = parse_format(sn);
  sn->prefix = mc_strdup(prefix);
  return sn;
}

mc_segname *mc_segname_new(const char *fmt) {
  return mc_segname_new_prefixed(fmt, NULL);
}

static void free_fields(mc_segname_field *snf) {
  for (mc_segname_field *next = snf; next; snf = next) {
    next = snf->next;
    free(snf);
  }
}

void mc_segname_free(mc_segname *sn) {
  if (sn) {
    free_fields(sn->fld);
    free_names(sn);
    free(sn->prefix);
    free(sn);
  }
}

int mc_segname_parse(mc_segname *sn, const char *name) {
  if (strlen(name) != sn->len) return 0;
  free_names(sn);
  for (mc_segname_field *snf = sn->fld; snf; snf = snf->next) {
    if (snf->field_len) {
      char tmp[snf->field_len + 1];
      if (memcmp(name + snf->pos, snf->frag, snf->frag_len)) return 0;
      memcpy(tmp, name + snf->pos + snf->frag_len, snf->field_len);
      tmp[snf->field_len] = '\0';
      char *ep;
      snf->seq = strtoul(tmp, &ep, 10);
      if (ep - tmp != snf->field_len) return 0;
    }
  }

  return 1;
}

int mc_segname_inc(mc_segname *sn) {
  free_names(sn);
  for (mc_segname_field *snf = sn->fld; snf; snf = snf->next) {
    uint64_t limit = 1;
    for (unsigned i = 0; i < snf->field_len; i++) limit *= 10;
    if (++snf->seq < limit) return 0;
    snf->seq = 0;
  }
  return 1;
}

char *mc_segname_format(mc_segname *sn) {
  char *buf = mc_alloc(sn->len + 1);

  for (mc_segname_field *snf = sn->fld; snf; snf = snf->next) {
    memcpy(buf + snf->pos, snf->frag, snf->frag_len);
    if (snf->field_len) {
      char fmt[20], tmp[snf->field_len + 1];
      sprintf(fmt, "%%0%u" PRIu64, snf->field_len);
      sprintf(tmp, fmt, snf->seq);
      memcpy(buf + snf->pos + snf->frag_len, tmp, snf->field_len);
    }
  }

  return buf;
}

char *mc_segname_next(mc_segname *sn) {
  char *next = mc_segname_format(sn);
  mc_segname_inc(sn);
  return next;
}

char *mc_segname_uri(mc_segname *sn) {
  if (!sn->uri)
    sn->uri = mc_segname_format(sn);
  return sn->uri;
}

char *mc_segname_prefix(mc_segname *sn, const char *name) {
  return mc_prefix(name, sn->prefix);
}

char *mc_segname_name(mc_segname *sn) {
  if (!sn->cur_name)
    sn->cur_name = mc_segname_prefix(sn, mc_segname_uri(sn));
  return sn->cur_name;
}

char *mc_segname_temp(mc_segname *sn) {
  if (!sn->tmp_name)
    sn->tmp_name = mc_tmp_name(mc_segname_name(sn));
  return sn->tmp_name;
}

void mc_segname_rename(mc_segname *sn) {
  char *temp = mc_segname_temp(sn);
  char *name = mc_segname_name(sn);
  if (rename(temp, name))
    jd_throw("Can't rename %s as %s: %m", temp, name);
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
