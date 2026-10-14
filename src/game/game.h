#ifndef GAME_GAME_H
#define GAME_GAME_H

#include <stdint.h>

/* Whole persistent game state. Statically allocated (no malloc). */
typedef struct {
  double coins;
  double diamonds;
  uint8_t zone;
} game_t;

extern game_t game;

void game_init(void);

#endif
