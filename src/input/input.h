#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>

/* Keyboard state with edge detection and auto-repeat on the arrow keys. */
typedef struct {
  uint64_t held;    /* keys currently down */
  uint64_t pressed; /* keys that went down this frame (or auto-repeated) */
  uint32_t repeat_at;
} input_t;

void input_update(input_t *in, uint64_t keys, uint32_t now_ms);

static inline bool input_pressed(const input_t *in, int key) { return (in->pressed >> key) & 1; }
static inline bool input_held(const input_t *in, int key) { return (in->held >> key) & 1; }

#endif
