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
  game.owned[0] = 3;
  game.owned[17] = 300;
  game.owned[PET_COUNT - 1] = 20000;
  game.discovered[0] = 1;
  game.discovered[2] = 2;
  game.team[0] = 17;
  game.team[1] = 0;
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

int main(void) {
  test_storage();
  test_format();
  if (failures == 0) printf("test_save: OK\n");
  return failures != 0;
}
