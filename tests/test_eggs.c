/* Checks the generated content and the egg draw: data integrity, then millions of
 * hatches per egg compared with the chances shown in the game. */
#include <math.h>
#include <stdio.h>
#include "../src/game/game.h"

static int failures;
static uint64_t rng = 0x9E3779B97F4A7C15ULL;

static uint32_t rand32(void) { /* xorshift64* */
  rng ^= rng >> 12;
  rng ^= rng << 25;
  rng ^= rng >> 27;
  return (uint32_t)((rng * 0x2545F4914F6CDD1DULL) >> 32);
}

#define FAIL(...) do { printf("FAIL " __VA_ARGS__); printf("\n"); failures++; } while (0)

static void check_content(void) {
  for (int e = 0; e < EGG_COUNT; e++) {
    uint64_t sum = 0;
    for (int s = eggs[e].first_slot; s < eggs[e].first_slot + eggs[e].slot_count; s++) {
      sum += egg_slots[s].weight;
      if (egg_slots[s].pet >= PET_COUNT) FAIL("egg %s: bad pet index", eggs[e].name);
    }
    if (sum != EGG_WEIGHT_TOTAL) FAIL("egg %s: weights sum to %llu", eggs[e].name, (unsigned long long)sum);
    if (e > 0 && eggs[e].cost <= eggs[e - 1].cost) FAIL("egg %s: not pricier than the previous one", eggs[e].name);
    if (eggs[e].zone >= ZONE_COUNT) FAIL("egg %s: bad zone", eggs[e].name);
    if (eggs[e].number != e + 1) FAIL("egg %s: number %d", eggs[e].name, eggs[e].number);
  }
  for (int z = 0; z < ZONE_COUNT; z++) {
    if (z > 0 && zones[z].cost <= zones[z - 1].cost) FAIL("zone %s: not pricier", zones[z].name);
    if (zones[z].egg_count == 0) FAIL("zone %s: no egg", zones[z].name);
    for (int i = 0; i < zones[z].egg_count; i++) {
      if (eggs[zones[z].first_egg + i].zone != z) FAIL("zone %s: wrong egg list", zones[z].name);
    }
  }
  for (int p = 0; p < PET_COUNT; p++) {
    if (pets[p].sprite->width != 48 || pets[p].sprite->height != 48) FAIL("pet %s: sprite size", pets[p].name);
    if (!(pets[p].power > 0)) FAIL("pet %s: power", pets[p].name);
    int found = 0;
    for (int s = 0; s < EGG_SLOT_COUNT; s++) found |= egg_slots[s].pet == p;
    if (!found) FAIL("pet %s: in no egg", pets[p].name);
  }
}

static void check_draws(int draws) {
  int counts[8];
  double worst = 0;
  for (int e = 0; e < EGG_COUNT; e++) {
    const egg_t *egg = &eggs[e];
    for (int i = 0; i < egg->slot_count; i++) counts[i] = 0;
    for (int n = 0; n < draws; n++) counts[egg_roll(e, rand32) - egg->first_slot]++;
    for (int i = 0; i < egg->slot_count; i++) {
      double p = egg_slot_chance(egg->first_slot + i);
      double expected = p * draws;
      double sigma = sqrt(draws * p * (1 - p));
      double z = (counts[i] - expected) / sigma;
      if (fabs(z) > worst) worst = fabs(z);
      if (fabs(z) > 5.5) {
        FAIL("egg %s, pet %s: %d hatches, expected %.0f (z = %.1f)", egg->name,
             pets[egg_slots[egg->first_slot + i].pet].name, counts[i], expected, z);
      }
    }
  }
  printf("test_eggs: %d eggs x %d hatches, largest deviation %.2f sigma\n", EGG_COUNT, draws, worst);
}

static void check_game(void) {
  game_init();
  bool is_new;
  if (game_open_egg(1, rand32, &is_new) >= 0) FAIL("egg 2 opened without enough coins");
  int pet = game_open_egg(0, rand32, &is_new);
  if (pet < 0 || !is_new || game.owned[pet] != 1 || game.team[0] != pet) FAIL("first egg");
  if (game.coins != START_COINS - eggs[0].cost) FAIL("coins after the first egg");
  if (game_equip(pet)) FAIL("equipped a copy that is not owned");
  game.owned[pet] = 9;
  for (int i = 0; i < 4; i++) game_equip(pet);
  if (game_team_size() != BASE_TEAM || game_equip(pet)) FAIL("team size limit");
  game.coins = 1e30;
  for (int z = 1; z < ZONE_COUNT; z++) {
    if (!game_unlock_next_zone()) FAIL("zone %d not unlocked", z);
  }
  if (game_unlock_next_zone()) FAIL("unlocked past the last zone");
  if (!game_egg_available(EGG_COUNT - 1)) FAIL("last egg unavailable");
}

/* Luck: chances still sum to 1, rare slots grow, and the draw follows them. */
static void check_luck(void) {
  game_init();
  game.upgrade_level[UPG_LUCK] = upgrades[UPG_LUCK].max_level;
  for (int e = 0; e < EGG_COUNT; e++) {
    double sum = 0;
    for (int s = eggs[e].first_slot; s < eggs[e].first_slot + eggs[e].slot_count; s++) {
      sum += egg_slot_chance(s);
      double base = (double)egg_slots[s].weight / EGG_WEIGHT_TOTAL;
      if (base < 0.2 && egg_slot_chance(s) <= base) FAIL("egg %s: luck does not help a rare pet", eggs[e].name);
    }
    if (fabs(sum - 1) > 1e-9) FAIL("egg %s: chances sum to %.12f with luck", eggs[e].name, sum);
  }
  check_draws(500000);
  game_init();
}

static void check_progression(void) {
  game_init();
  game.diamonds = 1e9;
  for (int id = 0; id < UPG_COUNT; id++) {
    double last = 0;
    while (game.upgrade_level[id] < upgrades[id].max_level) {
      double c = upgrade_cost(id);
      if (c <= last) FAIL("upgrade %s: cost does not grow", upgrades[id].name);
      last = c;
      if (!game_buy_upgrade(id)) FAIL("upgrade %s not bought", upgrades[id].name);
    }
    if (game_buy_upgrade(id)) FAIL("upgrade %s bought past its maximum", upgrades[id].name);
  }
  if (game_team_slots() != MAX_TEAM || game_hatch_count() != MAX_HATCH) FAIL("team / hatch upgrades");
  if (game_respawn_ms() < 500) FAIL("respawn too short");
  /* recycling keeps one copy and the equipped ones */
  game.owned[0] = 5;
  game.owned[1] = 1;
  game_equip(0);
  game_equip(0);
  int n;
  double before = game.coins, coins = game_recycle_all(&n);
  if (n != 3 || game.owned[0] != 2 || game.owned[1] != 1) FAIL("recycle counts (%d)", n);
  if (!(coins > 0) || game.coins != before + coins) FAIL("recycle coins");
  /* index rewards: once per completed egg */
  game_init();
  for (int s = eggs[0].first_slot; s < eggs[0].first_slot + eggs[0].slot_count; s++) {
    int p = egg_slots[s].pet;
    game.discovered[p / 8] |= (uint8_t)(1 << (p % 8));
  }
  if (game_claim_index_rewards() != game_egg_reward(0)) FAIL("index reward of the first egg");
  if (game_claim_index_rewards() != 0) FAIL("index reward given twice");
}

int main(void) {
  check_content();
  check_draws(2000000);
  check_luck();
  check_game();
  check_progression();
  if (failures == 0) printf("test_eggs: OK\n");
  return failures != 0;
}
