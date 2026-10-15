/* Machines of zones 38 to 65 (PS99): upgrade potions, fuse, upgrade enchants, daycare.
 * The golden / rainbow machines are in game.c. */
#include "game.h"

/* Zone indexes (zone number - 1). Golden 7 and rainbow 14 are game choices; the others
 * are spread over the zones added with the rebirth (PS99 has them earlier, in zones 13-28). */
const uint8_t machine_zone[MACHINE_COUNT] = {
  [MACHINE_GOLDEN] = GOLDEN_MACHINE_ZONE,
  [MACHINE_RAINBOW] = RAINBOW_MACHINE_ZONE,
  [MACHINE_POTIONS] = 37,  /* zone 38: Icy Peaks */
  [MACHINE_FUSE] = 49,     /* zone 50: Fire Dojo */
  [MACHINE_ENCHANTS] = 54, /* zone 55: Fairytale Meadows */
  [MACHINE_DAYCARE] = 57,  /* zone 58: Fairy Castle */
};

#define FUSE_YIELD 0.5         /* the result can reach half the total power put in */
#define FUSE_CHOICES 3         /* among the strongest pets under that power */
#define DAYCARE_PILES 150      /* coins: coin piles of the best zone, for a pet as strong as its egg */
#define DAYCARE_DIAMONDS 4
#define DAYCARE_POTION_PERCENT 30
#define DAYCARE_ENCHANT_PERCENT 15

int game_zone_machine(int zone) {
  for (int m = 0; m < MACHINE_COUNT; m++) {
    if (machine_zone[m] == zone) return m;
  }
  return NO_MACHINE;
}

/* --- Upgrade potions / enchants --- */

int game_enchant_free(int enchant) { return game.enchants_owned[enchant] - game_enchant_equipped(enchant); }

bool game_upgrade_potion(int potion) {
  if (potions[potion].tier >= 3 || game.potions_owned[potion] < ITEM_UPGRADE_INPUT) return false;
  game.potions_owned[potion] -= ITEM_UPGRADE_INPUT;
  game_give_potion(potion + 1, 1);
  return true;
}

bool game_upgrade_enchant(int enchant) {
  if (enchants[enchant].tier >= 3 || game_enchant_free(enchant) < ITEM_UPGRADE_INPUT) return false;
  game.enchants_owned[enchant] -= ITEM_UPGRADE_INPUT;
  game_give_enchant(enchant + 1, 1);
  return true;
}

/* --- Fuse --- */

static bool fuse_valid(const team_slot_t *in, int n) {
  if (n < FUSE_MIN || n > FUSE_MAX) return false;
  for (int i = 0; i < n; i++) {
    int p = in[i].pet, v = in[i].variant, same = 0;
    if (p >= PET_COUNT || v >= VARIANT_COUNT || pets[p].huge) return false;
    for (int j = 0; j < n; j++) same += in[j].pet == p && in[j].variant == v;
    if (same > game_machine_available(p, v)) return false;
  }
  return true;
}

double game_fuse_power(const team_slot_t *in, int n) {
  double total = 0;
  for (int i = 0; i < n; i++) total += pets[in[i].pet].power; /* the variant comes from the roll */
  return total * FUSE_YIELD;
}

bool game_fuse(const team_slot_t *in, int n, hatch_t *out) {
  if (!fuse_valid(in, n)) return false;
  double limit = game_fuse_power(in, n);
  /* the FUSE_CHOICES strongest pets of the unlocked eggs under the limit */
  int best[FUSE_CHOICES], found = 0;
  for (int p = 0; p < HUGE_FIRST; p++) {
    if (!game_egg_available(pets[p].egg) || pets[p].power > limit) continue;
    if (found < FUSE_CHOICES) best[found++] = p;
    else if (pets[p].power > pets[best[FUSE_CHOICES - 1]].power) best[FUSE_CHOICES - 1] = p;
    else continue;
    for (int j = found - 1; j > 0 && pets[best[j]].power > pets[best[j - 1]].power; j--) {
      int swap = best[j];
      best[j] = best[j - 1], best[j - 1] = swap;
    }
  }
  int golden = 0, rainbow = 0;
  for (int i = 0; i < n; i++) {
    golden += variant_level(in[i].variant) >= VAR_GOLDEN;
    rainbow += variant_level(in[i].variant) == VAR_RAINBOW;
    game.owned[in[i].pet][in[i].variant]--;
  }
  int pet = found ? best[game_random() % (uint32_t)found] : in[0].pet;
  uint32_t r = game_random() % (uint32_t)n;
  int variant = r < (uint32_t)rainbow ? VAR_RAINBOW : r < (uint32_t)golden ? VAR_GOLDEN : VAR_NORMAL;
  out->pet = (uint8_t)pet;
  out->variant = (uint8_t)variant;
  out->is_new = !game_discovered(pet);
  game.discovered[pet / 8] |= (uint8_t)(1 << (pet % 8));
  if (game.owned[pet][variant] < UINT16_MAX) game.owned[pet][variant]++;
  return true;
}

/* --- Daycare --- */

bool game_daycare_enroll(int pet, int variant) {
  if (pets[pet].huge || game_machine_available(pet, variant) < 1) return false;
  for (int i = 0; i < DAYCARE_SLOTS; i++) {
    if (game.daycare[i].pet != NO_PET) continue;
    game.daycare[i] = (team_slot_t){(uint8_t)pet, (uint8_t)variant};
    game.daycare_ms[i] = DAYCARE_MINUTES * 60000u;
    game.owned[pet][variant]--;
    return true;
  }
  return false;
}

void game_tick_daycare(uint32_t elapsed) {
  for (int i = 0; i < DAYCARE_SLOTS; i++) {
    if (game.daycare[i].pet == NO_PET) continue;
    game.daycare_ms[i] = game.daycare_ms[i] > elapsed ? game.daycare_ms[i] - elapsed : 0;
  }
}

/* Strength of a pet compared with a basic pet of the best egg, 0.2 to 3. */
static double daycare_ratio(int pet, int variant) {
  double ref = pets[egg_slots[eggs[game_best_egg()].first_slot].pet].power;
  double r = game_base_power(pet, variant) / ref;
  return r < 0.2 ? 0.2 : r > 3 ? 3 : r;
}

double game_daycare_coins(int pet, int variant) {
  return DAYCARE_PILES * zones[game.zones_unlocked - 1].pile_reward * daycare_ratio(pet, variant);
}

bool game_daycare_collect(int slot, daycare_reward_t *out) {
  team_slot_t t = game.daycare[slot];
  if (t.pet == NO_PET || game.daycare_ms[slot]) return false;
  double ratio = daycare_ratio(t.pet, t.variant);
  out->coins = game_daycare_coins(t.pet, t.variant) * game_coin_bonus();
  out->diamonds = (double)(long long)(DAYCARE_DIAMONDS * ratio * game_diamond_bonus() + 0.5);
  out->item = -1;
  uint32_t r = game_random() % 100;
  int tier = 1 + (game_random() % 100 < 25) + (game_random() % 100 < 5);
  if (r < DAYCARE_POTION_PERCENT) {
    out->item = (int)(game_random() % POTION_TYPE_COUNT) * 3 + tier - 1;
    game_give_potion(out->item, 1);
  } else if (r < DAYCARE_POTION_PERCENT + DAYCARE_ENCHANT_PERCENT) {
    int e = (int)(game_random() % ENCHANT_TYPE_COUNT) * 3 + tier - 1;
    game_give_enchant(e, 1);
    out->item = 100 + e;
  }
  game.coins += out->coins;
  game.diamonds += out->diamonds;
  if (game.owned[t.pet][t.variant] < UINT16_MAX) game.owned[t.pet][t.variant]++;
  game.daycare[slot].pet = NO_PET;
  return true;
}
