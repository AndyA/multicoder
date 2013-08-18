/* hls_m3u8_parser.c */

#include <ctype.h>
#include <jd_pretty.h>

#include "hls.h"

extern const char *hls__m3u8_parser_machine;

#define SKIP(lp) while (*(lp) && isspace(*(lp))) (lp)++

#define isattr(c) (((c) >= 'A' && (c) <= 'Z') || (c) == '-')
#define istag(c)  (isattr(c) || ((c) >= '0' && (c) <= '9'))

static jd_var *parse_string(jd_var *out, const char **lp) {
  const char *sp = *lp;
  (*lp)++;
  while (*(*lp) && *(*lp) != '\"') {
    if (*(*lp) == '\\')(*lp)++;
    (*lp)++;
  }
  if (*(*lp) != '\"') jd_throw("Missing closing quote");
  (*lp)++;
  return jd_from_json(out, jd_set_bytes(jd_nv(), sp, *lp - sp));
}

static jd_var *parse_attr(jd_var *out, const char *lp) {
  jd_set_hash(out, 10);

  scope {
    for (;;) {
      jd_var *k, *v;

      SKIP(lp);
      const char *np = lp;
      while (*lp && isattr(*lp)) lp++;
      if (np == lp) jd_throw("Missing attr name");
      k = jd_set_bytes(jd_nv(), np, lp - np);

      if (*lp != '=') jd_throw("Missing '='");
      lp++;

      if (*lp == '"') {
        v = parse_string(jd_nv(), &lp);
      }
      else {
        const char *vp = lp;
        while (*lp && *lp != ',') lp++;
        v = jd_set_bytes(jd_nv(), vp, lp - vp);
      }
      jd_assign(jd_get_key(out, k, 1), v);
      if (!*lp) break;
      if (*lp++ != ',') jd_throw("Missing comma");
    }
  }
  return out;
}

static jd_var *need_attr(jd_var *out, const char *lp) {
  if (*lp != ':') jd_throw("Missing attibute list");
  return parse_attr(out, lp + 1);
}

static int is(jd_var *v, const char *s) {
  return 0 == strcmp(jd_bytes(v, NULL), s);
}

static void parse_lines(jd_var *out, jd_var *lines) {
  enum { INIT, HLS, HLSSEG, HLSPL, IGNORE } state = INIT;

  scope {
    jd_var *global = jd_nhv(10);
    jd_var *seg = NULL;

    for (unsigned ln = 0; ln < jd_count(lines); ln++) {
      jd_var *line = jd_trim(jd_nv(), jd_get_idx(lines, ln));
      const char *lp = jd_bytes(line, NULL);
      if (!*lp) continue;

      if (*lp == '#') {
        const char *tp = ++lp;
        if (!isattr(*lp)) jd_throw("Bad attribute name");
        while (*lp && istag(*lp)) lp++;
        jd_var *tag = jd_set_bytes(jd_nv(), tp, lp - tp);
        jd_printf("tag: %V\n", tag);
        switch (state) {
        case INIT:
          if (is(tag, "EXTM3U"))
            state = HLS;
          break;
        case HLS:
          if (is(tag, "EXT-X-MAP") || is(tag, "EXT-X-KEY")) {
            need_attr(jd_get_key(global, tag, 1), lp);
            break;
          }

          if (is(tag, "EXT-X-ALLOW-CACHE") || is(tag, "EXT-X-MEDIA-SEQUENCE") ||
              is(tag, "EXT-X-PLAYLIST-TYPE") || is(tag, "EXT-X-TARGETDURATION") ||
              is(tag, "EXT-X-VERSION")) {
            if (*lp++ != ':') jd_throw("Missing attribute after %V", tag);
            jd_set_string(jd_get_key(jd_get_ks(out, "meta", 1), tag, 1), lp);
            break;
          }

          if (is(tag, "EXT-X-MEDIA") || is(tag, "EXT-X-I-FRAME-STREAM-INF")) {
            jd_var *slot = jd_get_key(jd_get_ks(out, "meta", 1), tag, 1);
            if (slot->type != ARRAY) jd_set_array(slot, 10);
            need_attr(jd_push(slot, 1), lp);
            break;
          }

          if (is(tag, "EXT-X-I-FRAMES-ONLY")) {
            if (*lp) jd_throw("Extra text after %V", tag);
            jd_set_bool(jd_get_key(jd_get_ks(out, "meta", 1), tag, 1), 1);
            break;
          }

          if (is(tag, "EXT-X-ENDLIST")) {
            if (*lp) jd_throw("Extra text after %V", tag);
            jd_set_bool(jd_get_ks(out, "closed", 1), 1);
            state = IGNORE;
            break;
          }

          if (is(tag, "EXT-X-STREAM-INF")) {
            seg = jd_clone(jd_nv(), global, 0);
            need_attr(jd_get_key(seg, tag, 1), lp);
            state = HLSPL;
            break;
          }

          if (is(tag, "EXT-X-DISCONTINUITY")) {
            jd_assign(jd_push(jd_get_ks(out, "seg", 1), 1), tag);
            break;
          }

          /* fall through */
        case HLSSEG:
          if (is(tag, "EXTINF")) {
            if (!seg) seg = jd_nhv(5);
            if (*lp++ != ':') jd_throw("Missing attributes after %V", tag);
            char *comma = strchr(lp, ',');
            if (comma) {
              jd_set_bytes(jd_get_ks(seg, "duration", 1), lp, comma - lp);
              jd_set_string(jd_get_ks(seg, "title", 1), comma + 1);
            }
            else {
              jd_set_string(jd_get_ks(seg, "duration", 1), lp);
              jd_set_string(jd_get_ks(seg, "title", 1), "");
            }
            state = HLSSEG;
            break;
          }

          if (is(tag, "EXT-X-PROGRAM-DATE-TIME")) {
            break;
          }

          if (is(tag, "EXT-X-BYTERANGE")) {
            break;
          }
          break;
        case HLSPL:
          break;
        case IGNORE:
          break;
        }
      }
      else {
        /* URI */
        switch (state) {
        case HLS:
        case HLSSEG:
        case HLSPL:
          if (!seg) seg = jd_nhv(5);
          jd_set_string(jd_get_ks(seg, "uri", 1), lp);
          jd_assign(jd_push(jd_get_ks(out, state == HLSSEG ? "seg" : "vpl", 1), 1), seg);
          seg = NULL;
          state = HLS;
          break;

        default:
          break;
        }
      }
    }
  }
}

jd_var *hls_m3u8_parse(jd_var *out, jd_var *m3u8) {
  scope {
    hls_m3u8_init(out);
    parse_lines(out, jd_split(jd_nv(), m3u8, jd_nsv("\n")));
  }
  return out;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
