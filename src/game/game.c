#include "game.h"

game_t game;

void game_init(void) {
  game = (game_t){0};
  game.coins = START_COINS;
  game.zones_unlocked = 1;
  for (int i = 0; i < TEAM_SIZE; i++) game.team[i] = NO_PET;
}

/* --- Pets and team --- */

bool game_discovered(int pet) { return (game.discovered[pet / 8] >> (pet % 8)) & 1; }

int game_equipped_count(int pet) {
  int n = 0;
  for (int i = 0; i < TEAM_SIZE; i++) n += game.team[i] == pet;
  return n;
}

int game_team_size(void) { return TEAM_SIZE - game_equipped_count(NO_PET); }

double game_team_power(void) {
  double p = 0;
  for (int i = 0; i < TEAM_SIZE; i++) {
    if (game.team[i] != NO_PET) p += pets[game.team[i]].power;
  }
  return p;
}

bool game_equip(int pet) {
  if (game_equipped_count(pet) >= game.owned[pet]) return false;
  for (int i = 0; i < TEAM_SIZE; i++) {
    if (game.team[i] == NO_PET) {
      game.team[i] = (uint8_t)pet;
      return true;
    }
  }
  return false;
}

bool game_unequip(int pet) {
  /* remove the last copy so the order of the others is kept */
  for (int i = TEAM_SIZE - 1; i >= 0; i--) {
    if (game.team[i] == pet) {
      for (int j = i; j < TEAM_SIZE - 1; j++) game.team[j] = game.team[j + 1];
      game.team[TEAM_SIZE - 1] = NO_PET;
      return true;
    }
  }
  return false;
}

void game_equip_best(void) {
  for (int i = 0; i < TEAM_SIZE; i++) game.team[i] = NO_PET;
  for (int slot = 0; slot < TEAM_SIZE; slot++) {
    int best = -1;
    for (int p = 0; p < PET_COUNT; p++) {
      if (game_equipped_count(p) >= game.owned[p]) continue;
      if (best < 0 || pets[p].power > pets[best].power) best = p;
    }
    if (best < 0) break;
    game.team[slot] = (uint8_t)best;
  }
}

/* --- Eggs --- */

int egg_roll(int egg, uint32_t (*rand32)(void)) {
  /* Rejection sampling: keep values below the largest multiple of the total. */
  const uint32_t limit = (uint32_t)(0x100000000ULL / EGG_WEIGHT_TOTAL * EGG_WEIGHT_TOTAL - 1);
  uint32_t r;
  do r = rand32();
  while (r > limit);
  r %= EGG_WEIGHT_TOTAL;
  const egg_t *e = &eggs[egg];
  for (int s = e->first_slot; s < e->first_slot + e->slot_count; s++) {
    if (r < egg_slots[s].weight) return s;
    r -= egg_slots[s].weight;
  }
  return e->first_slot + e->slot_count - 1; /* unreachable: weights sum to the total */
}

double egg_slot_chance(int slot) { return (double)egg_slots[slot].weight / EGG_WEIGHT_TOTAL; }

bool game_egg_available(int egg) { return eggs[egg].zone < game.zones_unlocked; }

int game_open_egg(int egg, uint32_t (*rand32)(void), bool *is_new) {
  if (!game_egg_available(egg) || game.coins < eggs[egg].cost) return -1;
  game.coins -= eggs[egg].cost;
  int pet = egg_slots[egg_roll(egg, rand32)].pet;
  *is_new = !game_discovered(pet);
  game.discovered[pet / 8] |= (uint8_t)(1 << (pet % 8));
  if (game.owned[pet] < UINT16_MAX) game.owned[pet]++;
  game.eggs_opened++;
  /* a free team slot is filled right away, so a new player sees the pet at work */
  if (game_team_size() < TEAM_SIZE) game_equip(pet);
  return pet;
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
