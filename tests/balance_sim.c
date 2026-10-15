/* Balance simulator (PC): a bot plays the whole game with the real rules (breakables,
 * eggs, upgrades, items, quests, machines) and reports how long each zone takes.
 *
 *   make balance          -> output/balance.txt
 *
 * The bot is an attentive but not perfect player: its pets attack all the time, it taps
 * TAPS_PER_SECOND times per second, hatches the best egg up to EGGS_PER_EGG times, buys
 * the next zone as soon as possible, spends its diamonds, uses its potions and enchants,
 * and turns 10 copies into golden / rainbow pets. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/game/breakables.h"
#include "../src/game/game.h"

#define TAPS_PER_SECOND 1.0
#define EGGS_PER_EGG 40       /* hatches of each best egg before saving for the next zone */
#define SLOTS 8
#define TAP_PILES 8           /* same as the Farm screen */
#define MAX_HOURS 30

static uint64_t rng = 0x2545F4914F6CDD1DULL;

static uint32_t rand32(void) {
  rng ^= rng >> 12;
  rng ^= rng << 25;
  rng ^= rng >> 27;
  return (uint32_t)((rng * 0x2545F4914F6CDD1DULL) >> 32);
}

static int random_breakable(void) {
  int r = (int)(rand32() % 1000), type = 0;
  while (type < BRK_COUNT - 1 && r >= breakables[type].weight) r -= breakables[type++].weight;
  return type;
}

static void spend_diamonds(void) {
  for (;;) { /* cheapest upgrade first */
    int best = -1;
    for (int id = 0; id < UPG_COUNT; id++) {
      if (game.upgrade_level[id] >= upgrades[id].max_level) continue;
      if (best < 0 || upgrade_cost(id) < upgrade_cost(best)) best = id;
    }
    if (best < 0 || !game_buy_upgrade(best)) return;
  }
}

static void use_items(void) {
  for (int p = POTION_COUNT - 1; p >= 0; p--) {
    if (game.potions_owned[p] && !game.boost_tier[potions[p].type]) game_use_potion(p);
  }
  for (int s = 0; s < game_enchant_slots(); s++) {
    if (game.enchant_slot[s] != NO_ITEM) continue;
    for (int e = ENCHANT_COUNT - 1; e >= 0; e--) {
      if (game_equip_enchant(e)) break;
    }
  }
}

static void use_machines(void) {
  for (int p = 0; p < PET_COUNT; p++) {
    for (int v = 0; v < VARIANT_COUNT; v++) {
      bool machine = variant_level(v) == VAR_NORMAL ? game.zones_unlocked > GOLDEN_MACHINE_ZONE
                                                    : game.zones_unlocked > RAINBOW_MACHINE_ZONE;
      while (machine && game_machine_make(p, v)) {}
    }
  }
}

int main(int argc, char **argv) {
  bool csv = argc > 1 && strcmp(argv[1], "--csv") == 0; /* zone,seconds for tools/calibrate_balance.py */
  game_random = rand32;
  game_init();
  quests_refill();
  double zone_time[ZONE_COUNT] = {0}, diamonds_earned = 0, last_diamonds = 0;
  int hatched_best = 0, best_egg = -1;
  double hp = 0;
  int type = BRK_PILE;
  long t, upgrades_done = -1;
  for (t = 0; t < MAX_HOURS * 3600L && game.zones_unlocked < ZONE_COUNT; t++) {
    /* one second of farming in the best zone */
    game.zone = (uint8_t)(game.zones_unlocked - 1);
    double tap = (zones[game.zone].pile_hp / TAP_PILES + game_team_power() / 5) * game_tap_bonus();
    double damage = game_team_power() + TAPS_PER_SECOND * tap;
    double max_kills = SLOTS * 1000.0 / game_respawn_ms(), kills = 0;
    if (hp <= 0) hp = breakable_hp(type);
    while (damage >= hp && kills < max_kills) {
      damage -= hp;
      kills++;
      breakable_destroyed(type, breakable_reward(type));
      type = random_breakable();
      hp = breakable_hp(type);
    }
    if (kills < max_kills) hp -= damage;
    if (game.diamonds > last_diamonds) diamonds_earned += game.diamonds - last_diamonds;
    /* decisions */
    if (game_unlock_next_zone()) {
      zone_time[game.zones_unlocked - 1] = t;
      continue;
    }
    int egg = game_best_egg();
    if (egg != best_egg) best_egg = egg, hatched_best = 0;
    hatch_t h;
    while (hatched_best < EGGS_PER_EGG && game_open_egg(egg, &h)) hatched_best++;
    game_equip_best();
    if (t % 60 == 0) use_machines(), game_equip_best();
    spend_diamonds();
    if (upgrades_done < 0) {
      bool all = true;
      for (int id = 0; id < UPG_COUNT; id++) all &= game.upgrade_level[id] == upgrades[id].max_level;
      if (all) upgrades_done = t;
    }
    use_items();
    game_tick_boosts(1000);
    bool up;
    quests_update(&up);
    last_diamonds = game.diamonds;
  }
  if (csv) {
    for (int z = 1; z < game.zones_unlocked; z++) printf("%d,%.0f\n", z, zone_time[z] - zone_time[z - 1]);
    printf("total,%ld\n", t);
    return 0;
  }
  printf("zone  time    name\n");
  for (int z = 1; z < game.zones_unlocked; z++) {
    printf("%4d  %2ld:%02ld  %s\n", z + 1, (long)zone_time[z] / 3600, (long)zone_time[z] / 60 % 60, zones[z].name);
  }
  printf("\nfinished: %s in %ld h %02ld min\n", game.zones_unlocked == ZONE_COUNT ? "yes" : "no", t / 3600, t / 60 % 60);
  printf("rank %d (%s), %d pets discovered, %u eggs, diamonds earned %.0f, upgrades:", game.rank + 1,
         ranks[game.rank].name, game_discovered_count(), game.eggs_opened, diamonds_earned);
  for (int id = 0; id < UPG_COUNT; id++) printf(" %d", game.upgrade_level[id]);
  if (upgrades_done >= 0) printf(" (all maxed at %ld h %02ld)", upgrades_done / 3600, upgrades_done / 60 % 60);
  printf("\n");
  return 0;
}
