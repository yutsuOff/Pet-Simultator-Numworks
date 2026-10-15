/* Balance simulator (PC): a bot plays the whole game with the real rules (breakables,
 * eggs, upgrades, items, quests, machines) and reports how long each zone takes.
 *
 *   make balance          -> output/balance.txt
 *
 * The bot is an attentive but not perfect player: its pets attack all the time, it taps
 * TAPS_PER_SECOND times per second, hatches the best egg up to EGGS_PER_EGG times, buys
 * the next zone as soon as possible, spends its diamonds, uses its potions and enchants,
 * turns 10 copies into golden / rainbow pets, uses the machines of zones 38-65 and makes
 * its rebirth as soon as zone 37 is reached (zones 38-65 need it). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../src/game/breakables.h"
#include "../src/game/game.h"

#define TAPS_PER_SECOND 1.0
#define EGGS_PER_EGG 40       /* hatches of each best egg before saving for the next zone */
#define SLOTS 8
#define TAP_PILES 8           /* same as the Farm screen */
#define MAX_HOURS 40

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

static bool reached(int machine) { return game.zones_unlocked > machine_zone[machine]; }

static void use_machines(void) {
  for (int p = 0; p < PET_COUNT; p++) {
    for (int v = 0; v < VARIANT_COUNT; v++) {
      bool machine = reached(variant_level(v) == VAR_NORMAL ? MACHINE_GOLDEN : MACHINE_RAINBOW);
      while (machine && game_machine_make(p, v)) {}
    }
  }
  for (int i = 0; reached(MACHINE_POTIONS) && i < POTION_COUNT; i++) while (game_upgrade_potion(i)) {}
  for (int i = 0; reached(MACHINE_ENCHANTS) && i < ENCHANT_COUNT; i++) while (game_upgrade_enchant(i)) {}
  if (reached(MACHINE_FUSE)) { /* fuses the normal pets weaker than the team, 12 at a time */
    double weakest = 1e300;
    for (int i = 0; i < game_team_slots(); i++) {
      if (game.team[i].pet != NO_PET) {
        double p = game_base_power(game.team[i].pet, game.team[i].variant);
        if (p < weakest) weakest = p;
      }
    }
    team_slot_t in[FUSE_MAX];
    int n = 0;
    hatch_t h;
    for (int p = 0; p < HUGE_FIRST; p++) {
      if (game_base_power(p, VAR_NORMAL) >= weakest) continue;
      for (int k = game_machine_available(p, VAR_NORMAL); k > 0; k--) {
        in[n++] = (team_slot_t){(uint8_t)p, VAR_NORMAL};
        if (n == FUSE_MAX) game_fuse(in, n, &h), n = 0;
      }
    }
  }
  if (reached(MACHINE_DAYCARE)) { /* the strongest free pets */
    daycare_reward_t r;
    for (int i = 0; i < DAYCARE_SLOTS; i++) game_daycare_collect(i, &r);
    for (int i = 0; i < DAYCARE_SLOTS; i++) {
      int best = -1, best_v = 0;
      for (int p = 0; p < HUGE_FIRST; p++) {
        for (int v = 0; v < VARIANT_COUNT; v++) {
          if (game_machine_available(p, v) > 0 && (best < 0 || game_base_power(p, v) > game_base_power(best, best_v))) {
            best = p, best_v = v;
          }
        }
      }
      if (best < 0 || !game_daycare_enroll(best, best_v)) break;
    }
  }
}

int main(int argc, char **argv) {
  bool csv = argc > 1 && strcmp(argv[1], "--csv") == 0; /* zone,seconds for tools/calibrate_balance.py */
  game_random = rand32;
  game_init();
  quests_refill();
  /* zone_time[z]: time zone z was bought (run 1 for zones up to 37, run 2 after the rebirth) */
  double zone_time[ZONE_COUNT] = {0}, replay_time[REBIRTH_ZONE] = {0}, diamonds_earned = 0, last_diamonds = 0;
  long rebirth_at = -1;
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
    if (game_can_rebirth()) {
      game_rebirth();
      rebirth_at = t;
      continue;
    }
    if (game_unlock_next_zone()) {
      if (rebirth_at < 0 || game.zones_unlocked > REBIRTH_ZONE) zone_time[game.zones_unlocked - 1] = t;
      else replay_time[game.zones_unlocked - 1] = t;
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
    game_tick_daycare(1000);
    bool up;
    quests_update(&up);
    last_diamonds = game.diamonds;
  }
  /* time spent in zone z-1 before buying zone z; zone 37 -> 38: from the end of the replay */
  double spent[ZONE_COUNT] = {0};
  int last = rebirth_at >= 0 ? game.zones_unlocked : REBIRTH_ZONE < game.zones_unlocked ? REBIRTH_ZONE : game.zones_unlocked;
  for (int z = 1; z < last; z++) {
    double before = z == REBIRTH_ZONE ? replay_time[REBIRTH_ZONE - 1] : zone_time[z - 1];
    spent[z] = zone_time[z] - before;
  }
  double replay = rebirth_at >= 0 ? replay_time[REBIRTH_ZONE - 1] - rebirth_at : 0;
  if (csv) {
    for (int z = 1; z < last; z++) printf("%d,%.0f\n", z, spent[z]);
    printf("replay,%.0f\n", replay);
    printf("total,%ld\n", t);
    return 0;
  }
  printf("zone  reached  in zone  name\n");
  for (int z = 1; z < last; z++) {
    if (z == REBIRTH_ZONE) {
      printf("      %2ld:%02ld    rebirth, zones 1-%d again in %.0f min\n", rebirth_at / 3600, rebirth_at / 60 % 60,
             REBIRTH_ZONE, replay / 60);
    }
    printf("%4d  %2ld:%02ld   %5.1f min  %s\n", z + 1, (long)zone_time[z] / 3600, (long)zone_time[z] / 60 % 60,
           spent[z] / 60, zones[z].name);
  }
  printf("\nfinished: %s in %ld h %02ld min\n", game.zones_unlocked == ZONE_COUNT ? "yes" : "no", t / 3600, t / 60 % 60);
  printf("rank %d (%s), %d pets discovered, %u eggs, diamonds earned %.0f, upgrades:", game.rank + 1,
         ranks[game.rank].name, game_discovered_count(), game.eggs_opened, diamonds_earned);
  for (int id = 0; id < UPG_COUNT; id++) printf(" %d", game.upgrade_level[id]);
  if (upgrades_done >= 0) printf(" (all maxed at %ld h %02ld)", upgrades_done / 3600, upgrades_done / 60 % 60);
  printf("\n");
  return 0;
}
