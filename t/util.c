/* util.t */

#include "framework.h"
#include "tap.h"

#include "jd_pretty.h"

#include "mc_util.h"

static void check_dirname(const char *in, const char *out) {
  char *d = mc_dirname(in);
  ok(out ? !strcmp(d, out) : !d, "got %s", d);
  if (d) free(d);
}

static void test_misc(void) {
  check_dirname("/usr/local/bin/ccache", "/usr/local/bin");
  check_dirname("ccache", NULL);
  check_dirname("/", NULL);
}

void test_main(void) {
  scope {
    test_misc();
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
