/* segname.c */

#include <string.h>
#include <stdlib.h>
#include <jd_pretty.h>

#include "framework.h"
#include "tap.h"

#include "mc_segname.h"

static int expect_next(mc_segname *s, const char *want) {
  char *got = mc_segname_format(s);
  if (!ok(strcmp(got, want) == 0, "got %s", want))
    diag("want: %s, got: %s\n", want, got);
  free(got);
  return mc_segname_inc(s);
}

static void test_segname(void) {
  {
    mc_segname *s = mc_segname_new("hd/%6d/%6d.ts");

    expect_next(s, "hd/000000/000000.ts");
    expect_next(s, "hd/000000/000001.ts");
    expect_next(s, "hd/000000/000002.ts");

    mc_segname_free(s);
  }

  {
    mc_segname *s = mc_segname_new("hd/%6d/%6d.ts");

    mc_segname_parse(s, "hd/000000/999999.ts");

    expect_next(s, "hd/000000/999999.ts");
    expect_next(s, "hd/000001/000000.ts");
    expect_next(s, "hd/000001/000001.ts");

    mc_segname_free(s);
  }

  {
    mc_segname *s = mc_segname_new("%1d/%1d/%1d");

    char want[10];
    int carry = 0;

    for (unsigned i = 0; i < 10; i++) {
      for (unsigned j = 0; j < 10; j++) {
        for (unsigned k = 0; k < 10; k++) {
          ok(carry == 0, "no previous carry");
          sprintf(want, "%d/%d/%d", i, j, k);
          carry = expect_next(s, want);
        }
      }
    }

    ok(carry == 1, "got carry");

    mc_segname_free(s);
  }
}

void test_main(void) {
  scope {
    test_segname();
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
