/* OBJETS screen: potions (temporary boosts, PS99 values) and enchants (permanent while
 * equipped, slots unlocked by the ranks). Tiers I to III. */
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define CELL_W 58
#define CELL_H 28
#define GRID_X 6
#define GRID_Y (CONTENT_Y + 26)
#define PANEL_X 192
#define PANEL_W 122

static bool enchant_tab; /* false: potions */
static bool on_header;
static int sel_type, sel_tier; /* tier 0..2 */
static char message[48];

static int type_count(void) { return enchant_tab ? ENCHANT_TYPE_COUNT : POTION_TYPE_COUNT; }
static int item_index(void) { return sel_type * 3 + sel_tier; }

static void enter(void) { on_header = false; }

static int press(uint32_t now) {
  int i = item_index();
  if (!enchant_tab) {
    if (!game_use_potion(i)) ui_message("Tu n'as pas cette potion", now);
    else ui_message("Potion activée !", now);
    return SCREEN_REDRAW_ALL;
  }
  /* OK equips one more copy (several copies of the same enchant can be equipped) */
  if (!game.enchants_owned[i]) {
    ui_message("Tu n'as pas cet enchant", now);
  } else if (game_enchant_equipped(i) >= game.enchants_owned[i]) {
    ui_message("Tous tes exemplaires sont équipés", now);
  } else if (!game_equip_enchant(i)) {
    /* next slot at ranks 3, 5, 8, 11 (PS99) */
    static const int next_rank[MAX_ENCHANT_SLOTS] = {3, 5, 8, 11, 0};
    int next = next_rank[game_enchant_slots() - 1];
    char *q = message;
    for (const char *s = "Emplacements pleins"; *s;) *q++ = *s++;
    if (next) {
      for (const char *s = " (rang "; *s;) *q++ = *s++;
      numfmt(next, q);
      while (*q) q++;
      for (const char *s = " : +1)"; *s;) *q++ = *s++;
    }
    *q = 0;
    ui_message(message, now);
  } else {
    ui_message("Enchant équipé !", now);
  }
  return SCREEN_REDRAW_CONTENT;
}

/* Effacer: removes one equipped copy of the selected enchant. */
static int unequip(uint32_t now) {
  int i = item_index();
  for (int s = game_enchant_slots() - 1; s >= 0; s--) {
    if (game.enchant_slot[s] == i) {
      game_unequip_enchant(s);
      ui_message("Enchant retiré", now);
      return SCREEN_REDRAW_CONTENT;
    }
  }
  return 0;
}

static int update(const input_t *in, uint32_t now) {
  int r = (now / 1000 != (now - 33) / 1000) ? SCREEN_REDRAW_CONTENT : 0; /* potion timers */
  if (on_header) {
    if (input_pressed(in, KEY_LEFT) || input_pressed(in, KEY_RIGHT)) {
      enchant_tab = !enchant_tab;
      sel_type = 0;
      r |= SCREEN_REDRAW_CONTENT;
    }
    if (input_pressed(in, KEY_DOWN) || input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
      on_header = false;
      r |= SCREEN_REDRAW_CONTENT;
    }
    return r;
  }
  if (input_pressed(in, KEY_UP)) {
    if (sel_type > 0) sel_type--;
    else on_header = true;
    r |= SCREEN_REDRAW_CONTENT;
  }
  if (input_pressed(in, KEY_DOWN)) {
    if (sel_type < type_count() - 1) sel_type++, r |= SCREEN_REDRAW_CONTENT;
    else return SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_LEFT) && sel_tier > 0) sel_tier--, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT) && sel_tier < 2) sel_tier++, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_TOOLBOX)) enchant_tab = !enchant_tab, sel_type = 0, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) r |= press(now);
  if (enchant_tab && input_pressed(in, KEY_BACKSPACE)) r |= unequip(now);
  return r;
}

/* --- Drawing --- */

static char *put(char *q, const char *s) {
  while (*s) *q++ = *s++;
  return q;
}

static char *put_2(char *q, unsigned v) { /* two digits */
  *q++ = (char)('0' + v / 10 % 10);
  *q++ = (char)('0' + v % 10);
  return q;
}

static void effect_text(char *buf) {
  static const char *const potion_effects[POTION_TYPE_COUNT] = {" % de coins", " % de dégâts", " % de diamants",
                                                                " % de chance", " % d'objets trouvés"};
  static const char *const enchant_effects[ENCHANT_TYPE_COUNT] = {" % de coins", " % de diamants", " % de chance",
                                                                  " % de force", " % au coup (OK)",
                                                                  " % d'objets trouvés"};
  int i = item_index();
  int pct = enchant_tab ? enchants[i].percent : potions[i].percent;
  char *q = put(buf, "+");
  numfmt(pct, q);
  while (*q) q++;
  put(q, enchant_tab ? enchant_effects[sel_type] : potion_effects[sel_type])[0] = 0;
}

static void draw_header(void) {
  static const char *const names[2] = {"Potions", "Enchants"};
  int x = 8;
  for (int t = 0; t < 2; t++) {
    bool cur = t == (int)enchant_tab;
    int w = gfx_text_width(&font_medium, names[t]) + 20;
    if (cur && on_header) gfx_round_rect(x - 2, CONTENT_Y + 2, w + 4, 24, 12, COL_GOLD);
    gfx_round_gradient(x, CONTENT_Y + 4, w, 20, 10, cur ? RGB(90, 160, 255) : COL_BAR_LIGHT, cur ? COL_BLUE_DARK : COL_BAR);
    gfx_text(&font_medium, x + 10, CONTENT_Y + 6, names[t], cur ? COL_WHITE : RGB(170, 180, 215));
    x += w + 6;
  }
  /* active potions with their time left */
  int ax = x + 6;
  for (int t = 0; t < POTION_TYPE_COUNT && ax < 300; t++) {
    if (!game.boost_tier[t]) continue;
    gfx_sprite_half(potions[t * 3 + game.boost_tier[t] - 1].sprite, ax, CONTENT_Y + 6);
    ax += 18;
  }
}

static void draw_grid(void) {
  for (int t = 0; t < type_count(); t++) {
    for (int k = 0; k < 3; k++) {
      int i = t * 3 + k;
      int x = GRID_X + k * CELL_W, y = GRID_Y + t * CELL_H;
      bool sel = !on_header && t == sel_type && k == sel_tier;
      int n = enchant_tab ? game.enchants_owned[i] : game.potions_owned[i];
      bool equipped = enchant_tab && game_enchant_equipped(i);
      ui_card(x, y, CELL_W - 4, CELL_H - 3, sel, n ? COL_PANEL_TOP : RGB(205, 210, 222),
              n ? (equipped ? RGB(170, 235, 170) : COL_PANEL_BOT) : RGB(185, 190, 205));
      if (sel) y -= 2;
      const sprite_t *s = enchant_tab ? enchants[i].sprite : potions[i].sprite;
      if (n) gfx_sprite_half(s, x + 3, y + 1);
      else gfx_sprite_half_tint(s, x + 3, y + 1, RGB(150, 155, 172));
      static const char *const roman[3] = {"I", "II", "III"};
      gfx_text(&font_small, x + 21, y + 1, roman[k], COL_TEXT_DIM);
      char buf[12] = "x";
      numfmt(n, buf + 1);
      gfx_text_aligned(&font_small, x, y + 12, CELL_W - 10, ALIGN_RIGHT, buf, n ? COL_TEXT : COL_TEXT_DIM);
    }
  }
}

static void draw_panel(void) {
  int x = PANEL_X, y = GRID_Y, w = PANEL_W, h = CONTENT_Y + CONTENT_H - GRID_Y - 2;
  ui_panel(x, y, w, h);
  int i = item_index();
  gfx_round_rect(x + (w - 40) / 2, y + 6, 40, 40, 10, RGB(225, 232, 248));
  gfx_sprite(enchant_tab ? enchants[i].sprite : potions[i].sprite, x + (w - 32) / 2, y + 10);
  char buf[48];
  ui_item_name(enchant_tab ? ITEM_ENCHANT + i : i, buf);
  gfx_text_aligned(&font_small, x, y + 50, w, ALIGN_CENTER, buf, COL_TEXT);
  effect_text(buf);
  gfx_text_aligned(&font_small, x, y + 64, w, ALIGN_CENTER, buf, COL_GREEN_DARK);
  char *q = buf;
  if (enchant_tab) {
    q = put(q, "Permanent (équipé)");
  } else {
    numfmt(potions[i].minutes, q);
    while (*q) q++;
    q = put(q, " min de jeu");
  }
  *q = 0;
  gfx_text_aligned(&font_small, x, y + 77, w, ALIGN_CENTER, buf, COL_TEXT_DIM);
  if (enchant_tab) { /* "Équipés : 2/5" */
    q = put(buf, "Équipés : ");
    numfmt(game_enchant_equipped(i), q);
    while (*q) q++;
    *q++ = '/';
    numfmt(game.enchants_owned[i], q);
  } else {
    q = put(buf, "Possédés : ");
    numfmt(game.potions_owned[i], q);
  }
  gfx_text_aligned(&font_small, x, y + 92, w, ALIGN_CENTER, buf, COL_TEXT);
  gfx_fill_rect(x + 8, y + 108, w - 16, 1, RGB(200, 210, 230));
  if (enchant_tab) {
    /* enchant slots */
    q = put(buf, "Emplacements ");
    *q++ = (char)('0' + game_enchant_slots());
    *q = 0;
    gfx_text_aligned(&font_small, x, y + 111, w, ALIGN_CENTER, buf, COL_TEXT_DIM);
    int sx = x + (w - MAX_ENCHANT_SLOTS * 22) / 2;
    for (int s = 0; s < MAX_ENCHANT_SLOTS; s++) {
      bool open = s < game_enchant_slots();
      gfx_round_rect(sx + s * 22, y + 125, 20, 20, 5, open ? RGB(205, 215, 235) : RGB(160, 165, 180));
      if (open && game.enchant_slot[s] != NO_ITEM) {
        gfx_sprite_half(enchants[game.enchant_slot[s]].sprite, sx + s * 22 + 2, y + 127);
      }
    }
    gfx_text_aligned(&font_small, x, y + h - 18, w, ALIGN_CENTER,
                     "OK : +1  ·  Effacer : -1", COL_BLUE_DARK);
  } else {
    int t = sel_type;
    if (game.boost_tier[t]) {
      uint32_t secs = game.boost_ms[t] / 1000;
      static const char *const roman[4] = {"", "I", "II", "III"};
      q = put(put(put(buf, "Actif : "), roman[game.boost_tier[t]]), "  ");
      q = put_2(q, secs / 60 > 99 ? 99 : secs / 60);
      *q++ = ':';
      q = put_2(q, secs % 60);
      *q = 0;
      gfx_text_aligned(&font_small, x, y + 113, w, ALIGN_CENTER, buf, COL_GREEN_DARK);
    } else {
      gfx_text_aligned(&font_small, x, y + 113, w, ALIGN_CENTER, "Pas actif", COL_TEXT_DIM);
    }
    gfx_text_aligned(&font_small, x, y + 126, w, ALIGN_CENTER, "Durées cumulables,", COL_TEXT_DIM);
    gfx_text_aligned(&font_small, x, y + 137, w, ALIGN_CENTER, "meilleur niveau gardé", COL_TEXT_DIM);
    gfx_text_aligned(&font_small, x, y + h - 15, w, ALIGN_CENTER, "OK : boire", COL_BLUE_DARK);
  }
}

static void draw(void) {
  draw_header();
  draw_grid();
  draw_panel();
}

const screen_t screen_items = {"Objets", &spr_tab_items, enter, update, draw};
