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

/* --- Epsilon's storage ---
 * Epsilon writes a "userland header" at the start of its userland, in each of the
 * two firmware slots of the external flash (Epsilon sources:
 * shared/ion/src/device/shared/drivers/userland_header.h and include/n01xx/config/board.h).
 * It gives the RAM address of the storage and the flash range of the external apps.
 * The right slot is the one whose external apps range contains this app's code. */

#define USERLAND_MAGIC 0xDEC0EDFEu
#define STORAGE_MAGIC 0xEE0BDDBAu

typedef struct {
  uint32_t header;
  char expected_version[8];
  uint32_t storage_address;
  uint32_t storage_size;
  uint32_t apps_flash_start, apps_flash_end;
  uint32_t apps_ram_start, apps_ram_end;
  uint32_t device_name_start, device_name_end;
  uint32_t footer;
} userland_header_t;

static bool in_sram(uint32_t a, uint32_t size) {
  /* N0110/N0115: 0x20000000, N0120: 0x24000000, 256 KiB each */
  static const uint32_t origins[] = {0x20000000u, 0x24000000u};
  for (int i = 0; i < 2; i++) {
    if (a >= origins[i] && size <= 0x40000u && a - origins[i] <= 0x40000u - size) return true;
  }
  return false;
}

bool platform_storage(uint8_t **records, size_t *size) {
  /* slot A / B, kernel 64 KiB, then optional 64 KiB of extra data */
  static const uint32_t candidates[] = {0x90010000u, 0x90020000u, 0x90410000u, 0x90420000u};
  uint32_t self = (uint32_t)(uintptr_t)&platform_storage;
  for (int i = 0; i < 4; i++) {
    const userland_header_t *h = (const userland_header_t *)candidates[i];
    if (h->header != USERLAND_MAGIC || h->footer != USERLAND_MAGIC) continue;
    if (self < h->apps_flash_start || self >= h->apps_flash_end) continue;
    uint32_t a = h->storage_address, n = h->storage_size;
    if (n < 1024 || n > 0x10000 || !in_sram(a, n + 8)) return false;
    if (*(const uint32_t *)a != STORAGE_MAGIC || *(const uint32_t *)(a + 4 + n) != STORAGE_MAGIC) return false;
    *records = (uint8_t *)(a + 4);
    *size = n;
    return true;
  }
  return false;
}

extern int app_main(void);

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return app_main();
}
