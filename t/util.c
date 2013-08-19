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

static void test_dirname(void) {
  check_dirname("/usr/local/bin/ccache", "/usr/local/bin");
  check_dirname("ccache", NULL);
  check_dirname("/", NULL);
}

static void check_prefix(const char *in, const char *pfx, const char *out) {
  char *d = mc_prefix(in, pfx);
  ok(out ? !strcmp(d, out) : !d, "got %s", d);
  if (d) free(d);
}

static void test_prefix(void) {
  check_prefix("foo", NULL, "foo");
  check_prefix("foo", "", "foo");
  check_prefix("foo", "bar", "bar/foo");
  check_prefix("foo", "bar/", "bar/foo");
}


void test_main(void) {
  scope {
    test_dirname();
    test_prefix();
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
