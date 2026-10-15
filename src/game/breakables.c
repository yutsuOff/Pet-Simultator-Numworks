#include "breakables.h"
#include "game.h"

/* Diamonds are rare (6 h game): 2 % of the breakables give diamonds. */
const breakable_t breakables[BRK_COUNT] = {
  [BRK_COIN] = {0.5, 0.5, false, 360, 1.0, QUEST_BREAK_COIN},
  [BRK_PILE] = {1.0, 1.0, false, 360, 1.0, -1},
  [BRK_CRATE] = {2.5, 3.0, false, 140, 2.0, QUEST_BREAK_CRATE},
  [BRK_GIFT] = {4.0, 5.0, false, 80, 3.0, QUEST_BREAK_GIFT},
  [BRK_CHEST] = {8.0, 12.0, false, 40, 6.0, QUEST_BREAK_CHEST},
  [BRK_GEM] = {1.5, 1.0, true, 15, 1.0, QUEST_BREAK_DIAMOND},
  [BRK_GEMS] = {4.0, 3.0, true, 5, 2.0, QUEST_BREAK_DIAMOND},
};

double breakable_hp(int type) { return breakables[type].hp * zones[game.zone].pile_hp; }

double breakable_reward(int type) {
  const breakable_t *b = &breakables[type];
  if (b->gives_diamonds) return b->reward * game_diamond_bonus();
  return b->reward * zones[game.zone].pile_reward * game_coin_bonus();
}

int breakable_destroyed(int type, double reward) {
  const breakable_t *b = &breakables[type];
  if (b->gives_diamonds) {
    game.diamonds += reward;
    quest_event(QUEST_EARN_DIAMONDS, reward, 0);
  } else {
    game.coins += reward;
  }
  quest_event(QUEST_BREAK_ANY, 1, 0);
  if (game.zone == game.zones_unlocked - 1) quest_event(QUEST_BREAK_BEST, 1, 0);
  if (b->quest_kind >= 0) quest_event(b->quest_kind, 1, 0);
  return game_roll_drop(b->drop_rarity);
}
