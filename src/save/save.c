#include "save.h"
#include <string.h>
#include "../game/game.h"
#include "../platform/platform.h"

#define SAVE_PERIOD_MS 5000
#define HEADER_TEXT "# Pet Sim : sauvegarde (ne pas modifier)\n#"
#define MAX_BINARY 640
/* worst case: every pet count on 3 varint bytes */
_Static_assert(3 + 8 + 8 + 1 + 1 + 4 + TEAM_SIZE + 1 + 3 * PET_COUNT + (PET_COUNT + 7) / 8 + 4 <= MAX_BINARY,
               "save too big");
_Static_assert(sizeof(HEADER_TEXT) + (MAX_BINARY + 2) / 3 * 4 + 2 <= SAVE_TEXT_CAPACITY, "save text too big");

/* --- Binary format, version 1 ---
 * 'P' 'S' version | coins f64 | diamonds f64 | zone u8 | zones_unlocked u8 |
 * eggs_opened u32 | team[5] u8 | pet_count u8 | owned[pet_count] varint |
 * discovered bitmap (pet_count bits) | CRC32 of all the previous bytes
 * Pets are identified by their index: new content only adds pets at the end. */

static uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  while (n--) {
    c ^= *p++;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

static uint8_t *put(uint8_t *p, const void *v, size_t n) {
  memcpy(p, v, n);
  return p + n;
}

static int encode_binary(uint8_t *out) {
  uint8_t *p = out;
  *p++ = 'P';
  *p++ = 'S';
  *p++ = SAVE_VERSION;
  p = put(p, &game.coins, 8);
  p = put(p, &game.diamonds, 8);
  *p++ = game.zone;
  *p++ = game.zones_unlocked;
  p = put(p, &game.eggs_opened, 4);
  p = put(p, game.team, TEAM_SIZE);
  *p++ = PET_COUNT;
  for (int i = 0; i < PET_COUNT; i++) {
    unsigned v = game.owned[i];
    do {
      *p++ = (uint8_t)((v & 0x7F) | (v > 0x7F ? 0x80 : 0));
      v >>= 7;
    } while (v);
  }
  p = put(p, game.discovered, (PET_COUNT + 7) / 8);
  uint32_t c = crc32(out, (size_t)(p - out));
  p = put(p, &c, 4);
  return (int)(p - out);
}

static bool decode_binary(const uint8_t *in, int n, bool *too_new) {
  *too_new = false;
  if (n < 3 + 4 || in[0] != 'P' || in[1] != 'S') return false;
  uint32_t c;
  memcpy(&c, in + n - 4, 4);
  if (crc32(in, (size_t)n - 4) != c) return false;
  if (in[2] > SAVE_VERSION) {
    *too_new = true;
    return false;
  }
  game_t g = {0};
  const uint8_t *p = in + 3, *end = in + n - 4;
  if (end - p < 8 + 8 + 1 + 1 + 4 + TEAM_SIZE + 1) return false;
  memcpy(&g.coins, p, 8), p += 8;
  memcpy(&g.diamonds, p, 8), p += 8;
  g.zone = *p++;
  g.zones_unlocked = *p++;
  memcpy(&g.eggs_opened, p, 4), p += 4;
  memcpy(g.team, p, TEAM_SIZE), p += TEAM_SIZE;
  int count = *p++;
  for (int i = 0; i < count; i++) {
    unsigned v = 0;
    for (int shift = 0;; shift += 7) {
      if (p >= end || shift > 14) return false;
      v |= (unsigned)(*p & 0x7F) << shift;
      if (!(*p++ & 0x80)) break;
    }
    if (i < PET_COUNT) g.owned[i] = (uint16_t)(v > UINT16_MAX ? UINT16_MAX : v);
  }
  int bitmap = (count + 7) / 8;
  if (end - p != bitmap) return false;
  for (int i = 0; i < count && i < PET_COUNT; i++) {
    if ((p[i / 8] >> (i % 8)) & 1) g.discovered[i / 8] |= (uint8_t)(1 << (i % 8));
  }
  /* sanity checks: a save from another content version must not crash the game */
  if (!(g.coins >= 0) || !(g.diamonds >= 0)) return false;
  if (g.zones_unlocked < 1 || g.zones_unlocked > ZONE_COUNT || g.zone >= g.zones_unlocked) return false;
  game = g;
  for (int i = 0; i < TEAM_SIZE; i++) {
    uint8_t pet = game.team[i];
    game.team[i] = NO_PET;
    if (pet < PET_COUNT && game_equipped_count(pet) < game.owned[pet]) game.team[i] = pet;
  }
  return true;
}

/* --- Text (base64) --- */

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int save_encode(char *text) {
  uint8_t bin[MAX_BINARY];
  int n = encode_binary(bin);
  char *q = text;
  for (const char *h = HEADER_TEXT; *h;) *q++ = *h++;
  for (int i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)bin[i] << 16 | (i + 1 < n ? bin[i + 1] << 8 : 0) | (i + 2 < n ? bin[i + 2] : 0);
    *q++ = b64[v >> 18];
    *q++ = b64[(v >> 12) & 63];
    *q++ = i + 1 < n ? b64[(v >> 6) & 63] : '=';
    *q++ = i + 2 < n ? b64[v & 63] : '=';
  }
  *q++ = '\n';
  *q = 0;
  return (int)(q - text);
}

static int b64_value(char c) {
  const char *f = c ? strchr(b64, c) : 0;
  return f ? (int)(f - b64) : -1;
}

bool save_decode(const char *text, size_t size, bool *too_new) {
  *too_new = false;
  size_t hl = strlen(HEADER_TEXT);
  if (size < hl || memcmp(text, HEADER_TEXT, hl) != 0) return false;
  uint8_t bin[MAX_BINARY];
  int n = 0;
  uint32_t acc = 0;
  int bits = 0;
  for (size_t i = hl; i < size; i++) {
    char c = text[i];
    if (c == '\n' || c == 0 || c == '=') break;
    int v = b64_value(c);
    if (v < 0) return false;
    acc = acc << 6 | (uint32_t)v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      if (n >= MAX_BINARY) return false;
      bin[n++] = (uint8_t)(acc >> bits);
    }
  }
  return decode_binary(bin, n, too_new);
}

/* --- Storage --- */

static storage_t storage;
static bool have_storage;
static save_status_t status = SAVE_NO_STORAGE;
static storage_result_t last_result = STORAGE_OK;
static uint32_t last_crc, last_tick;
static bool written_once;

/* Record value of a Python script: status byte (0: no auto-import in the shell),
 * text, NUL, zero padding up to the fixed capacity. */
static uint8_t record[1 + SAVE_TEXT_CAPACITY];

save_status_t save_init(void) {
  have_storage = platform_storage(&storage.buffer, &storage.size) && storage_used(&storage) >= 0;
  if (!have_storage) return status = SAVE_NO_STORAGE;
  size_t size;
  const uint8_t *value = storage_find(&storage, SAVE_NAME, &size);
  if (!value) return status = SAVE_NEW_GAME;
  bool too_new;
  if (size > 1 && save_decode((const char *)value + 1, size - 1, &too_new)) return status = SAVE_LOADED;
  return status = too_new ? SAVE_TOO_NEW : SAVE_UNREADABLE;
}

save_status_t save_status(void) { return status; }

storage_result_t save_last_result(void) { return last_result; }

storage_result_t save_now(void) {
  if (!have_storage || status == SAVE_UNREADABLE || status == SAVE_TOO_NEW) return last_result;
  memset(record, 0, sizeof(record));
  int len = save_encode((char *)record + 1);
  uint32_t c = crc32(record, (size_t)len + 1);
  if (written_once && c == last_crc) return last_result;
  last_result = storage_write(&storage, SAVE_NAME, record, sizeof(record));
  if (last_result == STORAGE_OK) {
    last_crc = c;
    written_once = true;
  }
  return last_result;
}

void save_tick(uint32_t now) {
  if ((uint32_t)(now - last_tick) < SAVE_PERIOD_MS) return;
  last_tick = now;
  save_now();
}

bool save_storage_info(uint32_t *address, long *used, long *size) {
  if (!have_storage) return false;
  *address = (uint32_t)(uintptr_t)storage.buffer;
  *used = storage_used(&storage);
  *size = (long)storage.size;
  return true;
}
