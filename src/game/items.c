/* Potions (temporary boosts) and enchants (permanent while equipped), PS99 values. */
#include "game.h"

/* Enchant slots unlocked by the ranks (PS99 wiki, Enchants page): ranks 1, 3, 5, 8, 11. */
static const uint8_t slot_ranks[MAX_ENCHANT_SLOTS] = {1, 3, 5, 8, 11};

/* Several enchants of the same type: each one counts less (PS99 wiki: 100 %, 60 %,
 * 38.3 %, 27.5 %...). */
static const double same_type_factor[MAX_ENCHANT_SLOTS] = {1.0, 0.6, 0.383, 0.275, 0.2};

/* Drops: chance (in %) of a potion for a breakable of rarity 1 (coin), enchants half. */
#define POTION_DROP_PERCENT 0.4
#define ENCHANT_DROP_PERCENT 0.2

int game_enchant_slots(void) {
  int n = 0;
  for (int i = 0; i < MAX_ENCHANT_SLOTS; i++) n += game.rank + 1 >= slot_ranks[i];
  return n;
}

bool game_use_potion(int potion) {
  if (!game.potions_owned[potion]) return false;
  const potion_t *p = &potions[potion];
  game.potions_owned[potion]--;
  /* the strongest tier stays active; the durations add up */
  if (p->tier > game.boost_tier[p->type]) game.boost_tier[p->type] = p->tier;
  game.boost_ms[p->type] += (uint32_t)p->minutes * 60000u;
  quest_event(QUEST_USE_POTION, 1, p->tier);
  return true;
}

int game_enchant_equipped(int enchant) {
  int n = 0;
  for (int i = 0; i < game_enchant_slots(); i++) n += game.enchant_slot[i] == enchant;
  return n;
}

bool game_equip_enchant(int enchant) {
  if (game_enchant_equipped(enchant) >= game.enchants_owned[enchant]) return false;
  for (int i = 0; i < game_enchant_slots(); i++) {
    if (game.enchant_slot[i] == NO_ITEM) {
      game.enchant_slot[i] = (uint8_t)enchant;
      return true;
    }
  }
  return false;
}

bool game_unequip_enchant(int slot) {
  if (game.enchant_slot[slot] == NO_ITEM) return false;
  game.enchant_slot[slot] = NO_ITEM;
  return true;
}

double game_boost_percent(int type) {
  int tier = game.boost_tier[type];
  if (!tier || !game.boost_ms[type]) return 0;
  return potions[type * 3 + tier - 1].percent;
}

double game_enchant_percent(int type) {
  /* strongest first, then the diminishing factors */
  double values[MAX_ENCHANT_SLOTS];
  int n = 0;
  for (int i = 0; i < game_enchant_slots(); i++) {
    int e = game.enchant_slot[i];
    if (e == NO_ITEM || enchants[e].type != type) continue;
    double v = enchants[e].percent;
    int j = n++;
    while (j > 0 && values[j - 1] < v) values[j] = values[j - 1], j--;
    values[j] = v;
  }
  double total = 0;
  for (int i = 0; i < n; i++) total += values[i] * same_type_factor[i];
  return total;
}

void game_tick_boosts(uint32_t elapsed) {
  for (int t = 0; t < POTION_TYPE_COUNT; t++) {
    if (!game.boost_ms[t]) continue;
    if (game.boost_ms[t] <= elapsed) {
      game.boost_ms[t] = 0;
      game.boost_tier[t] = 0;
    } else {
      game.boost_ms[t] -= elapsed;
    }
  }
}

void game_give_potion(int potion, int n) {
  int v = game.potions_owned[potion] + n;
  game.potions_owned[potion] = (uint16_t)(v > UINT16_MAX ? UINT16_MAX : v);
}

void game_give_enchant(int enchant, int n) {
  int v = game.enchants_owned[enchant] + n;
  game.enchants_owned[enchant] = (uint16_t)(v > UINT16_MAX ? UINT16_MAX : v);
}

static int random_tier(void) { /* I 80 %, II 17 %, III 3 % */
  uint32_t r = game_random() % 100;
  return r < 80 ? 1 : r < 97 ? 2 : 3;
}

int game_roll_drop(double rarity) {
  double r = (game_random() % 1000000) / 10000.0; /* 0..100 % */
  double potion = POTION_DROP_PERCENT * rarity * game_drop_bonus();
  double enchant = ENCHANT_DROP_PERCENT * rarity * game_drop_bonus();
  if (r < potion) {
    int p = (int)(game_random() % POTION_TYPE_COUNT) * 3 + random_tier() - 1;
    game_give_potion(p, 1);
    quest_event(QUEST_COLLECT_POTION, 1, 0);
    return p;
  }
  if (r < potion + enchant) {
    int e = (int)(game_random() % ENCHANT_TYPE_COUNT) * 3 + random_tier() - 1;
    game_give_enchant(e, 1);
    quest_event(QUEST_COLLECT_ENCHANT, 1, 0);
    return 100 + e;
  }
  return -1;
}
