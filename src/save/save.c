#include "save.h"
#include <string.h>
#include "../game/game.h"
#include "../platform/platform.h"

#define SAVE_PERIOD_MS 5000
#define HEADER_TEXT "# Pet Sim : sauvegarde (ne pas modifier)\n#"
#define MAX_BINARY 3000
/* worst case: every count on 3 varint bytes */
_Static_assert(40 + 2 * MAX_TEAM + 3 * PET_COUNT * VARIANT_COUNT + (PET_COUNT + 7) / 8 + UPG_COUNT + EGG_COUNT / 8 +
               3 * (POTION_COUNT + ENCHANT_COUNT) + MAX_ENCHANT_SLOTS + 5 * POTION_TYPE_COUNT + 6 * QUEST_SLOTS + 16 <=
               MAX_BINARY, "save too big");
_Static_assert(sizeof(HEADER_TEXT) + (MAX_BINARY + 2) / 3 * 4 + 2 <= SAVE_TEXT_CAPACITY, "save text too big");

/* --- Binary format ---
 * 'P' 'S' version | coins f64 | diamonds f64 | zone u8 | zones_unlocked u8 | eggs_opened u32 |
 * v1: team[5] (pets)     v2: team_count | team pets     v3: team_count | team (pet, variant)
 * pet_count u8 | v3: variant_count u8 |
 * owned: v1-2 varint per pet (normal); v3 per pet and variant, run-length: 0, run | n |
 * discovered bitmap (pet_count bits) |
 * v2+: upgrade_count | levels | egg_count | egg_rewarded bitmap |
 * v3: potion_count | potions varint | enchant_count | enchants varint | slot_count | slots |
 *     boost_count | (tier u8, ms u32) | rank | next_reward | star_bank u16 |
 *     quest_count | (goal u16, progress u32) | rebirths |
 * CRC32 of all the previous bytes.
 * Pets, eggs, upgrades and items are identified by their index: new content is only
 * added at the end, and counts are stored so an older save still loads. */

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

static uint8_t *put_varint(uint8_t *p, uint32_t v) {
  do {
    *p++ = (uint8_t)((v & 0x7F) | (v > 0x7F ? 0x80 : 0));
    v >>= 7;
  } while (v);
  return p;
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
  *p++ = MAX_TEAM;
  for (int i = 0; i < MAX_TEAM; i++) *p++ = game.team[i].pet, *p++ = game.team[i].variant;
  *p++ = PET_COUNT;
  *p++ = VARIANT_COUNT;
  const uint16_t *counts = &game.owned[0][0];
  for (int i = 0; i < PET_COUNT * VARIANT_COUNT;) {
    if (counts[i]) {
      p = put_varint(p, counts[i++]);
      continue;
    }
    int run = 0;
    while (i < PET_COUNT * VARIANT_COUNT && !counts[i]) run++, i++;
    *p++ = 0;
    p = put_varint(p, (uint32_t)run);
  }
  p = put(p, game.discovered, (PET_COUNT + 7) / 8);
  *p++ = UPG_COUNT;
  p = put(p, game.upgrade_level, UPG_COUNT);
  *p++ = EGG_COUNT;
  p = put(p, game.egg_rewarded, (EGG_COUNT + 7) / 8);
  *p++ = POTION_COUNT;
  for (int i = 0; i < POTION_COUNT; i++) p = put_varint(p, game.potions_owned[i]);
  *p++ = ENCHANT_COUNT;
  for (int i = 0; i < ENCHANT_COUNT; i++) p = put_varint(p, game.enchants_owned[i]);
  *p++ = MAX_ENCHANT_SLOTS;
  p = put(p, game.enchant_slot, MAX_ENCHANT_SLOTS);
  *p++ = POTION_TYPE_COUNT;
  for (int i = 0; i < POTION_TYPE_COUNT; i++) *p++ = game.boost_tier[i], p = put(p, &game.boost_ms[i], 4);
  *p++ = game.rank;
  *p++ = game.next_reward;
  p = put(p, &game.star_bank, 2);
  *p++ = QUEST_SLOTS;
  for (int i = 0; i < QUEST_SLOTS; i++) p = put(p, &game.quest_goal[i], 2), p = put(p, &game.quest_progress[i], 4);
  *p++ = game.rebirths;
  uint32_t c = crc32(out, (size_t)(p - out));
  p = put(p, &c, 4);
  return (int)(p - out);
}

/* Reads `count` bits into dest (keeping at most `keep` of them). */
static const uint8_t *get_bits(const uint8_t *p, int count, uint8_t *dest, int keep) {
  for (int i = 0; i < count && i < keep; i++) {
    if ((p[i / 8] >> (i % 8)) & 1) dest[i / 8] |= (uint8_t)(1 << (i % 8));
  }
  return p + (count + 7) / 8;
}

/* Reader with bounds checks: any overflow makes the whole save invalid. */
typedef struct {
  const uint8_t *p, *end;
  bool bad;
} reader_t;

static uint32_t rd_u8(reader_t *r) {
  if (r->p >= r->end) return r->bad = true, 0;
  return *r->p++;
}

static void rd(reader_t *r, void *dest, size_t n) {
  if ((size_t)(r->end - r->p) < n) {
    r->bad = true;
    memset(dest, 0, n);
    return;
  }
  memcpy(dest, r->p, n);
  r->p += n;
}

static uint32_t rd_varint(reader_t *r) {
  uint32_t v = 0;
  for (int shift = 0; shift <= 28; shift += 7) {
    uint32_t b = rd_u8(r);
    v |= (b & 0x7F) << shift;
    if (!(b & 0x80)) return v;
  }
  r->bad = true;
  return 0;
}

static uint16_t clamp16(uint32_t v) { return (uint16_t)(v > UINT16_MAX ? UINT16_MAX : v); }

static bool decode_binary(const uint8_t *in, int n, bool *too_new) {
  *too_new = false;
  if (n < 3 + 4 || in[0] != 'P' || in[1] != 'S') return false;
  uint32_t c;
  memcpy(&c, in + n - 4, 4);
  if (crc32(in, (size_t)n - 4) != c) return false;
  int version = in[2];
  if (version > SAVE_VERSION) {
    *too_new = true;
    return false;
  }
  static game_t g; /* large: not on the stack */
  g = (game_t){0};
  for (int i = 0; i < MAX_TEAM; i++) g.team[i].pet = NO_PET;
  for (int i = 0; i < MAX_ENCHANT_SLOTS; i++) g.enchant_slot[i] = NO_ITEM;
  for (int i = 0; i < QUEST_SLOTS; i++) g.quest_goal[i] = NO_QUEST;
  reader_t r = {in + 3, in + n - 4, false};
  rd(&r, &g.coins, 8);
  rd(&r, &g.diamonds, 8);
  g.zone = (uint8_t)rd_u8(&r);
  g.zones_unlocked = (uint8_t)rd_u8(&r);
  rd(&r, &g.eggs_opened, 4);
  int team = version == 1 ? 5 : (int)rd_u8(&r);
  for (int i = 0; i < team; i++) {
    uint8_t pet = (uint8_t)rd_u8(&r), variant = version >= 3 ? (uint8_t)rd_u8(&r) : 0;
    if (i < MAX_TEAM) g.team[i] = (team_slot_t){pet, variant};
  }
  int pet_count = (int)rd_u8(&r);
  if (version < 3) {
    for (int i = 0; i < pet_count; i++) {
      uint32_t v = rd_varint(&r);
      if (i < PET_COUNT) g.owned[i][VAR_NORMAL] = clamp16(v);
    }
  } else {
    int variants = (int)rd_u8(&r);
    for (int i = 0; i < pet_count * variants && !r.bad;) {
      uint32_t v = rd_varint(&r);
      if (v) {
        if (i / variants < PET_COUNT && i % variants < VARIANT_COUNT) g.owned[i / variants][i % variants] = clamp16(v);
        i++;
      } else {
        uint32_t run = rd_varint(&r);
        if (!run || run > (uint32_t)(pet_count * variants - i)) r.bad = true;
        i += (int)run;
      }
    }
  }
  if (r.end - r.p < (pet_count + 7) / 8) return false;
  r.p = get_bits(r.p, pet_count, g.discovered, PET_COUNT);
  if (version >= 2) {
    int k = (int)rd_u8(&r);
    for (int i = 0; i < k; i++) {
      uint32_t lv = rd_u8(&r);
      if (i < UPG_COUNT) g.upgrade_level[i] = (uint8_t)(lv <= upgrades[i].max_level ? lv : upgrades[i].max_level);
    }
    int eggs_saved = (int)rd_u8(&r);
    if (r.end - r.p < (eggs_saved + 7) / 8) return false;
    r.p = get_bits(r.p, eggs_saved, g.egg_rewarded, EGG_COUNT);
  }
  if (version >= 3) {
    int k = (int)rd_u8(&r);
    for (int i = 0; i < k; i++) {
      uint32_t v = rd_varint(&r);
      if (i < POTION_COUNT) g.potions_owned[i] = clamp16(v);
    }
    k = (int)rd_u8(&r);
    for (int i = 0; i < k; i++) {
      uint32_t v = rd_varint(&r);
      if (i < ENCHANT_COUNT) g.enchants_owned[i] = clamp16(v);
    }
    k = (int)rd_u8(&r);
    for (int i = 0; i < k; i++) {
      uint32_t e = rd_u8(&r);
      if (i < MAX_ENCHANT_SLOTS) g.enchant_slot[i] = (uint8_t)(e < ENCHANT_COUNT ? e : NO_ITEM);
    }
    k = (int)rd_u8(&r);
    for (int i = 0; i < k; i++) {
      uint32_t tier = rd_u8(&r), ms;
      rd(&r, &ms, 4);
      if (i < POTION_TYPE_COUNT && tier <= 3) g.boost_tier[i] = (uint8_t)tier, g.boost_ms[i] = tier ? ms : 0;
    }
    g.rank = (uint8_t)rd_u8(&r);
    g.next_reward = (uint8_t)rd_u8(&r);
    rd(&r, &g.star_bank, 2);
    k = (int)rd_u8(&r);
    for (int i = 0; i < k; i++) {
      uint16_t goal;
      uint32_t progress;
      rd(&r, &goal, 2);
      rd(&r, &progress, 4);
      if (i < QUEST_SLOTS) g.quest_goal[i] = goal < RANK_GOAL_COUNT ? goal : NO_QUEST, g.quest_progress[i] = progress;
    }
    g.rebirths = (uint8_t)rd_u8(&r);
  }
  if (r.bad || r.p != r.end) return false;
  /* sanity checks: a save from another content version must not crash the game */
  if (!(g.coins >= 0) || !(g.diamonds >= 0)) return false;
  if (g.zones_unlocked < 1 || g.zones_unlocked > ZONE_COUNT || g.zone >= g.zones_unlocked) return false;
  if (g.rank >= RANK_COUNT) g.rank = RANK_COUNT - 1, g.next_reward = 0;
  if (g.next_reward > ranks[g.rank].reward_count) g.next_reward = ranks[g.rank].reward_count;
  if (g.rebirths > MAX_REBIRTH) g.rebirths = MAX_REBIRTH;
  game = g;
  team_slot_t saved_team[MAX_TEAM];
  memcpy(saved_team, game.team, sizeof(saved_team));
  for (int i = 0; i < MAX_TEAM; i++) game.team[i].pet = NO_PET;
  for (int i = 0; i < game_team_slots(); i++) {
    team_slot_t t = saved_team[i];
    if (t.pet < PET_COUNT && t.variant < VARIANT_COUNT && game_equipped_count(t.pet, t.variant) < game.owned[t.pet][t.variant]) {
      game.team[i] = t;
    }
  }
  uint8_t slots[MAX_ENCHANT_SLOTS];
  memcpy(slots, game.enchant_slot, sizeof(slots));
  for (int i = 0; i < MAX_ENCHANT_SLOTS; i++) game.enchant_slot[i] = NO_ITEM;
  for (int i = 0; i < game_enchant_slots(); i++) {
    if (slots[i] != NO_ITEM && game_enchant_equipped(slots[i]) < game.enchants_owned[slots[i]]) {
      game.enchant_slot[i] = slots[i];
    }
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
 * text, NUL, zero padding up to the size of the record. */
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
  /* keep the size of the existing record if the text fits (rewritten in place) */
  size_t needed = (size_t)len + 2, size = SAVE_MIN_RECORD + 1, existing;
  if (storage_find(&storage, SAVE_NAME, &existing)) size = existing;
  if (size < needed) size = (needed + 511) / 512 * 512;
  if (size > sizeof(record)) {
    last_result = STORAGE_FULL;
    return last_result;
  }
  last_result = storage_write(&storage, SAVE_NAME, record, size);
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
