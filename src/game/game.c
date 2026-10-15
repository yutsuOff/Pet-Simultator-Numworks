#include "game.h"

game_t game;

/* Provisional balancing, like the rest (docs/CONCEPTION.md §6). */
const upgrade_t upgrades[UPG_COUNT] = {
  [UPG_DAMAGE] = {"Force des pets", "Dégâts des pets", UPG_CAT_PETS, 10, 5, 1.6},
  [UPG_TEAM] = {"Équipe", "Pets équipés", UPG_CAT_PETS, MAX_TEAM - BASE_TEAM, 25, 3.0},
  [UPG_TAP] = {"Coup de poing", "Dégâts de ton coup (OK)", UPG_CAT_FARMING, 5, 3, 1.7},
  [UPG_RESPAWN] = {"Réapparition", "Retour des objets cassés", UPG_CAT_FARMING, 5, 4, 1.7},
  [UPG_LUCK] = {"Chance", "Chance des pets rares", UPG_CAT_EGGS, 10, 8, 1.6},
  [UPG_MULTI] = {"Ouverture multiple", "Œufs ouverts d'un coup", UPG_CAT_EGGS, MAX_HATCH - 1, 30, 4.0},
  [UPG_COINS] = {"Coins", "Coins des objets", UPG_CAT_WEALTH, 10, 5, 1.6},
  [UPG_DIAMONDS] = {"Diamants", "Diamants des objets", UPG_CAT_WEALTH, 5, 6, 1.8},
};

void game_init(void) {
  game = (game_t){0};
  game.coins = START_COINS;
  game.zones_unlocked = 1;
  for (int i = 0; i < MAX_TEAM; i++) game.team[i] = NO_PET;
}

static int level(int id) { return game.upgrade_level[id]; }

/* --- Pets and team --- */

bool game_discovered(int pet) { return (game.discovered[pet / 8] >> (pet % 8)) & 1; }

int game_discovered_count(void) {
  int n = 0;
  for (int p = 0; p < PET_COUNT; p++) n += game_discovered(p);
  return n;
}

int game_team_slots(void) { return BASE_TEAM + level(UPG_TEAM); }

int game_equipped_count(int pet) {
  int n = 0;
  for (int i = 0; i < game_team_slots(); i++) n += game.team[i] == pet;
  return n;
}

int game_team_size(void) { return game_team_slots() - game_equipped_count(NO_PET); }

double game_pet_power(int pet) { return pets[pet].power * (1 + 0.15 * level(UPG_DAMAGE)); }

double game_team_power(void) {
  double p = 0;
  for (int i = 0; i < game_team_slots(); i++) {
    if (game.team[i] != NO_PET) p += game_pet_power(game.team[i]);
  }
  return p;
}

bool game_equip(int pet) {
  if (game_equipped_count(pet) >= game.owned[pet]) return false;
  for (int i = 0; i < game_team_slots(); i++) {
    if (game.team[i] == NO_PET) {
      game.team[i] = (uint8_t)pet;
      return true;
    }
  }
  return false;
}

bool game_unequip(int pet) {
  /* remove the last copy so the order of the others is kept */
  for (int i = game_team_slots() - 1; i >= 0; i--) {
    if (game.team[i] == pet) {
      for (int j = i; j < MAX_TEAM - 1; j++) game.team[j] = game.team[j + 1];
      game.team[MAX_TEAM - 1] = NO_PET;
      return true;
    }
  }
  return false;
}

void game_equip_best(void) {
  for (int i = 0; i < MAX_TEAM; i++) game.team[i] = NO_PET;
  for (int slot = 0; slot < game_team_slots(); slot++) {
    int best = -1;
    for (int p = 0; p < PET_COUNT; p++) {
      if (game_equipped_count(p) >= game.owned[p]) continue;
      if (best < 0 || pets[p].power > pets[best].power) best = p;
    }
    if (best < 0) break;
    game.team[slot] = (uint8_t)best;
  }
}

int game_duplicates(int pet) {
  int keep = game_equipped_count(pet);
  if (keep < 1) keep = 1;
  return game.owned[pet] > keep ? game.owned[pet] - keep : 0;
}

double game_recycle_value(int pet) {
  return eggs[pets[pet].egg].cost * 0.2 * (1 + pets[pet].tier);
}

double game_recycle_all(int *count) {
  double coins = 0;
  *count = 0;
  for (int p = 0; p < PET_COUNT; p++) {
    int n = game_duplicates(p);
    coins += n * game_recycle_value(p);
    *count += n;
    game.owned[p] = (uint16_t)(game.owned[p] - n);
  }
  game.coins += coins;
  return coins;
}

/* --- Eggs --- */

uint32_t egg_slot_weight(int slot) {
  uint32_t w = egg_slots[slot].weight;
  if (w < EGG_WEIGHT_TOTAL / 5) w += (uint32_t)((uint64_t)w * level(UPG_LUCK) / 10);
  return w;
}

static uint32_t egg_total(int egg) {
  uint32_t total = 0;
  for (int s = eggs[egg].first_slot; s < eggs[egg].first_slot + eggs[egg].slot_count; s++) {
    total += egg_slot_weight(s); /* at most 2 x 10^9: fits */
  }
  return total;
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

int game_hatch_count(void) { return 1 + level(UPG_MULTI); }

int game_open_egg(int egg, uint32_t (*rand32)(void), bool *is_new) {
  if (!game_egg_available(egg) || game.coins < eggs[egg].cost) return -1;
  game.coins -= eggs[egg].cost;
  int pet = egg_slots[egg_roll(egg, rand32)].pet;
  *is_new = !game_discovered(pet);
  game.discovered[pet / 8] |= (uint8_t)(1 << (pet % 8));
  if (game.owned[pet] < UINT16_MAX) game.owned[pet]++;
  game.eggs_opened++;
  /* a free team slot is filled right away, so a new player sees the pet at work */
  if (game_team_size() < game_team_slots()) game_equip(pet);
  return pet;
}

/* --- Index rewards --- */

bool game_egg_complete(int egg) {
  for (int s = eggs[egg].first_slot; s < eggs[egg].first_slot + eggs[egg].slot_count; s++) {
    if (!game_discovered(egg_slots[s].pet)) return false;
  }
  return true;
}

double game_egg_reward(int egg) { return 10 + 2 * eggs[egg].number; }

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

/* --- Zones --- */

bool game_unlock_next_zone(void) {
  int z = game.zones_unlocked;
  if (z >= ZONE_COUNT || game.coins < zones[z].cost) return false;
  game.coins -= zones[z].cost;
  game.zones_unlocked++;
  game.zone = (uint8_t)z;
  return true;
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
  return (1 + 0.1 * level(UPG_COINS)) * (1 + 0.05 * (game_discovered_count() / INDEX_STEP));
}

double game_diamond_bonus(void) { return 1 + 0.2 * level(UPG_DIAMONDS); }

double game_tap_bonus(void) { return 1 + 0.5 * level(UPG_TAP); }

uint32_t game_respawn_ms(void) { return 3000 - 400 * (uint32_t)level(UPG_RESPAWN); }
