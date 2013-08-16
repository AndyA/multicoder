/* basic.t */

#include <jd_pretty.h>
#include <stdlib.h>
#include <string.h>

#include "framework.h"
#include "mc_model.h"
#include "tap.h"

static void test_getters(void) {
  scope {
    jd_var *m = tf_load_resource(jd_nv(), "data/general.json");

    ok(!strcmp(mc_model_get_str(m, "", "$.general.log") , "debug"), "access field");
    ok(mc_model_get_real(m, 1.23, "$.options.pi") == 1.23, "access default");
  }
}

static jd_var *resource_list(jd_var *out, const char const *res[]) {
  jd_set_array(out, 10);
  for (unsigned i = 0; res[i]; i++) {
    char *rname = tf_resource(res[i]);
    jd_set_string(jd_push(out, 1), rname);
    free(rname);
  }
  return out;
}

#if 0
static void test_multi(void) {
  static const char *conf[] = {
    "data/general.json",
    "data/mc.json",
    NULL
  };
  scope {
    jd_var *res = resource_list(jd_nv(), conf);
    jd_var *multi = mc_model_multi_new(jd_nv(), res);
    jd_var *m = mc_model_multi_load(jd_nv(), multi);
    ok(!strcmp(mc_model_get_str(m, "", "$.general.log") , "debug"), "general field");
    ok(!strcmp(mc_model_get_str(m, "", "$.general.work") , "temp"), "mc field");
  }
}
#endif

void test_main(void) {
  test_getters();
  /*  test_multi();*/
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
