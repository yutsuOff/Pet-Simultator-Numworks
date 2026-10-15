/* QUÊTES screen: the current rank (PS99 ranks), its reward path and the 4 quests
 * (1 to 4 stars). Quests complete on their own while playing. */
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

static void enter(void) { quests_refill(); }

static int update(const input_t *in, uint32_t now) {
  int r = (now / 500 != (now - 33) / 500) ? SCREEN_REDRAW_CONTENT : 0; /* progress bars */
  if (input_pressed(in, KEY_DOWN)) r |= SCREEN_FOCUS_TABS;
  return r;
}

static char *put(char *q, const char *s) {
  while (*s) *q++ = *s++;
  return q;
}

static char *put_num(char *q, double v) {
  numfmt(v, q);
  while (*q) q++;
  return q;
}

static void draw_stars(int x, int y, int n, color_t c) {
  for (int i = 0; i < n; i++) gfx_text(&font_medium, x + i * 13, y, "★", c);
}

static void draw_header(void) {
  int x = 6, y = CONTENT_Y + 4, w = 308;
  ui_panel(x, y, w, 46);
  const rank_t *r = &ranks[game.rank];
  char buf[64], *q = buf;
  q = put(put_num(put(q, "Rang "), game.rank + 1), " · ");
  put(q, r->name)[0] = 0;
  gfx_text(&font_medium, x + 10, y + 4, buf, COL_TEXT);
  int have = quest_rank_stars(), total = r->stars_total;
  q = put_num(put(buf, "★ "), have > total ? total : have);
  *q++ = '/';
  put_num(q, total)[0] = 0;
  gfx_text_aligned(&font_medium, x, y + 4, w - 10, ALIGN_RIGHT, buf, COL_GOLD_DARK);
  gfx_progress_bar(x + 10, y + 21, w - 20, 6, total ? (have > total ? 1000 : have * 1000 / total) : 1000, COL_GOLD,
                   COL_HP_BACK);
  /* next reward */
  if (game.next_reward < r->reward_count) {
    const rank_reward_t *rw = &rank_rewards[r->first_reward + game.next_reward];
    int ix = x + 10;
    if (rw->kind == REWARD_DIAMONDS) {
      gfx_sprite(&spr_gem, ix, y + 29);
      q = put_num(put(buf, ""), rw->quantity);
      put(q, " diamants")[0] = 0;
    } else {
      int item = rw->kind == REWARD_POTION ? rw->item : ITEM_ENCHANT + rw->item;
      gfx_sprite_half(ui_item_sprite(item), ix - 1, y + 26);
      char name[40];
      ui_item_name(item, name);
      q = put(buf, name);
      if (rw->quantity > 1) q = put_num(put(q, " x"), rw->quantity);
      *q = 0;
    }
    char line[80];
    q = put(put(line, "Ensuite : "), buf);
    q = put_num(put(q, "  ("), rw->stars);
    put(q, " ★)")[0] = 0;
    gfx_text(&font_small, ix + 18, y + 29, line, COL_TEXT);
  } else {
    gfx_text(&font_small, x + 10, y + 29, "Rang maximum atteint pour l'instant", COL_TEXT_DIM);
  }
}

static void draw_quest(int s, int y) {
  int x = 6, w = 308, h = 33;
  int g = game.quest_goal[s];
  ui_card(x, y, w, h, false, g == NO_QUEST ? RGB(205, 210, 222) : COL_PANEL_TOP,
          g == NO_QUEST ? RGB(185, 190, 205) : COL_PANEL_BOT);
  gfx_round_gradient(x + 5, y + 5, 56, h - 10, 8, RGB(255, 225, 90), COL_GOLD_DARK);
  draw_stars(x + 7 + (4 - (s + 1)) * 6, y + 8, s + 1, COL_WHITE);
  if (g == NO_QUEST) {
    gfx_text(&font_small, x + 68, y + 10,
             s == 2 && game.rank < 1 ? "Quête 3 ★ : au rang 2" : s == 3 && game.rank < 4 ? "Quête 4 ★ : au rang 5"
                                                                                          : "Bientôt une nouvelle quête",
             COL_TEXT_DIM);
    return;
  }
  char text[64];
  quest_text(s, text);
  gfx_text(&font_small, x + 68, y + 3, text, COL_TEXT);
  uint32_t amount = rank_goals[g].amount, done = game.quest_progress[s];
  if (done > amount) done = amount;
  gfx_progress_bar(x + 68, y + 19, w - 140, 7, (int)((uint64_t)done * 1000 / amount), COL_GREEN, COL_HP_BACK);
  char buf[32], *q = put_num(buf, done);
  *q++ = '/';
  put_num(q, amount)[0] = 0;
  gfx_text_aligned(&font_small, x, y + 16, w - 10, ALIGN_RIGHT, buf, COL_TEXT_DIM);
}

static void draw(void) {
  draw_header();
  for (int s = 0; s < QUEST_SLOTS; s++) draw_quest(s, CONTENT_Y + 54 + s * 35);
}

const screen_t screen_quests = {"Quêtes", &spr_tab_quests, enter, update, draw};
