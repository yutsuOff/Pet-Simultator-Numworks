#ifndef PLATFORM_H
#define PLATFORM_H

/* Hardware abstraction: the game never calls EADK directly.
 * platform_eadk.c targets the calculator, platform_host.c a PC (tests, screenshots). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 240

/* Key bits, same numbering as eadk_key_t. */
enum {
  KEY_LEFT = 0, KEY_UP = 1, KEY_DOWN = 2, KEY_RIGHT = 3,
  KEY_OK = 4, KEY_BACK = 5, KEY_HOME = 6,
  KEY_SHIFT = 12, KEY_ALPHA = 13, KEY_XNT = 14, KEY_VAR = 15, KEY_TOOLBOX = 16,
  KEY_BACKSPACE = 17,
  KEY_7 = 30, KEY_8 = 31, KEY_9 = 32,
  KEY_4 = 36, KEY_5 = 37, KEY_6 = 38,
  KEY_1 = 42, KEY_2 = 43, KEY_3 = 44,
  KEY_0 = 48, KEY_EXE = 52
};

void platform_init(void);
void platform_push_rect(int x, int y, int w, int h, const uint16_t *pixels);
uint64_t platform_keys(void);
uint32_t platform_millis(void);
uint32_t platform_random(void);
/* Called once per frame: paces the loop. Returns false when the app must quit. */
bool platform_end_frame(void);
/* Epsilon's storage (Python scripts): the record area, after its magic header.
 * Returns false if it cannot be found safely (saving is then disabled). */
bool platform_storage(uint8_t **records, size_t *size);
/* PC screenshots only (petsim-host --rich): start with coins to show a later game. */
bool platform_debug_rich(void);

#endif
