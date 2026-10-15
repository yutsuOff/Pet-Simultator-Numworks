/* AMÉLIORATIONS screen: permanent upgrades bought with diamonds, in 4 categories.
 * ←/→ change the category, ↑/↓ choose the upgrade, OK buys the next level. */
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define CARD_X 8
#define CARD_W 304
#define CARD_H 80
#define CARDS_Y (CONTENT_Y + 28)

static const char *const category_names[UPG_CAT_COUNT] = {"Pets", "Farming", "Œufs", "Richesse"};
static const color_t category_colors[UPG_CAT_COUNT] = {RGB(240, 110, 90), RGB(70, 190, 90), RGB(240, 170, 40),
                                                       RGB(60, 150, 240)};
static int category, selected;
static uint8_t list[UPG_COUNT];
static int count;

static void build_list(void) {
  count = 0;
  for (int i = 0; i < UPG_COUNT; i++) {
    if (upgrades[i].category == category) list[count++] = (uint8_t)i;
  }
  if (selected >= count) selected = count - 1;
}

static void enter(void) { build_list(); }

static int update(const input_t *in, uint32_t now) {
  int r = 0;
  if (input_pressed(in, KEY_LEFT) && category > 0) category--, selected = 0, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT) && category < UPG_CAT_COUNT - 1) category++, selected = 0, r |= SCREEN_REDRAW_CONTENT;
  build_list();
  if (input_pressed(in, KEY_UP) && selected > 0) selected--, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_DOWN)) {
    if (selected < count - 1) selected++, r |= SCREEN_REDRAW_CONTENT;
    else return SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
    int id = list[selected];
    if (game.upgrade_level[id] >= upgrades[id].max_level) ui_message("Niveau maximum atteint", now);
    else if (!game_buy_upgrade(id)) ui_message("Pas assez de diamants", now);
    else ui_message("Amélioration achetée !", now);
    r |= SCREEN_REDRAW_ALL;
  }
  return r;
}

/* --- Effect text: "+30%", "6 pets", "x2.5", "2.2 s"... --- */

static char *put(char *q, const char *s) {
  while (*s) *q++ = *s++;
  return q;
}

static char *put_int(char *q, int v) {
  char tmp[8];
  int n = 0;
  do tmp[n++] = (char)('0' + v % 10), v /= 10;
  while (v);
  while (n) *q++ = tmp[--n];
  return q;
}

static char *put_tenths(char *q, int tenths) { /* 25 -> "2.5" */
  q = put_int(q, tenths / 10);
  *q++ = '.';
  *q++ = (char)('0' + tenths % 10);
  return q;
}

static char *effect(int id, int level, char *q) {
  switch (id) {
    case UPG_DAMAGE: return put(put_int(put(q, "+"), 15 * level), "%");
    case UPG_TEAM: return put(put_int(q, BASE_TEAM + level), " pets");
    case UPG_TAP: return put_tenths(put(q, "×"), 10 + 5 * level);
    case UPG_RESPAWN: return put(put_tenths(q, 30 - 4 * level), " s");
    case UPG_LUCK: return put(put_int(put(q, "+"), 10 * level), "%");
    case UPG_MULTI: return put(put_int(q, 1 + level), level ? " œufs" : " œuf");
    case UPG_COINS: return put(put_int(put(q, "+"), 10 * level), "%");
    default: return put(put_int(put(q, "+"), 20 * level), "%");
  }
}

static void draw_card(int i, int y) {
  int id = list[i];
  const upgrade_t *u = &upgrades[id];
  bool sel = i == selected;
  int lvl = game.upgrade_level[id];
  bool maxed = lvl >= u->max_level;
  color_t cc = category_colors[category];
  ui_card(CARD_X, y, CARD_W, CARD_H - 6, sel, COL_PANEL_TOP, COL_PANEL_BOT);
  if (sel) y -= 2;
  gfx_round_rect(CARD_X + 6, y + 8, 6, CARD_H - 22, 3, cc);
  gfx_text(&font_medium, CARD_X + 18, y + 6, u->name, COL_TEXT);
  gfx_text(&font_small, CARD_X + 18, y + 24, u->description, COL_TEXT_DIM);
  /* level pips */
  for (int k = 0; k < u->max_level; k++) {
    int px = CARD_X + 18 + k * 13;
    gfx_round_rect(px, y + 41, 11, 8, 3, k < lvl ? cc : RGB(205, 212, 228));
  }
  char text[40], *q = text;
  q = effect(id, lvl, q);
  if (!maxed) q = effect(id, lvl + 1, put(q, "  →  "));
  *q = 0;
  gfx_text(&font_small, CARD_X + 18, y + 54, text, COL_TEXT);
  /* price button */
  int bx = CARD_X + CARD_W - 92, by = y + 22, bw = 82, bh = 26;
  if (maxed) {
    gfx_round_gradient(bx, by, bw, bh, 13, RGB(255, 225, 90), COL_GOLD_DARK);
    gfx_text_aligned(&font_medium, bx, by + 5, bw, ALIGN_CENTER, "MAX", COL_WHITE);
  } else {
    double cost = upgrade_cost(id);
    bool can = game.diamonds >= cost;
    gfx_round_gradient(bx, by, bw, bh, 13, can ? RGB(110, 225, 120) : RGB(190, 195, 210),
                       can ? COL_GREEN_DARK : RGB(140, 145, 165));
    char num[16];
    numfmt(cost, num);
    int w = gfx_text_width(&font_medium, num) + spr_gem.width + 4;
    gfx_sprite(&spr_gem, bx + (bw - w) / 2, by + 6);
    gfx_text(&font_medium, bx + (bw - w) / 2 + spr_gem.width + 4, by + 5, num, COL_WHITE);
  }
  char lv[16], *l = put(lv, "Niv. ");
  l = put_int(put(put_int(l, lvl), "/"), u->max_level);
  *l = 0;
  gfx_text_aligned(&font_small, CARD_X, y + 6, CARD_W - 14, ALIGN_RIGHT, lv, COL_TEXT_DIM);
}

static void draw(void) {
  /* categories */
  int x = 8;
  for (int c = 0; c < UPG_CAT_COUNT; c++) {
    int w = gfx_text_width(&font_medium, category_names[c]) + 20;
    bool cur = c == category;
    color_t col = category_colors[c];
    gfx_round_gradient(x, CONTENT_Y + 4, w, 20, 10, cur ? gfx_mix(col, COL_WHITE, 80) : COL_BAR_LIGHT,
                       cur ? col : COL_BAR);
    gfx_text(&font_medium, x + 10, CONTENT_Y + 6, category_names[c], cur ? COL_WHITE : RGB(170, 180, 215));
    x += w + 6;
  }
  for (int i = 0; i < count; i++) draw_card(i, CARDS_Y + i * CARD_H);
}

const screen_t screen_upgrades = {"Amélio.", &spr_tab_upgrades, enter, update, draw};
