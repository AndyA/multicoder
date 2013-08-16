/* sequence.c */

#include <string.h>
#include <stdlib.h>
#include <jd_pretty.h>

#include "framework.h"
#include "tap.h"

#include "mc_sequence.h"

static int expect_next(mc_sequence *s, const char *want) {
  char *got = mc_sequence_format(s);
  if (!ok(strcmp(got, want) == 0, "got %s", want))
    diag("want: %s, got: %s\n", want, got);
  free(got);
  return mc_sequence_inc(s);
}

static void test_sequence(void) {
  {
    mc_sequence *s = mc_sequence_new("hd/%6a/%6a.ts");

    expect_next(s, "hd/aaaaaa/aaaaaa.ts");
    expect_next(s, "hd/aaaaaa/aaaaab.ts");
    expect_next(s, "hd/aaaaaa/aaaaac.ts");

    mc_sequence_free(s);
  }

  {
    mc_sequence *s = mc_sequence_new("%1n/%1n/%1n");

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

    mc_sequence_free(s);
  }
}

void test_main(void) {
  scope {
    test_sequence();
  }
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
