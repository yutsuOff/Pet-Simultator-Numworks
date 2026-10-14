#include "input.h"
#include "../platform/platform.h"

#define REPEAT_DELAY_MS 350
#define REPEAT_PERIOD_MS 90

static const uint64_t repeatable =
  (1ULL << KEY_LEFT) | (1ULL << KEY_RIGHT) | (1ULL << KEY_UP) | (1ULL << KEY_DOWN);

void input_update(input_t *in, uint64_t keys, uint32_t now) {
  in->pressed = keys & ~in->held;
  if (in->pressed & repeatable) {
    in->repeat_at = now + REPEAT_DELAY_MS;
  } else if ((keys & in->held & repeatable) && (int32_t)(now - in->repeat_at) >= 0) {
    in->pressed |= keys & in->held & repeatable;
    in->repeat_at = now + REPEAT_PERIOD_MS;
  }
  in->held = keys;
}
