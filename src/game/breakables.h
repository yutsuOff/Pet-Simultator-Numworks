#ifndef GAME_BREAKABLES_H
#define GAME_BREAKABLES_H

/* Breakables of the Farm, from the cheapest to the richest. HP and rewards are
 * multiples of the zone's coin pile (zone_t.pile_hp, pile_reward). Shared by the
 * Farm screen and the balance simulator (tests/balance_sim.c). */
#include <stdbool.h>
#include <stdint.h>

enum { BRK_COIN, BRK_PILE, BRK_CRATE, BRK_GIFT, BRK_CHEST, BRK_GEM, BRK_GEMS, BRK_COUNT };

typedef struct {
  double hp;
  double reward;       /* coins x pile_reward, or diamonds */
  bool gives_diamonds;
  uint16_t weight;     /* spawn chance in per mille, the weights sum to 1000 */
  double drop_rarity;  /* multiplies the potion / enchant drop chance */
  int quest_kind;      /* QUEST_BREAK_COIN... or -1 */
} breakable_t;

extern const breakable_t breakables[BRK_COUNT];

/* Reward of one breakable in the current zone, with every bonus. */
double breakable_reward(int type);
double breakable_hp(int type);
/* Breakable destroyed: reward, quests, drop. Returns the item dropped (see game_roll_drop). */
int breakable_destroyed(int type, double reward);

#endif
