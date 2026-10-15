#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "../generated/content.h"

#define BASE_TEAM 5       /* equipped pets at the start */
#define MAX_TEAM 8        /* with every "Équipe" upgrade */
#define NO_PET 0xFF
#define NO_ITEM 0xFF
#define START_COINS 100   /* enough for the first Cracked Egg */
#define MAX_HATCH 3       /* eggs opened at once with every "Ouverture multiple" upgrade */

/* --- Pet variants: (normal, golden, rainbow) x (shiny or not) --- */
enum { VAR_NORMAL, VAR_GOLDEN, VAR_RAINBOW, VAR_SHINY /* + level */, VARIANT_COUNT = 6 };
static inline int variant_level(int v) { return v % 3; }
static inline bool variant_shiny(int v) { return v >= VAR_SHINY; }

/* Hatch odds of the variants and huges (per million, rolled on every egg). */
#define RAINBOW_PER_MILLION 5000  /* 0.5 % */
#define GOLDEN_PER_MILLION 20000  /* 2 % */
#define SHINY_PER_MILLION 1000    /* 1 in 1,000 */
/* Huges: each variant has its own odds (1 in N per egg), then one of the HUGE_COUNT at
 * random. Normal 50k, golden 75k, shiny 100k, rainbow 125k, golden shiny 150k, rainbow shiny 175k. */
extern const uint32_t huge_odds[VARIANT_COUNT];

/* Machines (PS99). Each one stands in the last Farm slot of its zone. */
enum { MACHINE_GOLDEN, MACHINE_RAINBOW, MACHINE_POTIONS, MACHINE_FUSE, MACHINE_ENCHANTS, MACHINE_DAYCARE, MACHINE_COUNT };
extern const uint8_t machine_zone[MACHINE_COUNT]; /* zone index */
#define NO_MACHINE (-1)
#define MACHINE_INPUT 10          /* golden / rainbow: 10 identical pets -> 1 of the next level */
#define GOLDEN_MACHINE_ZONE 6     /* zone 7: Farm */
#define RAINBOW_MACHINE_ZONE 13   /* zone 14: Mushroom Field */
#define ITEM_UPGRADE_INPUT 3      /* PS99 upgrade machines: 3 of tier N -> 1 of tier N+1 */
#define FUSE_MIN 3                /* PS99 Fuse Machine: 3 pets or more (up to FUSE_MAX here) */
#define FUSE_MAX 12
#define DAYCARE_SLOTS 5
#define DAYCARE_MINUTES 10        /* of play time, like the potions */

#define REBIRTH_ZONE 37           /* zones (1-based) up to 37 are open without rebirth */
#define MAX_REBIRTH 1             /* zones 38 and above need 1 rebirth */
#define MAX_ENCHANT_SLOTS 5
#define QUEST_SLOTS 4             /* 1, 2, 3 and 4 star quests (PS99: 3 from rank 2, 4 from rank 5) */
#define NO_QUEST 0xFFFF

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

typedef struct {
  uint8_t pet, variant;
} team_slot_t;

/* Whole persistent game state. Statically allocated (no malloc). */
typedef struct {
  double coins;
  double diamonds;
  uint8_t zone;            /* current zone */
  uint8_t zones_unlocked;  /* zones [0, zones_unlocked) are unlocked */
  uint16_t owned[PET_COUNT][VARIANT_COUNT];  /* pets are stacked: count per pet and variant */
  uint8_t discovered[(PET_COUNT + 7) / 8];   /* hatched at least once (index) */
  team_slot_t team[MAX_TEAM];                /* equipped pets, pet NO_PET if empty */
  uint32_t eggs_opened;
  uint8_t upgrade_level[UPG_COUNT];
  uint8_t egg_rewarded[(EGG_COUNT + 7) / 8]; /* index reward of the egg already given */
  /* items */
  uint16_t potions_owned[POTION_COUNT];
  uint16_t enchants_owned[ENCHANT_COUNT];
  uint8_t enchant_slot[MAX_ENCHANT_SLOTS];   /* equipped enchant, NO_ITEM if empty */
  uint8_t boost_tier[POTION_TYPE_COUNT];     /* active potion tier per type, 0 if none */
  uint32_t boost_ms[POTION_TYPE_COUNT];      /* time left (only counts while playing) */
  /* ranks and quests */
  uint8_t rank;                              /* index in ranks[] */
  uint8_t next_reward;                       /* next reward of the rank */
  uint16_t star_bank;                        /* stars earned, not spent on rewards yet */
  uint16_t quest_goal[QUEST_SLOTS];          /* index in rank_goals, NO_QUEST if none */
  uint32_t quest_progress[QUEST_SLOTS];
  uint8_t rebirths;
  /* daycare: enrolled pets leave the inventory until collected */
  team_slot_t daycare[DAYCARE_SLOTS];        /* pet NO_PET if empty */
  uint32_t daycare_ms[DAYCARE_SLOTS];        /* time left, 0 when ready */
} game_t;

extern game_t game;
/* Random source of the game (platform_random on the calculator, fixed in tests). */
extern uint32_t (*game_random)(void);
/* PC screenshots only: the next egg gives a huge. */
extern bool game_debug_next_huge;

void game_init(void);

/* --- Pets and team --- */
bool game_discovered(int pet);
int game_discovered_count(void);
int game_owned_total(int pet);      /* every variant */
int game_team_slots(void);          /* BASE_TEAM + "Équipe" upgrade */
int game_equipped_count(int pet, int variant);
int game_team_size(void);
double game_pet_power(int pet, int variant); /* with every bonus */
/* Before bonuses. A huge is a percentage of the best other pet owned
 * (200 %, golden 225 %, shiny 250 %, rainbow 275 %, golden shiny 300 %, rainbow shiny 325 %). */
double game_base_power(int pet, int variant);
double game_team_power(void);       /* damage per second of the whole team */
bool game_equip(int pet, int variant);   /* adds one copy to the team; false if full or none left */
bool game_unequip(int pet, int variant); /* removes one copy from the team */
void game_equip_best(void);         /* fills the team with the strongest pets owned */
/* Duplicates: copies beyond one (and beyond the equipped ones) are sold for coins. */
int game_duplicates(int pet, int variant);
double game_recycle_value(int pet, int variant); /* coins for one copy */
double game_recycle_all(int *count);  /* sells every duplicate, returns the coins */

/* --- Machines --- */
int game_machine_available(int pet, int variant); /* copies that can go in a machine */
/* Turns MACHINE_INPUT copies of (pet, variant) into 1 of the next level. */
bool game_machine_make(int pet, int variant);

typedef struct {
  uint8_t pet, variant;
  bool is_new;
} hatch_t;

/* Machine standing in a zone, or NO_MACHINE. */
int game_zone_machine(int zone);
/* Potions / enchants machines: ITEM_UPGRADE_INPUT of a tier I or II item -> 1 of the next tier. */
int game_enchant_free(int enchant); /* copies not equipped */
bool game_upgrade_potion(int potion);
bool game_upgrade_enchant(int enchant);
/* Fuse machine: FUSE_MIN..FUSE_MAX pets (not equipped, no huge) -> 1 stronger pet. The result
 * is one of the 3 strongest pets of the unlocked eggs whose power is at most FUSE_YIELD x the
 * total power put in; golden / rainbow odds are the share of golden / rainbow pets put in. */
double game_fuse_power(const team_slot_t *in, int n); /* power the result can reach */
bool game_fuse(const team_slot_t *in, int n, hatch_t *out);
/* Daycare: a pet (not equipped, no huge) stays DAYCARE_MINUTES, then gives coins,
 * diamonds and sometimes an item, according to its power. */
bool game_daycare_enroll(int pet, int variant);
void game_tick_daycare(uint32_t elapsed_ms);
typedef struct {
  double coins, diamonds;
  int item; /* potion, ITEM 100 + enchant, or -1 */
} daycare_reward_t;
double game_daycare_coins(int pet, int variant); /* expected coins (shown before enrolling) */
bool game_daycare_collect(int slot, daycare_reward_t *out);

/* --- Eggs --- */
/* Hatch weight of a slot with luck: pets rarer than 20 % in the egg get +luck %,
 * then the egg is renormalized (the chances shown are exact). */
uint32_t egg_slot_weight(int slot);
double egg_slot_chance(int slot);  /* exact chance, 0..1 */
/* Weighted draw: returns the slot index (in egg_slots). Unbiased. */
int egg_roll(int egg, uint32_t (*rand32)(void));
bool game_egg_available(int egg);  /* its zone is unlocked */
int game_best_egg(void);           /* the most expensive egg available */
int game_hatch_count(void);        /* eggs opened at once (1 to MAX_HATCH) */
/* Buys and opens one egg (huge 1/50,000, then variants). False if not affordable. */
bool game_open_egg(int egg, hatch_t *out);

/* --- Index rewards --- */
#define INDEX_STEP 20      /* every 20 pets discovered: +5 % coins */
bool game_egg_complete(int egg);
double game_egg_reward(int egg);   /* diamonds for completing an egg */
double game_claim_index_rewards(void); /* rewards of newly completed eggs, returns the diamonds */

/* --- Zones and rebirth --- */
bool game_unlock_next_zone(void);  /* buys zone zones_unlocked if affordable (and rebirth done) */
bool game_zone_needs_rebirth(int zone);
bool game_can_rebirth(void);
void game_rebirth(void);           /* coins and zones back to the start, +75 % pet strength */

/* --- Upgrades and bonuses (upgrades x potions x enchants x rebirths) --- */
double upgrade_cost(int id);       /* cost of the next level */
bool game_buy_upgrade(int id);
double game_coin_bonus(void);
double game_diamond_bonus(void);
double game_tap_bonus(void);
double game_luck_percent(void);    /* extra weight of the rare pets */
double game_drop_bonus(void);      /* treasure hunter */
uint32_t game_respawn_ms(void);

/* --- Items (items.c) --- */
int game_enchant_slots(void);      /* from the rank */
bool game_use_potion(int potion);
bool game_equip_enchant(int enchant);   /* in the first free slot */
bool game_unequip_enchant(int slot);
int game_enchant_equipped(int enchant); /* copies in the slots */
double game_boost_percent(int potion_type);   /* active potion */
double game_enchant_percent(int enchant_type);/* equipped enchants, diminishing returns */
void game_tick_boosts(uint32_t elapsed_ms);
/* Random drop when a breakable breaks (rarity: 1 coin .. 6 chest). Returns the potion
 * (0..) or enchant (100..) index given, or -1. */
int game_roll_drop(double rarity);
void game_give_potion(int potion, int n);
void game_give_enchant(int enchant, int n);

/* --- Quests and ranks (quests.c) --- */
void quest_event(int kind, double amount, int tier); /* progress of the active quests */
void quests_refill(void);          /* gives a goal to the empty quest slots */
/* Completes finished quests and pays the rewards. Returns the stars earned (0 if none);
 * *ranked_up is set on a new rank. */
int quests_update(bool *ranked_up);
int quest_rank_stars(void);        /* stars already spent in the current rank */
void quest_text(int slot, char *buf); /* "Casse 25 objets" */

#endif
