/* Ranks and quests (PS99): 4 quest slots worth 1 to 4 stars, goals drawn from the
 * pool of the current rank; stars pay the rank rewards one by one, then the next rank. */
#include "game.h"

static uint16_t last_goal[QUEST_SLOTS] = {NO_QUEST, NO_QUEST, NO_QUEST, NO_QUEST}; /* a quest is not given again right after */

void quest_event(int kind, double amount, int tier) {
  for (int s = 0; s < QUEST_SLOTS; s++) {
    int g = game.quest_goal[s];
    if (g == NO_QUEST || rank_goals[g].kind != kind) continue;
    if (kind == QUEST_USE_POTION && tier < rank_goals[g].potion_tier) continue;
    double p = game.quest_progress[s] + amount;
    game.quest_progress[s] = p > 4e9 ? 4000000000u : (uint32_t)p;
  }
}

static bool feasible(int g) {
  switch (rank_goals[g].kind) {
    case QUEST_MAKE_GOLDEN: return game.zones_unlocked > GOLDEN_MACHINE_ZONE;
    case QUEST_MAKE_RAINBOW: return game.zones_unlocked > RAINBOW_MACHINE_ZONE;
    case QUEST_UNLOCK_AREA: return game.zones_unlocked < ZONE_COUNT;
    case QUEST_USE_POTION:
      for (int p = 0; p < POTION_COUNT; p++) {
        if (game.potions_owned[p] && potions[p].tier >= rank_goals[g].potion_tier) return true;
      }
      return false;
    default: return true;
  }
}

static bool kind_active(int kind, int except) {
  for (int s = 0; s < QUEST_SLOTS; s++) {
    if (s != except && game.quest_goal[s] != NO_QUEST && rank_goals[game.quest_goal[s]].kind == kind) return true;
  }
  return false;
}

void quests_refill(void) {
  const rank_t *r = &ranks[game.rank];
  for (int s = 0; s < QUEST_SLOTS; s++) {
    if (game.quest_goal[s] != NO_QUEST) continue;
    int candidates[64], n = 0;
    for (int g = r->first_goal; g < r->first_goal + r->goal_count && n < 64; g++) {
      if (rank_goals[g].stars != s + 1 || g == last_goal[s]) continue;
      if (kind_active(rank_goals[g].kind, s) || !feasible(g)) continue;
      candidates[n++] = g;
    }
    if (!n) continue; /* e.g. no 3 star quest in rank 1 */
    game.quest_goal[s] = (uint16_t)candidates[game_random() % (uint32_t)n];
    game.quest_progress[s] = 0;
  }
}

static void give(const rank_reward_t *w) {
  if (w->kind == REWARD_DIAMONDS) game.diamonds += w->quantity;
  else if (w->kind == REWARD_POTION) game_give_potion(w->item, w->quantity);
  else game_give_enchant(w->item, w->quantity);
}

int quests_update(bool *ranked_up) {
  int stars = 0;
  *ranked_up = false;
  for (int s = 0; s < QUEST_SLOTS; s++) {
    int g = game.quest_goal[s];
    if (g == NO_QUEST || game.quest_progress[s] < rank_goals[g].amount) continue;
    stars += rank_goals[g].stars;
    last_goal[s] = (uint16_t)g;
    game.quest_goal[s] = NO_QUEST;
  }
  game.star_bank = (uint16_t)(game.star_bank + stars);
  for (;;) {
    const rank_t *r = &ranks[game.rank];
    if (game.next_reward >= r->reward_count) {
      if (game.rank + 1 >= RANK_COUNT) break; /* last rank: stars are kept */
      game.rank++;
      game.next_reward = 0;
      *ranked_up = true;
      continue;
    }
    const rank_reward_t *w = &rank_rewards[r->first_reward + game.next_reward];
    if (game.star_bank < w->stars) break;
    game.star_bank = (uint16_t)(game.star_bank - w->stars);
    give(w);
    game.next_reward++;
  }
  quests_refill();
  return stars;
}

int quest_rank_stars(void) {
  const rank_t *r = &ranks[game.rank];
  int n = game.star_bank;
  for (int i = 0; i < game.next_reward; i++) n += rank_rewards[r->first_reward + i].stars;
  return n;
}

/* --- Text --- */

static char *put(char *q, const char *s) {
  while (*s) *q++ = *s++;
  return q;
}

static char *put_num(char *q, uint32_t v) {
  char tmp[12];
  int n = 0;
  do tmp[n++] = (char)('0' + v % 10), v /= 10;
  while (v);
  while (n) *q++ = tmp[--n];
  return q;
}

void quest_text(int slot, char *buf) {
  int g = game.quest_goal[slot];
  if (g == NO_QUEST) {
    put(buf, "")[0] = 0;
    return;
  }
  const rank_goal_t *goal = &rank_goals[g];
  static const char *const verbs[QUEST_KIND_COUNT][2] = {
    [QUEST_BREAK_ANY] = {"Casse ", " objets"},
    [QUEST_BREAK_BEST] = {"Casse ", " objets (meilleure zone)"},
    [QUEST_BREAK_DIAMOND] = {"Casse ", " objets à diamants"},
    [QUEST_BREAK_COIN] = {"Casse ", " pièces"},
    [QUEST_BREAK_CRATE] = {"Casse ", " caisses"},
    [QUEST_BREAK_GIFT] = {"Casse ", " cadeaux"},
    [QUEST_BREAK_CHEST] = {"Casse ", " coffres"},
    [QUEST_EARN_DIAMONDS] = {"Gagne ", " diamants"},
    [QUEST_HATCH_ANY] = {"Ouvre ", " œufs"},
    [QUEST_HATCH_BEST] = {"Ouvre ", " de tes meilleurs œufs"},
    [QUEST_HATCH_LEGENDARY] = {"Obtiens ", " pets Légendaires+"},
    [QUEST_MAKE_GOLDEN] = {"Fabrique ", " pets golden"},
    [QUEST_MAKE_RAINBOW] = {"Fabrique ", " pets rainbow"},
    [QUEST_UNLOCK_AREA] = {"Débloque ", " zone"},
    [QUEST_COLLECT_POTION] = {"Trouve ", " potions"},
    [QUEST_COLLECT_ENCHANT] = {"Trouve ", " enchantements"},
    [QUEST_USE_POTION] = {"Utilise ", " potions niv. "},
  };
  char *q = put_num(put(buf, verbs[goal->kind][0]), goal->amount);
  q = put(q, verbs[goal->kind][1]);
  if (goal->kind == QUEST_USE_POTION) q = put(q, goal->potion_tier == 1 ? "I+" : goal->potion_tier == 2 ? "II+" : "III");
  *q = 0;
}
