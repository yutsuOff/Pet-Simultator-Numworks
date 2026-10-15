/* Hidden admin menu, opened from anywhere with the code
 *   2 8 1 2  ↑ ↑ ↓ ↓  ← → ← →  OK
 * Gives coins, diamonds, pets, items... to test the game. */
#include "admin.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../platform/platform.h"
#include "../generated/sprites.h"
#include "../ui/ui.h"

static const uint8_t code[] = {KEY_2, KEY_8, KEY_1, KEY_2, KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN,
                               KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT, KEY_OK};
#define CODE_LEN (int)sizeof(code)
static int progress; /* keys of the code typed so far */

static bool open;
static int selected, scroll;
static bool confirm_reset;
static int next_huge;
static char message[48];

enum {
  A_COINS, A_DIAMONDS, A_ZONES, A_EGGS, A_HUGE, A_VARIANTS, A_ITEMS, A_QUESTS, A_RANK, A_INDEX,
  A_UPGRADES, A_REBIRTH, A_RESET, A_CLOSE, A_COUNT
};
static const char *const labels[A_COUNT] = {
  "Coins : + prix de la zone suivante x10",
  "Diamants : +1 000",
  "Débloquer toutes les zones",
  "Ouvrir 10 meilleurs œufs (gratuit)",
  "Donner un Huge",
  "Golden / Rainbow / Shiny de l'équipe",
  "Potions et enchants : +5 de chaque",
  "Finir les quêtes en cours",
  "Rang suivant",
  "Index complet",
  "Améliorations au maximum",
  "Rebirth disponible à nouveau",
  "Réinitialiser la partie",
  "Fermer",
};
#define ROWS 8
#define ROW_H 19

bool admin_is_open(void) { return open; }

bool admin_check_code(const input_t *in) {
  for (int k = 0; k < 64; k++) {
    if (!input_pressed(in, k)) continue;
    if (k == code[progress]) progress++;
    else progress = k == code[0] ? 1 : 0;
    if (progress == CODE_LEN) {
      progress = 0;
      open = true;
      selected = scroll = 0;
      confirm_reset = false;
      return true;
    }
  }
  return false;
}

static void say(const char *text, uint32_t now) {
  char *q = message;
  while (*text && q < message + sizeof(message) - 1) *q++ = *text++;
  *q = 0;
  ui_message(message, now);
}

static void apply(int action, uint32_t now) {
  switch (action) {
    case A_COINS: {
      int z = game.zones_unlocked < ZONE_COUNT ? game.zones_unlocked : ZONE_COUNT - 1;
      game.coins += zones[z].cost * 10 + 1000;
      say("Coins ajoutés", now);
      break;
    }
    case A_DIAMONDS: game.diamonds += 1000, say("+1 000 diamants", now); break;
    case A_ZONES:
      game.zones_unlocked = ZONE_COUNT;
      say("Toutes les zones sont débloquées", now);
      break;
    case A_EGGS: {
      int egg = game_best_egg();
      hatch_t h;
      for (int i = 0; i < 10; i++) {
        game.coins += eggs[egg].cost;
        game_open_egg(egg, &h);
      }
      say("10 œufs ouverts (onglet Pets)", now);
      break;
    }
    case A_HUGE: {
      int p = HUGE_FIRST + next_huge;
      next_huge = (next_huge + 1) % HUGE_COUNT;
      game.owned[p][VAR_NORMAL]++;
      game.discovered[p / 8] |= (uint8_t)(1 << (p % 8));
      char name[40];
      say(ui_pet_name(p, VAR_NORMAL, name), now);
      break;
    }
    case A_VARIANTS:
      /* one golden, rainbow and shiny copy of every pet in the team */
      for (int i = 0; i < game_team_slots(); i++) {
        int p = game.team[i].pet;
        if (p == NO_PET) continue;
        game.owned[p][VAR_GOLDEN]++, game.owned[p][VAR_RAINBOW]++, game.owned[p][VAR_SHINY]++;
      }
      say("Variantes ajoutées (onglet Pets)", now);
      break;
    case A_ITEMS:
      for (int i = 0; i < POTION_COUNT; i++) game_give_potion(i, 5);
      for (int i = 0; i < ENCHANT_COUNT; i++) game_give_enchant(i, 5);
      say("Objets ajoutés (onglet 8)", now);
      break;
    case A_QUESTS:
      quests_refill();
      for (int s = 0; s < QUEST_SLOTS; s++) {
        if (game.quest_goal[s] != NO_QUEST) game.quest_progress[s] = rank_goals[game.quest_goal[s]].amount;
      }
      say("Quêtes terminées", now);
      break;
    case A_RANK:
      if (game.rank + 1 < RANK_COUNT) game.rank++, game.next_reward = 0, game.star_bank = 0;
      for (int s = 0; s < QUEST_SLOTS; s++) game.quest_goal[s] = NO_QUEST;
      quests_refill();
      say(ranks[game.rank].name, now);
      break;
    case A_INDEX:
      for (int p = 0; p < PET_COUNT; p++) game.discovered[p / 8] |= (uint8_t)(1 << (p % 8));
      game_claim_index_rewards();
      say("Index complet", now);
      break;
    case A_UPGRADES:
      for (int id = 0; id < UPG_COUNT; id++) game.upgrade_level[id] = upgrades[id].max_level;
      say("Améliorations au maximum", now);
      break;
    case A_REBIRTH:
      game.rebirths = 0;
      say("Rebirth à nouveau possible", now);
      break;
    case A_RESET:
      if (!confirm_reset) {
        confirm_reset = true;
        say("Encore OK pour TOUT effacer", now);
        return;
      }
      game_init();
      quests_refill();
      open = false;
      say("Partie réinitialisée", now);
      break;
    default: open = false; break;
  }
  confirm_reset = false;
}

int admin_update(const input_t *in, uint32_t now) {
  int r = 0;
  if (input_pressed(in, KEY_BACK) || input_pressed(in, KEY_HOME)) {
    open = false;
    return SCREEN_REDRAW_ALL;
  }
  if (input_pressed(in, KEY_UP) && selected > 0) selected--, confirm_reset = false, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_DOWN) && selected < A_COUNT - 1) selected++, confirm_reset = false, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
    apply(selected, now);
    r |= SCREEN_REDRAW_ALL;
  }
  if (selected < scroll) scroll = selected;
  if (selected >= scroll + ROWS) scroll = selected - ROWS + 1;
  return r;
}

void admin_draw(void) {
  int x = 8, y = CONTENT_Y + 3, w = 304, h = CONTENT_H - 6;
  gfx_round_rect(x, y, w, h, 10, RGB(120, 20, 30));
  gfx_round_gradient(x + 2, y + 2, w - 4, h - 4, 8, RGB(40, 30, 50), RGB(20, 16, 30));
  gfx_text_shadow(&font_medium, x + 12, y + 6, "MENU ADMIN", COL_GOLD, COL_SHADOW);
  gfx_text_aligned(&font_small, x, y + 8, w - 12, ALIGN_RIGHT, "OK : appliquer · Retour : fermer",
                   RGB(170, 160, 190));
  for (int i = scroll; i < A_COUNT && i < scroll + ROWS; i++) {
    int ry = y + 26 + (i - scroll) * ROW_H;
    bool sel = i == selected;
    if (sel) gfx_round_rect(x + 8, ry - 1, w - 16, ROW_H - 1, 6, RGB(90, 60, 130));
    color_t c = i == A_RESET ? RGB(255, 120, 120) : sel ? COL_WHITE : RGB(200, 195, 215);
    gfx_text(&font_small, x + 16, ry + 2, i == A_RESET && confirm_reset ? "Réinitialiser : OK pour confirmer" : labels[i], c);
  }
}
