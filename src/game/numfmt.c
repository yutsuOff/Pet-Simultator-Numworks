#include "numfmt.h"

static const char *const suffixes[] = {
  "", "K", "M", "B", "T", "Qa", "Qi", "Sx", "Sp", "Oc", "No", "Dc",
  "Ud", "Dd", "Td", "Qad", "Qid", "Sxd", "Spd", "Ocd", "Nod", "Vg"
};
#define SUFFIX_COUNT (int)(sizeof(suffixes) / sizeof(suffixes[0]))

static char *put_uint(char *p, unsigned v) {
  char tmp[12];
  int n = 0;
  do {
    tmp[n++] = (char)('0' + v % 10);
    v /= 10;
  } while (v);
  while (n) *p++ = tmp[--n];
  return p;
}

static char *put_str(char *p, const char *s) {
  while (*s) *p++ = *s++;
  return p;
}

char *numfmt(double value, char *buf) {
  char *p = buf;
  if (value != value) { /* NaN */
    put_str(p, "?")[0] = 0;
    return buf;
  }
  if (value < 0) {
    *p++ = '-';
    value = -value;
  }
  if (value < 1000.0) {
    put_uint(p, (unsigned)value)[0] = 0;
    return buf;
  }
  int group = 0;
  double m = value;
  while (m >= 999.999999999 && group < 1000) {
    m /= 1000.0;
    group++;
  }
  /* Guard against floating point noise just below a power of 1000. */
  if (m < 1.0) m = 1.0;
  /* Three significant digits, truncated. */
  unsigned digits;
  int decimals;
  if (m >= 100.0) {
    digits = (unsigned)m;
    decimals = 0;
  } else if (m >= 10.0) {
    digits = (unsigned)(m * 10.0 + 1e-9);
    decimals = 1;
  } else {
    digits = (unsigned)(m * 100.0 + 1e-9);
    decimals = 2;
  }
  if (digits >= 1000) digits = 999; /* m just below 1000 after rounding noise */
  unsigned scale = decimals == 2 ? 100 : decimals == 1 ? 10 : 1;
  p = put_uint(p, digits / scale);
  if (decimals) {
    *p++ = '.';
    unsigned frac = digits % scale;
    if (decimals == 2 && frac < 10) *p++ = '0';
    p = put_uint(p, frac);
  }
  if (group < SUFFIX_COUNT) {
    p = put_str(p, suffixes[group]);
  } else {
    p = put_str(p, "e");
    p = put_uint(p, (unsigned)(group * 3));
  }
  *p = 0;
  return buf;
}
