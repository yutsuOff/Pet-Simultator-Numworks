/* Checks the generated content and the game rules: data integrity, millions of hatches
 * per egg compared with the chances shown in the game, variants and huges odds,
 * machines, items, quests and rebirth. */
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
    if (found == pets[p].huge) FAIL("pet %s: in no egg (or a huge in an egg)", pets[p].name);
    if (pets[p].huge != (p >= HUGE_FIRST) || (pets[p].huge && pets[p].tier != TIER_EXCLUSIVE)) {
      FAIL("pet %s: huge flags", pets[p].name);
    }
  }
  for (int i = 0; i < POTION_COUNT; i++) {
    if (potions[i].type != i / 3 || potions[i].tier != i % 3 + 1 || !potions[i].minutes) FAIL("potion %d", i);
  }
  for (int i = 0; i < ENCHANT_COUNT; i++) {
    if (enchants[i].type != i / 3 || enchants[i].tier != i % 3 + 1 || !enchants[i].percent) FAIL("enchant %d", i);
  }
  for (int r = 0; r < RANK_COUNT; r++) {
    int stars = 0;
    for (int i = 0; i < ranks[r].reward_count; i++) stars += rank_rewards[ranks[r].first_reward + i].stars;
    if (stars != ranks[r].stars_total) FAIL("rank %s: %d stars of rewards, %d expected", ranks[r].name, stars,
                                            ranks[r].stars_total);
    for (int i = 0; i < ranks[r].goal_count; i++) {
      const rank_goal_t *g = &rank_goals[ranks[r].first_goal + i];
      if (g->stars < 1 || g->stars > QUEST_SLOTS || !g->amount || g->kind >= QUEST_KIND_COUNT) FAIL("rank %s: goal %d", ranks[r].name, i);
    }
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

static void check_game(void) {
  game_init();
  hatch_t h;
  if (game_open_egg(1, &h)) FAIL("egg 2 opened without enough coins");
  if (!game_open_egg(0, &h) || !h.is_new || game.owned[h.pet][h.variant] != 1 || game.team[0].pet != h.pet) FAIL("first egg");
  if (game.coins != START_COINS - eggs[0].cost) FAIL("coins after the first egg");
  if (game_equip(h.pet, h.variant)) FAIL("equipped a copy that is not owned");
  game.owned[h.pet][h.variant] = 9;
  for (int i = 0; i < 4; i++) game_equip(h.pet, h.variant);
  if (game_team_size() != BASE_TEAM || game_equip(h.pet, h.variant)) FAIL("team size limit");
  game.coins = 1e30;
  for (int z = 1; z < ZONE_COUNT; z++) {
    if (!game_unlock_next_zone()) FAIL("zone %d not unlocked", z);
  }
  if (game_unlock_next_zone()) FAIL("unlocked past the last zone");
  if (!game_egg_available(EGG_COUNT - 1)) FAIL("last egg unavailable");
}

/* Golden 2 %, rainbow 0.5 %, shiny 0.1 %, huge 1 in 50,000: counted over many eggs. */
static void check_variants(int n) {
  game_init();
  game.coins = 1e300;
  long variants[VARIANT_COUNT] = {0}, huges = 0, shiny = 0;
  for (int i = 0; i < n; i++) {
    hatch_t h;
    game_open_egg(0, &h);
    variants[variant_level(h.variant)]++;
    shiny += variant_shiny(h.variant);
    huges += pets[h.pet].huge;
  }
  struct { const char *name; long got; double p; } checks[] = {
    {"golden", variants[VAR_GOLDEN], GOLDEN_PER_MILLION / 1e6},
    {"rainbow", variants[VAR_RAINBOW], RAINBOW_PER_MILLION / 1e6},
    {"shiny", shiny, SHINY_PER_MILLION / 1e6},
    {"huge", huges, 1.0 / HUGE_ODDS},
  };
  for (int i = 0; i < 4; i++) {
    double expected = checks[i].p * n, sigma = sqrt(n * checks[i].p * (1 - checks[i].p));
    double z = (checks[i].got - expected) / sigma;
    if (fabs(z) > 5.5) FAIL("%s: %ld out of %d, expected %.0f", checks[i].name, checks[i].got, n, expected);
    printf("test_eggs: %s %ld / %d (expected %.0f)\n", checks[i].name, checks[i].got, n, expected);
  }
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
  game.owned[0][VAR_NORMAL] = 5;
  game.owned[1][VAR_GOLDEN] = 1;
  game_equip(0, VAR_NORMAL);
  game_equip(0, VAR_NORMAL);
  int n;
  double before = game.coins, coins = game_recycle_all(&n);
  if (n != 3 || game.owned[0][VAR_NORMAL] != 2 || game.owned[1][VAR_GOLDEN] != 1) FAIL("recycle counts (%d)", n);
  if (!(coins > 0) || game.coins != before + coins) FAIL("recycle coins");
  /* machines: 10 copies not equipped -> 1 of the next level, shiny kept */
  game_init();
  game.owned[3][VAR_NORMAL] = 11;
  game_equip(3, VAR_NORMAL);
  game_equip(3, VAR_NORMAL);
  if (game_machine_make(3, VAR_NORMAL)) FAIL("machine used equipped copies");
  game.owned[3][VAR_NORMAL] = 12;
  if (!game_machine_make(3, VAR_NORMAL) || game.owned[3][VAR_NORMAL] != 2 || game.owned[3][VAR_GOLDEN] != 1) FAIL("golden machine");
  game.owned[4][VAR_SHINY + VAR_GOLDEN] = 10;
  if (!game_machine_make(4, VAR_SHINY + VAR_GOLDEN) || game.owned[4][VAR_SHINY + VAR_RAINBOW] != 1) FAIL("rainbow machine");
  if (game_pet_power(3, VAR_GOLDEN) <= game_pet_power(3, VAR_NORMAL) ||
      game_pet_power(3, VAR_RAINBOW) <= game_pet_power(3, VAR_GOLDEN)) FAIL("variant powers");
  /* index rewards: once per completed egg */
  game_init();
  for (int s = eggs[0].first_slot; s < eggs[0].first_slot + eggs[0].slot_count; s++) {
    int p = egg_slots[s].pet;
    game.discovered[p / 8] |= (uint8_t)(1 << (p % 8));
  }
  if (game_claim_index_rewards() != game_egg_reward(0)) FAIL("index reward of the first egg");
  if (game_claim_index_rewards() != 0) FAIL("index reward given twice");
  /* rebirth: only at the last zone, keeps the pets, +75 % strength */
  game_init();
  game.owned[0][0] = 3;
  double power = game_pet_power(0, 0);
  if (game_can_rebirth()) FAIL("rebirth before the last zone");
  game.zones_unlocked = ZONE_COUNT;
  game.zone = ZONE_COUNT - 1;
  game_rebirth();
  if (game.zones_unlocked != 1 || game.zone != 0 || game.coins != START_COINS || game.owned[0][0] != 3) FAIL("rebirth reset");
  if (fabs(game_pet_power(0, 0) / power - 1.75) > 1e-9) FAIL("rebirth strength");
  if (game_can_rebirth()) FAIL("second rebirth");
}

static void check_items(void) {
  game_init();
  int coins2 = POTION_COINS * 3 + 1; /* Coins II */
  if (game_use_potion(coins2)) FAIL("potion used without owning it");
  game_give_potion(coins2, 2);
  double before = game_coin_bonus();
  game_use_potion(coins2);
  if (fabs(game_coin_bonus() / before - (1 + potions[coins2].percent / 100.0)) > 1e-9) FAIL("coins potion");
  game_use_potion(coins2);
  if (game.boost_ms[POTION_COINS] != 2u * potions[coins2].minutes * 60000u) FAIL("potion durations add up");
  game_tick_boosts(game.boost_ms[POTION_COINS]);
  if (game_boost_percent(POTION_COINS) != 0 || game.boost_tier[POTION_COINS]) FAIL("potion end");
  /* enchants: slots from the rank, diminishing returns */
  if (game_enchant_slots() != 1) FAIL("1 enchant slot at rank 1");
  int strong3 = ENCHANT_STRONG * 3 + 2;
  game_give_enchant(strong3, 2);
  if (!game_equip_enchant(strong3) || game_equip_enchant(strong3)) FAIL("enchant slots");
  game.rank = 4; /* rank 5: 3 slots */
  if (game_enchant_slots() != 3 || !game_equip_enchant(strong3)) FAIL("enchant slots at rank 5");
  if (fabs(game_enchant_percent(ENCHANT_STRONG) - enchants[strong3].percent * 1.6) > 1e-9) FAIL("enchant diminishing");
  /* drops happen, and only from the tables */
  int drops = 0;
  for (int i = 0; i < 100000; i++) drops += game_roll_drop(1.0) >= 0;
  if (drops < 300 || drops > 1000) FAIL("drop rate (%d / 100000)", drops);
}

static void check_quests(void) {
  game_init();
  quests_refill();
  if (game.quest_goal[0] == NO_QUEST || game.quest_goal[1] == NO_QUEST) FAIL("rank 1 quests");
  if (game.quest_goal[2] != NO_QUEST || game.quest_goal[3] != NO_QUEST) FAIL("no 3-4 star quest in rank 1");
  /* finish everything many times: rewards are paid in order and ranks go up */
  for (int round = 0; round < 400; round++) {
    for (int s = 0; s < QUEST_SLOTS; s++) {
      if (game.quest_goal[s] != NO_QUEST) game.quest_progress[s] = rank_goals[game.quest_goal[s]].amount;
    }
    game.zones_unlocked = ZONE_COUNT - 1; /* every quest kind possible */
    game_give_potion(POTION_COINS * 3 + 2, 1);
    bool up;
    quests_update(&up);
  }
  if (game.rank != RANK_COUNT - 1) FAIL("rank %d after many quests", game.rank);
  if (!(game.diamonds > 0)) FAIL("rank rewards");
}

int main(void) {
  game_random = rand32;
  check_content();
  check_draws(2000000);
  check_luck();
  check_game();
  check_variants(5000000);
  check_progression();
  check_items();
  check_quests();
  if (failures == 0) printf("test_eggs: OK\n");
  return failures != 0;
}
