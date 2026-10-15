/* Packed sprites (DEFLATE, src/gfx/inflate.c): every image of the content decodes to
 * width x height palette indices, and damaged data is refused. */
#include <stdio.h>
#include "../src/generated/content.h"
#include "../src/gfx/inflate.h"

static int failures, checked;

static void check(const sprite_t *s, const char *what, int i) {
  uint8_t out[SPRITE_MAX_PACKED_PIXELS];
  int n = s->width * s->height;
  checked++;
  if (!s->packed || inflate_raw(s->pixels, s->packed, out, sizeof(out)) != n) {
    printf("FAIL %s %d does not decode\n", what, i);
    failures++;
    return;
  }
  for (int k = 0; k < n; k++) {
    if (out[k] >= s->palette_size) {
      printf("FAIL %s %d: color %d outside the palette\n", what, i, out[k]);
      failures++;
      return;
    }
  }
}

int main(void) {
  for (int i = 0; i < PET_COUNT; i++) check(pets[i].sprite, "pet", i);
  for (int i = 0; i < EGG_COUNT; i++) check(eggs[i].sprite, "egg", i);
  for (int i = 0; i < ZONE_COUNT; i++) check(zones[i].thumb, "zone", i);
  for (int i = 0; i < POTION_COUNT; i++) check(potions[i].sprite, "potion", i);
  for (int i = 0; i < ENCHANT_COUNT; i++) check(enchants[i].sprite, "enchant", i);
  /* damaged or too large: refused, never written past the end */
  const sprite_t *s = pets[0].sprite;
  uint8_t small[100], copy[4096];
  if (inflate_raw(s->pixels, s->packed, small, sizeof(small)) != -1) printf("FAIL output overflow\n"), failures++;
  if (inflate_raw(s->pixels, s->packed / 2, copy, sizeof(copy)) != -1) printf("FAIL truncated data\n"), failures++;
  if (failures == 0) printf("test_sprites: %d images OK\n", checked);
  return failures != 0;
}
