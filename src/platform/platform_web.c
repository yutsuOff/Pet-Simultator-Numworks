/* Web backend (WebAssembly, web/index.html): the page calls web_init() once, then
 * web_frame() on every animation frame with the time and the keys held. The screen is
 * a 320x240 RGB565 framebuffer that the page copies into a canvas; the Epsilon storage
 * (Python scripts + petsim.py) is emulated in memory and kept by the page in the
 * browser (localStorage), so the save survives a reload. */
#include <string.h>
#include "platform.h"

#define EXPORT(name) __attribute__((export_name(name)))

static uint16_t framebuffer[SCREEN_W * SCREEN_H];
static uint32_t now_ms;
static uint64_t keys;
static uint32_t rng_state = 0x12345678;
static bool dirty; /* framebuffer changed since the last web_frame */

/* Emulated Epsilon storage: | magic | records (42 KiB, like the calculator) | magic | */
#define STORAGE_SIZE (42 * 1024)
#define STORAGE_MAGIC 0xEE0BDDBAu
static uint8_t storage[4 + STORAGE_SIZE + 4];

static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint32_t get32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

void platform_init(void) {}

void platform_push_rect(int x, int y, int w, int h, const uint16_t *pixels) {
  for (int j = 0; j < h; j++) {
    memcpy(&framebuffer[(y + j) * SCREEN_W + x], &pixels[j * w], (size_t)w * sizeof(uint16_t));
  }
  dirty = true;
}

uint64_t platform_keys(void) { return keys; }
uint32_t platform_millis(void) { return now_ms; }

uint32_t platform_random(void) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

bool platform_end_frame(void) { return true; }

bool platform_storage(uint8_t **records, size_t *size) {
  *records = storage + 4;
  *size = STORAGE_SIZE;
  return true;
}

bool platform_debug_rich(void) { return false; }
bool platform_debug_huge(void) { return false; }

extern void app_init(void);
extern bool app_step(void);
extern void app_quit(void);

/* --- Interface with the page --- */

EXPORT("web_framebuffer") uint16_t *web_framebuffer(void) { return framebuffer; }
EXPORT("web_storage") uint8_t *web_storage(void) { return storage; }
EXPORT("web_storage_size") uint32_t web_storage_size(void) { return sizeof(storage); }

/* The page may have copied a saved storage into web_storage() before this call. */
EXPORT("web_init") void web_init(uint32_t seed, uint32_t now) {
  if (get32(storage) != STORAGE_MAGIC || get32(storage + 4 + STORAGE_SIZE) != STORAGE_MAGIC) {
    memset(storage, 0, sizeof(storage)); /* an empty calculator */
    put32(storage, STORAGE_MAGIC);
    put32(storage + 4 + STORAGE_SIZE, STORAGE_MAGIC);
  }
  rng_state = seed ? seed : 0x12345678;
  now_ms = now;
  app_init();
}

/* One frame. low / high: the 64 key bits. Returns 1 if the screen changed, 0 if not,
 * -1 when the player quit (Home). */
EXPORT("web_frame") int web_frame(uint32_t now, uint32_t low, uint32_t high) {
  now_ms = now;
  keys = (uint64_t)high << 32 | low;
  dirty = false;
  if (!app_step()) {
    app_quit();
    return -1;
  }
  return dirty;
}

/* Saves now (the page calls it when it is closed or hidden). */
EXPORT("web_save") void web_save(void) { app_quit(); }
