#include "game.h"

game_t game;

void game_init(void) {
  game.coins = 0;
  game.diamonds = 0;
  game.zone = 0;
}
