#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "../generated/content.h"

#define TEAM_SIZE 5       /* equipped pets */
#define NO_PET 0xFF
#define START_COINS 100   /* enough for the first Cracked Egg */

/* Whole persistent game state. Statically allocated (no malloc). */
typedef struct {
  double coins;
  double diamonds;
  uint8_t zone;            /* current zone */
  uint8_t zones_unlocked;  /* zones [0, zones_unlocked) are unlocked */
  uint16_t owned[PET_COUNT];                 /* pets are stacked: count per pet */
  uint8_t discovered[(PET_COUNT + 7) / 8];   /* hatched at least once (index) */
  uint8_t team[TEAM_SIZE];                   /* equipped pets, NO_PET if empty */
  uint32_t eggs_opened;
} game_t;

extern game_t game;

void game_init(void);

/* --- Pets and team --- */
bool game_discovered(int pet);
int game_equipped_count(int pet); /* copies of this pet in the team */
int game_team_size(void);
double game_team_power(void);      /* damage per second of the whole team */
bool game_equip(int pet);          /* adds one copy to the team; false if full or none left */
bool game_unequip(int pet);        /* removes one copy from the team */
void game_equip_best(void);        /* fills the team with the strongest pets owned */

/* --- Eggs --- */
/* Weighted draw: returns the slot index (in egg_slots) of the hatched pet.
 * rand32 returns uniform 32-bit values; the draw itself is unbiased. */
int egg_roll(int egg, uint32_t (*rand32)(void));
/* Exact hatch chance of a slot, 0..1. */
double egg_slot_chance(int slot);
bool game_egg_available(int egg);  /* its zone is unlocked */
/* Buys and opens the egg. Returns the pet index, or -1 if not affordable/available.
 * *is_new is set when the pet was never hatched before. */
int game_open_egg(int egg, uint32_t (*rand32)(void), bool *is_new);

/* --- Zones --- */
bool game_unlock_next_zone(void);  /* buys zone zones_unlocked if affordable */

#endif
