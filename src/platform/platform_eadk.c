#include <eadk.h>
#include "platform.h"

#define FRAME_MS 33

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Pet Sim";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

static uint32_t frame_start;

void platform_init(void) { frame_start = (uint32_t)eadk_timing_millis(); }

void platform_push_rect(int x, int y, int w, int h, const uint16_t *pixels) {
  eadk_display_push_rect((eadk_rect_t){(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h}, pixels);
}

uint64_t platform_keys(void) { return eadk_keyboard_scan(); }

uint32_t platform_millis(void) { return (uint32_t)eadk_timing_millis(); }

uint32_t platform_random(void) { return eadk_random(); }

bool platform_end_frame(void) {
  uint32_t elapsed = platform_millis() - frame_start;
  if (elapsed < FRAME_MS) {
    eadk_timing_msleep(FRAME_MS - elapsed);
  }
  frame_start = platform_millis();
  return true;
}

bool platform_debug_rich(void) { return false; }

extern int app_main(void);

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return app_main();
}
