#include "game.h"

game_t game;
uint32_t (*game_random)(void);
bool game_debug_next_huge;

/* Provisional balancing, like the rest (docs/CONCEPTION.md §6). */
const upgrade_t upgrades[UPG_COUNT] = {
  [UPG_DAMAGE] = {"Force des pets", "Dégâts des pets", UPG_CAT_PETS, 10, 15, 1.6},
  [UPG_TEAM] = {"Équipe", "Pets équipés", UPG_CAT_PETS, MAX_TEAM - BASE_TEAM, 75, 3.0},
  [UPG_TAP] = {"Coup de poing", "Dégâts de ton coup (OK)", UPG_CAT_FARMING, 5, 9, 1.7},
  [UPG_RESPAWN] = {"Réapparition", "Retour des objets cassés", UPG_CAT_FARMING, 5, 12, 1.7},
  [UPG_LUCK] = {"Chance", "Chance des pets rares", UPG_CAT_EGGS, 10, 24, 1.6},
  [UPG_MULTI] = {"Ouverture multiple", "Œufs ouverts d'un coup", UPG_CAT_EGGS, MAX_HATCH - 1, 90, 4.0},
  [UPG_COINS] = {"Coins", "Coins des objets", UPG_CAT_WEALTH, 10, 15, 1.6},
  [UPG_DIAMONDS] = {"Diamants", "Diamants des objets", UPG_CAT_WEALTH, 5, 18, 1.8},
};

/* Power of the variants. Golden and rainbow: PS99 wiki (Gold / Rainbow Machine pages,
 * "about +13 %" and "+27 %"); shiny: not published by BIG Games, game choice. */
static const double variant_power[VARIANT_COUNT] = {1.0, 1.13, 1.27, 1.5, 1.5 * 1.13, 1.5 * 1.27};
/* Huges, by variant: odds (1 in N) and power (% of the best other pet owned). */
const uint32_t huge_odds[VARIANT_COUNT] = {50000, 75000, 125000, 100000, 150000, 175000};
static const double huge_percent[VARIANT_COUNT] = {200, 225, 275, 250, 300, 325};
#define REBIRTH_STRENGTH 0.75 /* PS99 Rebirth 1: +75 % pet strength */

void game_init(void) {
  game = (game_t){0};
  game.coins = START_COINS;
  game.zones_unlocked = 1;
  for (int i = 0; i < MAX_TEAM; i++) game.team[i].pet = NO_PET;
  for (int i = 0; i < MAX_ENCHANT_SLOTS; i++) game.enchant_slot[i] = NO_ITEM;
  for (int i = 0; i < QUEST_SLOTS; i++) game.quest_goal[i] = NO_QUEST;
}

static int level(int id) { return game.upgrade_level[id]; }

/* --- Pets and team --- */

bool game_discovered(int pet) { return (game.discovered[pet / 8] >> (pet % 8)) & 1; }

int game_discovered_count(void) {
  int n = 0;
  for (int p = 0; p < PET_COUNT; p++) n += game_discovered(p);
  return n;
}

int game_owned_total(int pet) {
  int n = 0;
  for (int v = 0; v < VARIANT_COUNT; v++) n += game.owned[pet][v];
  return n;
}

int game_team_slots(void) { return BASE_TEAM + level(UPG_TEAM); }

int game_equipped_count(int pet, int variant) {
  int n = 0;
  for (int i = 0; i < game_team_slots(); i++) n += game.team[i].pet == pet && game.team[i].variant == variant;
  return n;
}

int game_team_size(void) {
  int n = 0;
  for (int i = 0; i < game_team_slots(); i++) n += game.team[i].pet != NO_PET;
  return n;
}

double game_base_power(int pet, int variant) {
  if (!pets[pet].huge) return pets[pet].power * variant_power[variant];
  double best = 0;
  for (int p = 0; p < HUGE_FIRST; p++) {
    for (int v = 0; v < VARIANT_COUNT; v++) {
      if (game.owned[p][v] && pets[p].power * variant_power[v] > best) best = pets[p].power * variant_power[v];
    }
  }
  if (best == 0) best = pets[egg_slots[eggs[game_best_egg()].first_slot].pet].power; /* no other pet yet */
  return best * huge_percent[variant] / 100;
}

double game_pet_power(int pet, int variant) {
  double strong = 1 + (game_boost_percent(POTION_DAMAGE) + game_enchant_percent(ENCHANT_STRONG)) / 100;
  return game_base_power(pet, variant) * (1 + 0.15 * level(UPG_DAMAGE)) * strong *
         (1 + REBIRTH_STRENGTH * game.rebirths);
}

double game_team_power(void) {
  double p = 0;
  for (int i = 0; i < game_team_slots(); i++) {
    if (game.team[i].pet != NO_PET) p += game_pet_power(game.team[i].pet, game.team[i].variant);
  }
  return p;
}

bool game_equip(int pet, int variant) {
  if (game_equipped_count(pet, variant) >= game.owned[pet][variant]) return false;
  for (int i = 0; i < game_team_slots(); i++) {
    if (game.team[i].pet == NO_PET) {
      game.team[i] = (team_slot_t){(uint8_t)pet, (uint8_t)variant};
      return true;
    }
  }
  return false;
}

bool game_unequip(int pet, int variant) {
  /* remove the last copy so the order of the others is kept */
  for (int i = game_team_slots() - 1; i >= 0; i--) {
    if (game.team[i].pet == pet && game.team[i].variant == variant) {
      for (int j = i; j < MAX_TEAM - 1; j++) game.team[j] = game.team[j + 1];
      game.team[MAX_TEAM - 1].pet = NO_PET;
      return true;
    }
  }
  return false;
}

void game_equip_best(void) {
  for (int i = 0; i < MAX_TEAM; i++) game.team[i].pet = NO_PET;
  for (int slot = 0; slot < game_team_slots(); slot++) {
    int best = -1, best_v = 0;
    double best_power = 0;
    for (int p = 0; p < PET_COUNT; p++) {
      for (int v = 0; v < VARIANT_COUNT; v++) {
        if (game_equipped_count(p, v) >= game.owned[p][v]) continue;
        double power = game_base_power(p, v);
        if (best < 0 || power > best_power) best = p, best_v = v, best_power = power;
      }
    }
    if (best < 0) break;
    game.team[slot] = (team_slot_t){(uint8_t)best, (uint8_t)best_v};
  }
}

int game_duplicates(int pet, int variant) {
  int keep = game_equipped_count(pet, variant);
  if (keep < 1) keep = 1;
  return game.owned[pet][variant] > keep ? game.owned[pet][variant] - keep : 0;
}

double game_recycle_value(int pet, int variant) {
  if (pets[pet].huge) return 0; /* never sold */
  return eggs[pets[pet].egg].cost * 0.2 * (1 + pets[pet].tier) * variant_power[variant];
}

double game_recycle_all(int *count) {
  double coins = 0;
  *count = 0;
  for (int p = 0; p < PET_COUNT; p++) {
    if (pets[p].huge) continue;
    for (int v = 0; v < VARIANT_COUNT; v++) {
      int n = game_duplicates(p, v);
      coins += n * game_recycle_value(p, v);
      *count += n;
      game.owned[p][v] = (uint16_t)(game.owned[p][v] - n);
    }
  }
  game.coins += coins;
  return coins;
}

/* --- Machines --- */

int game_machine_available(int pet, int variant) {
  return game.owned[pet][variant] - game_equipped_count(pet, variant);
}

bool game_machine_make(int pet, int variant) {
  /* huges never go in a machine (PS99: golden / rainbow huges only hatch) */
  if (pets[pet].huge || variant_level(variant) == VAR_RAINBOW || game_machine_available(pet, variant) < MACHINE_INPUT) {
    return false;
  }
  game.owned[pet][variant] -= MACHINE_INPUT;
  game.owned[pet][variant + 1]++;
  quest_event(variant_level(variant) == VAR_NORMAL ? QUEST_MAKE_GOLDEN : QUEST_MAKE_RAINBOW, 1, 0);
  return true;
}

/* --- Eggs --- */

uint32_t egg_slot_weight(int slot) {
  uint32_t w = egg_slots[slot].weight;
  if (w < EGG_WEIGHT_TOTAL / 5) w += (uint32_t)(w * game_luck_percent() / 100);
  return w;
}

static uint32_t egg_total(int egg) {
  uint64_t total = 0;
  for (int s = eggs[egg].first_slot; s < eggs[egg].first_slot + eggs[egg].slot_count; s++) {
    total += egg_slot_weight(s);
  }
  return total > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)total;
}

double egg_slot_chance(int slot) {
  int egg = 0;
  while (eggs[egg].first_slot + eggs[egg].slot_count <= slot) egg++;
  return (double)egg_slot_weight(slot) / egg_total(egg);
}

int egg_roll(int egg, uint32_t (*rand32)(void)) {
  /* Rejection sampling: keep values below the largest multiple of the total. */
  uint32_t total = egg_total(egg);
  const uint32_t limit = (uint32_t)(0x100000000ULL / total * total - 1);
  uint32_t r;
  do r = rand32();
  while (r > limit);
  r %= total;
  const egg_t *e = &eggs[egg];
  for (int s = e->first_slot; s < e->first_slot + e->slot_count; s++) {
    uint32_t w = egg_slot_weight(s);
    if (r < w) return s;
    r -= w;
  }
  return e->first_slot + e->slot_count - 1; /* unreachable: r < total */
}

bool game_egg_available(int egg) { return eggs[egg].zone < game.zones_unlocked; }

int game_best_egg(void) {
  int e = 0;
  while (e + 1 < EGG_COUNT && game_egg_available(e + 1)) e++;
  return e;
}

int game_hatch_count(void) { return 1 + level(UPG_MULTI); }

/* Uniform value in [0, n) without bias. */
static uint32_t uniform(uint32_t n) {
  const uint32_t limit = (uint32_t)(0x100000000ULL / n * n - 1);
  uint32_t r;
  do r = game_random();
  while (r > limit);
  return r % n;
}

bool game_open_egg(int egg, hatch_t *out) {
  if (!game_egg_available(egg) || game.coins < eggs[egg].cost) return false;
  game.coins -= eggs[egg].cost;
  /* huge first: each variant has its own odds (out of a billion) */
  uint32_t h = uniform(1000000000u), huge_variant = VARIANT_COUNT;
  for (int v = 0; v < VARIANT_COUNT; v++) {
    uint32_t w = 1000000000u / huge_odds[v];
    if (h < w) {
      huge_variant = (uint32_t)v;
      break;
    }
    h -= w;
  }
  if (game_debug_next_huge && huge_variant == VARIANT_COUNT) huge_variant = VAR_NORMAL;
  game_debug_next_huge = false;
  int pet, variant;
  if (huge_variant < VARIANT_COUNT) {
    pet = HUGE_FIRST + (int)uniform(HUGE_COUNT);
    variant = (int)huge_variant;
  } else {
    pet = egg_slots[egg_roll(egg, game_random)].pet;
    uint32_t r = uniform(1000000);
    variant = r < RAINBOW_PER_MILLION ? VAR_RAINBOW : r < RAINBOW_PER_MILLION + GOLDEN_PER_MILLION ? VAR_GOLDEN : VAR_NORMAL;
    if (uniform(1000000) < SHINY_PER_MILLION) variant += VAR_SHINY;
  }
  out->pet = (uint8_t)pet;
  out->variant = (uint8_t)variant;
  out->is_new = !game_discovered(pet);
  game.discovered[pet / 8] |= (uint8_t)(1 << (pet % 8));
  if (game.owned[pet][variant] < UINT16_MAX) game.owned[pet][variant]++;
  game.eggs_opened++;
  /* a free team slot is filled right away, so a new player sees the pet at work */
  if (game_team_size() < game_team_slots()) game_equip(pet, variant);
  quest_event(QUEST_HATCH_ANY, 1, 0);
  if (egg == game_best_egg()) quest_event(QUEST_HATCH_BEST, 1, 0);
  if (pets[pet].tier >= TIER_LEGENDARY) quest_event(QUEST_HATCH_LEGENDARY, 1, 0);
  return true;
}

/* --- Index rewards --- */

bool game_egg_complete(int egg) {
  for (int s = eggs[egg].first_slot; s < eggs[egg].first_slot + eggs[egg].slot_count; s++) {
    if (!game_discovered(egg_slots[s].pet)) return false;
  }
  return true;
}

double game_egg_reward(int egg) { return 5 + eggs[egg].number; }

double game_claim_index_rewards(void) {
  double given = 0;
  for (int e = 0; e < EGG_COUNT; e++) {
    bool done = (game.egg_rewarded[e / 8] >> (e % 8)) & 1;
    if (done || !game_egg_complete(e)) continue;
    game.egg_rewarded[e / 8] |= (uint8_t)(1 << (e % 8));
    given += game_egg_reward(e);
  }
  game.diamonds += given;
  return given;
}

/* --- Zones and rebirth --- */

bool game_unlock_next_zone(void) {
  int z = game.zones_unlocked;
  if (z >= ZONE_COUNT || game.coins < zones[z].cost) return false;
  game.coins -= zones[z].cost;
  game.zones_unlocked++;
  game.zone = (uint8_t)z;
  quest_event(QUEST_UNLOCK_AREA, 1, 0);
  return true;
}

bool game_can_rebirth(void) { return game.zones_unlocked == ZONE_COUNT && game.rebirths < MAX_REBIRTH; }

void game_rebirth(void) {
  if (!game_can_rebirth()) return;
  game.coins = START_COINS;
  game.zones_unlocked = 1;
  game.zone = 0;
  game.rebirths++;
}

/* --- Upgrades and bonuses --- */

double upgrade_cost(int id) {
  double c = upgrades[id].base_cost;
  for (int i = 0; i < level(id); i++) c *= upgrades[id].cost_growth;
  return (double)(long long)(c + 0.5);
}

bool game_buy_upgrade(int id) {
  if (level(id) >= upgrades[id].max_level) return false;
  double c = upgrade_cost(id);
  if (game.diamonds < c) return false;
  game.diamonds -= c;
  game.upgrade_level[id]++;
  return true;
}

double game_coin_bonus(void) {
  double boosts = 1 + (game_boost_percent(POTION_COINS) + game_enchant_percent(ENCHANT_COINS)) / 100;
  return (1 + 0.1 * level(UPG_COINS)) * (1 + 0.05 * (game_discovered_count() / INDEX_STEP)) * boosts;
}

double game_diamond_bonus(void) {
  return (1 + 0.2 * level(UPG_DIAMONDS)) *
         (1 + (game_boost_percent(POTION_DIAMONDS) + game_enchant_percent(ENCHANT_DIAMONDS)) / 100);
}

double game_tap_bonus(void) { return (1 + 0.5 * level(UPG_TAP)) * (1 + game_enchant_percent(ENCHANT_TAP) / 100); }

double game_luck_percent(void) {
  return 10.0 * level(UPG_LUCK) + game_boost_percent(POTION_LUCKY) + game_enchant_percent(ENCHANT_LUCKY);
}

double game_drop_bonus(void) {
  return 1 + (game_boost_percent(POTION_TREASURE) + game_enchant_percent(ENCHANT_TREASURE)) / 100;
}

uint32_t game_respawn_ms(void) { return 3000 - 400 * (uint32_t)level(UPG_RESPAWN); }
