#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "../generated/content.h"

#define BASE_TEAM 5       /* equipped pets at the start */
#define MAX_TEAM 8        /* with every "Équipe" upgrade */
#define NO_PET 0xFF
#define START_COINS 100   /* enough for the first Cracked Egg */
#define MAX_HATCH 3       /* eggs opened at once with every "Ouverture multiple" upgrade */

/* Upgrades (AMÉLIO. screen), bought with diamonds. */
enum {
  UPG_DAMAGE, UPG_TEAM,     /* Pets */
  UPG_TAP, UPG_RESPAWN,     /* Farming */
  UPG_LUCK, UPG_MULTI,      /* Œufs */
  UPG_COINS, UPG_DIAMONDS,  /* Richesse */
  UPG_COUNT
};
enum { UPG_CAT_PETS, UPG_CAT_FARMING, UPG_CAT_EGGS, UPG_CAT_WEALTH, UPG_CAT_COUNT };

typedef struct {
  const char *name;
  const char *description;
  uint8_t category;
  uint8_t max_level;
  double base_cost;   /* diamonds for level 1 */
  double cost_growth; /* x per level */
} upgrade_t;

extern const upgrade_t upgrades[UPG_COUNT];

/* Whole persistent game state. Statically allocated (no malloc). */
typedef struct {
  double coins;
  double diamonds;
  uint8_t zone;            /* current zone */
  uint8_t zones_unlocked;  /* zones [0, zones_unlocked) are unlocked */
  uint16_t owned[PET_COUNT];                 /* pets are stacked: count per pet */
  uint8_t discovered[(PET_COUNT + 7) / 8];   /* hatched at least once (index) */
  uint8_t team[MAX_TEAM];                    /* equipped pets, NO_PET if empty */
  uint32_t eggs_opened;
  uint8_t upgrade_level[UPG_COUNT];
  uint8_t egg_rewarded[(EGG_COUNT + 7) / 8]; /* index reward of the egg already given */
} game_t;

extern game_t game;

void game_init(void);

/* --- Pets and team --- */
bool game_discovered(int pet);
int game_discovered_count(void);
int game_team_slots(void);         /* BASE_TEAM + "Équipe" upgrade */
int game_equipped_count(int pet);  /* copies of this pet in the team */
int game_team_size(void);
double game_pet_power(int pet);    /* with the damage upgrade */
double game_team_power(void);      /* damage per second of the whole team */
bool game_equip(int pet);          /* adds one copy to the team; false if full or none left */
bool game_unequip(int pet);        /* removes one copy from the team */
void game_equip_best(void);        /* fills the team with the strongest pets owned */
/* Duplicates: copies beyond one (and beyond the equipped ones) are sold for coins. */
int game_duplicates(int pet);
double game_recycle_value(int pet); /* coins for one copy */
double game_recycle_all(int *count); /* sells every duplicate, returns the coins */

/* --- Eggs --- */
/* Hatch weight of a slot with the luck upgrade: pets rarer than 20 % in the egg
 * get +10 % per level, then the egg is renormalized (the chances shown are exact). */
uint32_t egg_slot_weight(int slot);
double egg_slot_chance(int slot);  /* exact chance, 0..1 */
/* Weighted draw: returns the slot index (in egg_slots). Unbiased. */
int egg_roll(int egg, uint32_t (*rand32)(void));
bool game_egg_available(int egg);  /* its zone is unlocked */
int game_hatch_count(void);        /* eggs opened at once (1 to MAX_HATCH) */
/* Buys and opens one egg. Returns the pet index, or -1 if not affordable/available.
 * *is_new is set when the pet was never hatched before. */
int game_open_egg(int egg, uint32_t (*rand32)(void), bool *is_new);

/* --- Index rewards --- */
#define INDEX_STEP 20      /* every 20 pets discovered: +5 % coins */
bool game_egg_complete(int egg);
double game_egg_reward(int egg);   /* diamonds for completing an egg */
/* Gives the rewards of newly completed eggs. Returns the diamonds given. */
double game_claim_index_rewards(void);

/* --- Zones --- */
bool game_unlock_next_zone(void);  /* buys zone zones_unlocked if affordable */

/* --- Upgrades and bonuses --- */
double upgrade_cost(int id);       /* cost of the next level */
bool game_buy_upgrade(int id);
double game_coin_bonus(void);      /* multiplier: upgrade x index milestones */
double game_diamond_bonus(void);
double game_tap_bonus(void);
uint32_t game_respawn_ms(void);

#endif
