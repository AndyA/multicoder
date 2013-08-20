/* mc_hls.c */

#include <jd_pretty.h>

#include "hls.h"
#include "mc_hls.h"
#include "multicoder.h"

static const char *kinds[] = { "audio", "video" };

static unsigned bit_rate(jd_var *stm) {
  unsigned total = 0;
  for (unsigned i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
    jd_var *spec = jd_get_ks(stm, kinds[i], 0);
    if (spec) {
      jd_var *br = jd_get_ks(spec, "bit_rate", 0);
      if (br) total += jd_get_int(br);
    }
  }
  return total;
}

jd_var *mc_hls_make_root(jd_var *m3u8, jd_var *ctx, jd_var *spec) {
  scope {
    hls_m3u8_init(m3u8);

    jd_var *inc = jd_get_ks(spec, "include", 0);
    if (!inc) jd_throw("Missing include");
    jd_var *by_name = jd_get_ks(ctx, "by_name", 0);

    for (unsigned i = 0; i < jd_count(inc); i++) {
      jd_var *name = jd_get_idx(inc, i);
      jd_var *stm = jd_get_key(by_name, name, 0);
      if (!stm) {
        mc_warning("No active stream %V", name);
        continue;
      }
      unsigned br = bit_rate(stm);
      if (br == 0) jd_throw("Stream %V has 0 bit rate", name);
      jd_var *uri = jd_rv(stm, "$.output.playlist");
      if (!uri) jd_throw("Stream %V has no playlist", name);

      jd_var *rec = jd_nhv(2);
      jd_assign(jd_get_ks(rec, "uri", 1), uri);
      jd_var *meta = jd_set_hash(jd_get_ks(rec, "EXT-X-STREAM-INF", 1), 5);
      jd_set_int(jd_get_ks(meta, "BANDWIDTH", 1), br);
      jd_set_int(jd_get_ks(meta, "PROGRAM-ID", 1), 1);
      hls_m3u8_push_playlist(m3u8, rec);
    }
  }

  return m3u8;
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
