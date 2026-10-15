/* Epsilon storage records and save format: existing records must never move,
 * and a game must come back identical after a save. */
#include <stdio.h>
#include <string.h>
#include "../src/game/game.h"
#include "../src/save/save.h"

/* save.c asks the platform for the storage; these tests use their own buffer. */
bool platform_storage(uint8_t **records, size_t *size) { return false; }

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL " __VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint8_t area[2048];
static storage_t s = {area, sizeof(area)};

static void add(const char *name, size_t value_size, uint8_t fill) {
  uint8_t value[512];
  memset(value, fill, value_size);
  CHECK(storage_write(&s, name, value, value_size) == STORAGE_OK, "add %s", name);
}

static void test_storage(void) {
  memset(area, 0xAA, sizeof(area)); /* garbage after the end marker, like in a real storage */
  area[0] = area[1] = 0;
  CHECK(storage_used(&s) == 0, "empty storage");
  add("a.py", 10, 1);
  add("b.py", 20, 2);
  long used = storage_used(&s);
  CHECK(used == (2 + 5 + 10) + (2 + 5 + 20), "used %ld", used);
  size_t n;
  const uint8_t *b = storage_find(&s, "b.py", &n);
  CHECK(b && n == 20 && b[0] == 2 && b[19] == 2, "find b.py");
  CHECK(!storage_find(&s, "c.py", &n), "find missing");

  /* new record at the end; a.py and b.py stay where they are */
  const uint8_t *a_before = storage_find(&s, "a.py", &n);
  add("petsim.py", 100, 7);
  CHECK(storage_find(&s, "a.py", &n) == a_before && storage_find(&s, "b.py", &n) == b, "records moved");
  /* same size: rewritten in place */
  const uint8_t *p = storage_find(&s, "petsim.py", &n);
  add("petsim.py", 100, 8);
  CHECK(storage_find(&s, "petsim.py", &n) == p && p[50] == 8, "in place");
  /* last record may change size */
  add("petsim.py", 120, 9);
  CHECK(storage_find(&s, "petsim.py", &n) == p && n == 120, "resize last");
  /* not last + other size: refused, nothing changes */
  add("z.py", 5, 3);
  uint8_t before[sizeof(area)];
  memcpy(before, area, sizeof(area));
  uint8_t v[90] = {0};
  CHECK(storage_write(&s, "petsim.py", v, sizeof(v)) == STORAGE_MOVED, "moved refused");
  CHECK(memcmp(before, area, sizeof(area)) == 0, "storage changed after a refusal");
  /* full */
  uint8_t big[1900] = {0};
  CHECK(storage_write(&s, "big.py", big, sizeof(big)) == STORAGE_FULL, "full");
  CHECK(memcmp(before, area, sizeof(area)) == 0, "storage changed when full");
  /* corrupt chain: never written */
  area[0] = 1;
  CHECK(storage_used(&s) < 0, "corrupt detected");
  CHECK(storage_write(&s, "x.py", v, 1) == STORAGE_CORRUPT, "corrupt write refused");
}

static void test_format(void) {
  game_init();
  game.coins = 123456789.5;
  game.diamonds = 42;
  game.zone = 3;
  game.zones_unlocked = 5;
  game.eggs_opened = 77;
  game.owned[0][VAR_NORMAL] = 3;
  game.owned[17][VAR_NORMAL] = 300;
  game.owned[17][VAR_SHINY + VAR_RAINBOW] = 2;
  game.owned[PET_COUNT - 1][VAR_GOLDEN] = 20000; /* a huge */
  game.discovered[0] = 1;
  game.discovered[2] = 2;
  game.team[0] = (team_slot_t){17, VAR_NORMAL};
  game.team[1] = (team_slot_t){0, VAR_NORMAL};
  game.upgrade_level[UPG_TEAM] = 2;
  game.team[6] = (team_slot_t){17, VAR_SHINY + VAR_RAINBOW}; /* slot 7 exists with 2 team upgrades */
  game.upgrade_level[UPG_LUCK] = 4;
  game.egg_rewarded[1] = 0x81;
  game.potions_owned[4] = 7;
  game.enchants_owned[10] = 2;
  game.rank = 2; /* 2 enchant slots */
  game.enchant_slot[1] = 10;
  game.boost_tier[POTION_LUCKY] = 2;
  game.boost_ms[POTION_LUCKY] = 123456;
  game.next_reward = 3;
  game.star_bank = 2;
  game.quest_goal[1] = 40;
  game.quest_progress[1] = 17;
  game.rebirths = 1;
  game_t saved = game;
  char text[SAVE_TEXT_CAPACITY];
  int len = save_encode(text);
  CHECK(len > 0 && len < SAVE_TEXT_CAPACITY && (int)strlen(text) == len, "encode length %d", len);
  game_init();
  bool too_new;
  CHECK(save_decode(text, (size_t)len, &too_new), "decode");
  CHECK(memcmp(&saved, &game, sizeof(game)) == 0, "game differs after a save/load");
  /* any damaged character is detected */
  int detected = 0, total = 0;
  for (int i = 45; i < len - 1; i += 7) {
    char copy[SAVE_TEXT_CAPACITY];
    memcpy(copy, text, (size_t)len + 1);
    copy[i] = copy[i] == 'A' ? 'B' : 'A';
    total++;
    detected += !save_decode(copy, (size_t)len, &too_new);
  }
  CHECK(detected == total, "%d/%d damaged saves detected", detected, total);
  CHECK(memcmp(&saved, &game, sizeof(game)) == 0, "a damaged save changed the game");
  printf("test_save: save text %d characters\n", len);
}

/* A save written by v0.3 (format 1) must still load. */
static uint32_t crc32_ref(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  while (n--) {
    c ^= *p++;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
  }
  return ~c;
}

static void test_version1(void) {
  uint8_t bin[600], *p = bin;
  double coins = 5000, diamonds = 12;
  uint32_t opened = 9;
  *p++ = 'P', *p++ = 'S', *p++ = 1;
  memcpy(p, &coins, 8), p += 8;
  memcpy(p, &diamonds, 8), p += 8;
  *p++ = 1, *p++ = 2;
  memcpy(p, &opened, 4), p += 4;
  uint8_t team[5] = {2, 2, NO_PET, NO_PET, NO_PET};
  memcpy(p, team, 5), p += 5;
  *p++ = 4; /* an older content with 4 pets */
  *p++ = 0, *p++ = 0, *p++ = 3, *p++ = 1;
  *p++ = 0x0C; /* pets 2 and 3 discovered */
  uint32_t c = crc32_ref(bin, (size_t)(p - bin));
  memcpy(p, &c, 4), p += 4;
  static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char text[SAVE_TEXT_CAPACITY] = "# Pet Sim : sauvegarde (ne pas modifier)\n#";
  char *q = text + strlen(text);
  int n = (int)(p - bin);
  for (int i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)bin[i] << 16 | (i + 1 < n ? bin[i + 1] << 8 : 0) | (i + 2 < n ? bin[i + 2] : 0);
    *q++ = b64[v >> 18], *q++ = b64[(v >> 12) & 63];
    *q++ = i + 1 < n ? b64[(v >> 6) & 63] : '=';
    *q++ = i + 2 < n ? b64[v & 63] : '=';
  }
  *q = 0;
  game_init();
  bool too_new;
  CHECK(save_decode(text, strlen(text), &too_new), "version 1 save not loaded");
  CHECK(game.coins == 5000 && game.diamonds == 12 && game.zone == 1 && game.zones_unlocked == 2, "v1 values");
  CHECK(game.owned[2][VAR_NORMAL] == 3 && game.owned[3][VAR_NORMAL] == 1 && game_discovered(2) && !game_discovered(0),
        "v1 pets");
  CHECK(game.team[0].pet == 2 && game.team[1].pet == 2 && game.team[0].variant == VAR_NORMAL && game.team[2].pet == NO_PET,
        "v1 team");
  CHECK(game.rank == 0 && game.rebirths == 0 && game.enchant_slot[0] == NO_ITEM && game.quest_goal[0] == NO_QUEST, "v1 defaults");
  CHECK(game.upgrade_level[UPG_DAMAGE] == 0, "v1 upgrades");
}

int main(void) {
  test_storage();
  test_format();
  test_version1();
  if (failures == 0) printf("test_save: OK\n");
  return failures != 0;
}
