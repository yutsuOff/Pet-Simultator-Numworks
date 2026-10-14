#include <stdio.h>
#include <string.h>
#include "../src/game/numfmt.h"

static int failures;

static void check(double v, const char *expected) {
  char buf[32];
  numfmt(v, buf);
  if (strcmp(buf, expected) != 0) {
    printf("FAIL numfmt(%g) = '%s', expected '%s'\n", v, buf, expected);
    failures++;
  }
}

int main(void) {
  check(0, "0");
  check(7.9, "7");
  check(999, "999");
  check(1000, "1.00K");
  check(1234, "1.23K");
  check(1239, "1.23K"); /* truncated, not rounded */
  check(12345, "12.3K");
  check(999999, "999K");
  check(1e6, "1.00M");
  check(45.6e6, "45.6M");
  check(789e9, "789B");
  check(1e12, "1.00T");
  check(2.5e15, "2.50Qa");
  check(1e18, "1.00Qi");
  check(1e33, "1.00Dc");
  check(1e66, "1.00e66");
  check(-1500, "-1.50K");
  if (failures == 0) printf("test_numfmt: OK\n");
  return failures != 0;
}
