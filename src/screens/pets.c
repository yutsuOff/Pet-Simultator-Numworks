/* PETS screen: owned pets (stacked) and the team.
 * Toolbar (↑ from the first row): sort, filter, equip the best, recycle duplicates.
 * In the grid: OK equips one copy, Backspace removes one, Toolbox equips the best. */
#include <string.h>
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define COLS 5
#define ROWS 4
#define CELL_W 36
#define CELL_H 42
#define GRID_X 6
#define GRID_Y (CONTENT_Y + 24)
#define PANEL_X 192
#define PANEL_W 122

enum { SORT_POWER, SORT_TIER, SORT_NAME, SORT_COUNT_OWNED, SORT_COUNT };
enum { FILTER_ALL, FILTER_EQUIPPED, FILTER_DUPLICATES, FILTER_COUNT };
enum { BTN_SORT, BTN_FILTER, BTN_BEST, BTN_RECYCLE, BTN_COUNT };

static const char *const sort_names[SORT_COUNT] = {"Tri Puissance", "Tri Rareté", "Tri Nom", "Tri Nombre"};
static const char *const filter_names[FILTER_COUNT] = {"Tous", "Équipés", "Doublons"};

static uint8_t list[PET_COUNT];
static int count, selected, scroll;
static int sort_mode, filter_mode;
static bool on_bar;   /* the toolbar has the focus */
static int bar_sel;
static bool confirm;  /* "recycle the duplicates?" */
static char message[64];

static bool before(int a, int b) { /* true if pet a is listed before pet b */
  switch (sort_mode) {
    case SORT_TIER:
      if (pets[a].tier != pets[b].tier) return pets[a].tier > pets[b].tier;
      break;
    case SORT_NAME: return strcmp(pets[a].name, pets[b].name) < 0;
    case SORT_COUNT_OWNED:
      if (game.owned[a] != game.owned[b]) return game.owned[a] > game.owned[b];
      break;
  }
  return pets[a].power > pets[b].power;
}

static bool shown(int p) {
  if (!game.owned[p]) return false;
  if (filter_mode == FILTER_EQUIPPED) return game_equipped_count(p) > 0;
  if (filter_mode == FILTER_DUPLICATES) return game_duplicates(p) > 0;
  return true;
}

static void build_list(void) {
  int keep = count ? list[selected] : -1;
  count = 0;
  for (int p = 0; p < PET_COUNT; p++) {
    if (shown(p)) list[count++] = (uint8_t)p;
  }
  for (int i = 1; i < count; i++) { /* insertion sort, small list */
    uint8_t v = list[i];
    int j = i - 1;
    while (j >= 0 && before(v, list[j])) list[j + 1] = list[j], j--;
    list[j + 1] = v;
  }
  selected = 0;
  for (int i = 0; i < count; i++) {
    if (list[i] == keep) selected = i;
  }
}

static void enter(void) {
  count = 0;
  build_list();
  scroll = 0;
  on_bar = false;
  confirm = false;
}

static void say(const char *a, double n, const char *b, uint32_t now) {
  char num[16], *q = message;
  numfmt(n, num);
  for (const char *s = a; *s;) *q++ = *s++;
  for (const char *s = num; *s;) *q++ = *s++;
  for (const char *s = b; *s;) *q++ = *s++;
  *q = 0;
  ui_message(message, now);
}

static int duplicates_total(double *coins) {
  int n = 0;
  *coins = 0;
  for (int p = 0; p < PET_COUNT; p++) {
    n += game_duplicates(p);
    *coins += game_duplicates(p) * game_recycle_value(p);
  }
  return n;
}

static int press_button(int b, uint32_t now) {
  double coins;
  switch (b) {
    case BTN_SORT: sort_mode = (sort_mode + 1) % SORT_COUNT; break;
    case BTN_FILTER: filter_mode = (filter_mode + 1) % FILTER_COUNT; break;
    case BTN_BEST:
      game_equip_best();
      ui_message("Meilleurs pets équipés", now);
      break;
    case BTN_RECYCLE:
      if (duplicates_total(&coins)) confirm = true;
      else ui_message("Aucun doublon à recycler", now);
      break;
  }
  build_list();
  return SCREEN_REDRAW_CONTENT;
}

static int update(const input_t *in, uint32_t now) {
  int r = 0;
  if (confirm) {
    if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
      int n;
      double coins = game_recycle_all(&n);
      say("Doublons recyclés : +", coins, " coins", now);
      confirm = false;
      build_list();
      return SCREEN_REDRAW_ALL;
    }
    if (input_pressed(in, KEY_BACK)) {
      confirm = false;
      return SCREEN_REDRAW_CONTENT | SCREEN_USED_BACK;
    }
    return 0;
  }
  if (on_bar) {
    if (input_pressed(in, KEY_LEFT) && bar_sel > 0) bar_sel--, r |= SCREEN_REDRAW_CONTENT;
    if (input_pressed(in, KEY_RIGHT) && bar_sel < BTN_COUNT - 1) bar_sel++, r |= SCREEN_REDRAW_CONTENT;
    if (input_pressed(in, KEY_DOWN)) {
      if (!count) return SCREEN_FOCUS_TABS;
      on_bar = false;
      r |= SCREEN_REDRAW_CONTENT;
    }
    if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) r |= press_button(bar_sel, now);
    return r;
  }
  if (input_pressed(in, KEY_DOWN)) {
    if (selected + COLS < count) selected += COLS, r |= SCREEN_REDRAW_CONTENT;
    else if (selected / COLS < (count - 1) / COLS) selected = count - 1, r |= SCREEN_REDRAW_CONTENT;
    else return SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_UP)) {
    if (selected >= COLS) selected -= COLS;
    else on_bar = true;
    r |= SCREEN_REDRAW_CONTENT;
  }
  if (input_pressed(in, KEY_LEFT) && selected > 0) selected--, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT) && selected < count - 1) selected++, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_TOOLBOX)) r |= press_button(BTN_BEST, now);
  if (count && (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE))) {
    int p = list[selected];
    if (game_equip(p)) r |= SCREEN_REDRAW_CONTENT;
    else if (game_team_size() >= game_team_slots()) ui_message("Équipe complète (Effacer : retirer)", now), r |= SCREEN_REDRAW_CONTENT;
    else ui_message("Tous les exemplaires sont équipés", now), r |= SCREEN_REDRAW_CONTENT;
    if (filter_mode != FILTER_ALL) build_list();
  }
  if (count && input_pressed(in, KEY_BACKSPACE) && game_unequip(list[selected])) {
    r |= SCREEN_REDRAW_CONTENT;
    if (filter_mode != FILTER_ALL) build_list();
  }
  int row = selected / COLS;
  if (row < scroll) scroll = row;
  if (row >= scroll + ROWS) scroll = row - ROWS + 1;
  return r;
}

/* --- Drawing --- */

static void draw_toolbar(void) {
  const char *labels[BTN_COUNT] = {sort_names[sort_mode], filter_names[filter_mode], "Meilleurs", "Recycler"};
  /* buttons as wide as their label, narrower padding if the row would not fit */
  int total = 0;
  for (int b = 0; b < BTN_COUNT; b++) total += gfx_text_width(&font_small, labels[b]) + 14 + 5;
  int pad = total > 312 ? 8 : 14;
  int x = 6;
  for (int b = 0; b < BTN_COUNT; b++) {
    bool sel = on_bar && b == bar_sel;
    int w = gfx_text_width(&font_small, labels[b]) + pad;
    if (sel) gfx_round_rect(x - 2, CONTENT_Y + 2, w + 4, 21, 10, COL_GOLD);
    gfx_round_gradient(x, CONTENT_Y + 4, w, 17, 8, sel ? RGB(90, 160, 255) : COL_BAR_LIGHT, sel ? COL_BLUE_DARK : COL_BAR);
    gfx_text(&font_small, x + pad / 2, CONTENT_Y + 6, labels[b], COL_WHITE);
    x += w + 5;
  }
}

static void draw_cell(int i, int x, int y) {
  const pet_t *p = &pets[list[i]];
  bool sel = i == selected && !on_bar;
  color_t tc = ui_tier_color(p->tier);
  ui_card(x, y, CELL_W - 4, CELL_H - 4, sel, gfx_mix(COL_WHITE, tc, 50), gfx_mix(COL_WHITE, tc, 120));
  if (sel) y -= 2;
  gfx_sprite_half(p->sprite, x + 4, y + 3);
  int n = game.owned[list[i]];
  if (n > 1) {
    char buf[12] = "x";
    numfmt(n, buf + 1);
    gfx_text_aligned(&font_small, x, y + CELL_H - 19, CELL_W - 7, ALIGN_RIGHT, buf, COL_TEXT);
  }
  if (game_equipped_count(list[i])) {
    gfx_round_rect(x + 2, y + CELL_H - 17, 11, 11, 5, COL_GREEN_DARK);
    gfx_round_rect(x + 3, y + CELL_H - 16, 9, 9, 4, COL_GREEN);
  }
}

static void draw_details(int x, int y, int w, int h) {
  ui_panel(x, y, w, h);
  const pet_t *p = &pets[list[selected]];
  color_t tc = ui_tier_color(p->tier);
  gfx_round_gradient(x + 6, y + 6, w - 12, 50, 8, gfx_mix(COL_WHITE, tc, 60), gfx_mix(COL_WHITE, tc, 140));
  gfx_sprite(p->sprite, x + (w - 48) / 2, y + 7);
  const font_t *f = gfx_text_width(&font_medium, p->name) <= w - 12 ? &font_medium : &font_small;
  gfx_text_aligned(f, x, y + 58, w, ALIGN_CENTER, p->name, COL_TEXT);
  int pw = gfx_text_width(&font_small, ui_tier_name(p->tier)) + 10;
  ui_pill(x + (w - pw) / 2, y + 74, ui_tier_name(p->tier), tc, COL_WHITE);
  static const char *const labels[] = {"Puissance", "Possédés", "Équipés"};
  double values[] = {game_pet_power(list[selected]), game.owned[list[selected]], game_equipped_count(list[selected])};
  char buf[24];
  int ly = y + 90;
  for (int i = 0; i < 3; i++, ly += 12) {
    gfx_text(&font_small, x + 8, ly, labels[i], COL_TEXT_DIM);
    gfx_text_aligned(&font_small, x, ly, w - 8, ALIGN_RIGHT, numfmt(values[i], buf), COL_TEXT);
  }
  gfx_text_aligned(&font_small, x, ly, w, ALIGN_CENTER, eggs[p->egg].name, COL_BLUE_DARK);
  gfx_fill_rect(x + 8, ly + 15, w - 16, 1, RGB(200, 210, 230));
  gfx_text_aligned(&font_small, x, ly + 17, w, ALIGN_CENTER, "OK : équiper", COL_TEXT_DIM);
  gfx_text_aligned(&font_small, x, ly + 29, w, ALIGN_CENTER, "Effacer : retirer", COL_TEXT_DIM);
}

static void draw_confirm(void) {
  double coins;
  int n = duplicates_total(&coins);
  ui_panel(30, CONTENT_Y + 40, 260, 110);
  gfx_text_aligned(&font_big, 30, CONTENT_Y + 50, 260, ALIGN_CENTER, "Recycler ?", COL_TEXT);
  char num[16], line[48], *q = line;
  for (const char *s = numfmt(n, num); *s;) *q++ = *s++;
  for (const char *s = n > 1 ? " doublons contre" : " doublon contre"; *s;) *q++ = *s++;
  *q = 0;
  gfx_text_aligned(&font_medium, 30, CONTENT_Y + 78, 260, ALIGN_CENTER, line, COL_TEXT);
  numfmt(coins, num);
  int cw = gfx_text_width(&font_medium, num) + spr_coin.width + 3;
  ui_coin_amount(160 - cw / 2, CONTENT_Y + 96, &font_medium, coins, COL_GOLD_DARK);
  gfx_text_aligned(&font_small, 30, CONTENT_Y + 124, 260, ALIGN_CENTER,
                   "On garde 1 exemplaire et les équipés", COL_TEXT_DIM);
  gfx_text_aligned(&font_small, 30, CONTENT_Y + 136, 260, ALIGN_CENTER, "OK : recycler  ·  Retour : annuler",
                   COL_BLUE_DARK);
}

static void draw(void) {
  draw_toolbar();
  if (!count) {
    ui_panel(10, CONTENT_Y + 34, 300, 110);
    bool none = filter_mode == FILTER_ALL;
    gfx_text_aligned(&font_big, 10, CONTENT_Y + 50, 300, ALIGN_CENTER, none ? "Aucun pet" : "Aucun pet ici", COL_TEXT);
    gfx_text_aligned(&font_medium, 10, CONTENT_Y + 84, 300, ALIGN_CENTER,
                     none ? "Ouvre un œuf dans l'onglet" : "Change le filtre (↑ puis OK)", COL_TEXT_DIM);
    if (none) gfx_text_aligned(&font_medium, 10, CONTENT_Y + 102, 300, ALIGN_CENTER, "Œufs (touche 3) !", COL_TEXT_DIM);
  } else {
    for (int i = scroll * COLS; i < count && i < (scroll + ROWS) * COLS; i++) {
      int c = i % COLS, rr = i / COLS - scroll;
      draw_cell(i, GRID_X + c * CELL_W, GRID_Y + rr * CELL_H);
    }
    int rows = (count + COLS - 1) / COLS;
    if (rows > ROWS) { /* scroll indicator */
      int bar_h = ROWS * CELL_H - 6;
      gfx_round_rect(GRID_X + COLS * CELL_W, GRID_Y, 3, bar_h, 1, gfx_mix(COL_OUTLINE, COL_WHITE, 120));
      int th = bar_h * ROWS / rows;
      gfx_round_rect(GRID_X + COLS * CELL_W, GRID_Y + (bar_h - th) * scroll / (rows - ROWS), 3, th, 1, COL_WHITE);
    }
    draw_details(PANEL_X, GRID_Y, PANEL_W, CONTENT_Y + CONTENT_H - GRID_Y - 2);
  }
  if (confirm) draw_confirm();
}

const screen_t screen_pets = {"Pets", &spr_tab_pets, enter, update, draw};
