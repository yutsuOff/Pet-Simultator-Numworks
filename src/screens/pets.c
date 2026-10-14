/* PETS screen: owned pets (stacked, strongest first) and the team.
 * OK equips one copy, Backspace removes one, Toolbox equips the best pets. */
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
#define GRID_Y (CONTENT_Y + 22)

static uint8_t list[PET_COUNT];
static int count, selected, scroll;

static void build_list(void) {
  int keep = count ? list[selected] : -1;
  count = 0;
  for (int p = 0; p < PET_COUNT; p++) {
    if (game.owned[p]) list[count++] = (uint8_t)p;
  }
  /* strongest first (insertion sort, small list) */
  for (int i = 1; i < count; i++) {
    uint8_t v = list[i];
    int j = i - 1;
    while (j >= 0 && pets[list[j]].power < pets[v].power) list[j + 1] = list[j], j--;
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
}

static int update(const input_t *in, uint32_t now) {
  int r = 0;
  if (input_pressed(in, KEY_DOWN)) {
    if (selected + COLS < count) selected += COLS, r |= SCREEN_REDRAW_CONTENT;
    else if (selected / COLS < (count - 1) / COLS) selected = count - 1, r |= SCREEN_REDRAW_CONTENT;
    else return SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_UP) && selected >= COLS) selected -= COLS, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_LEFT) && selected > 0) selected--, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT) && selected < count - 1) selected++, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_TOOLBOX) && count) {
    game_equip_best();
    ui_message("Meilleurs pets équipés", now);
    r |= SCREEN_REDRAW_CONTENT;
  }
  if (count && (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE))) {
    int p = list[selected];
    if (game_equip(p)) r |= SCREEN_REDRAW_CONTENT;
    else if (game_team_size() >= TEAM_SIZE) ui_message("Équipe complète (Effacer : retirer)", now), r |= SCREEN_REDRAW_CONTENT;
    else ui_message("Tous les exemplaires sont équipés", now), r |= SCREEN_REDRAW_CONTENT;
  }
  if (count && input_pressed(in, KEY_BACKSPACE) && game_unequip(list[selected])) r |= SCREEN_REDRAW_CONTENT;
  int row = selected / COLS;
  if (row < scroll) scroll = row;
  if (row >= scroll + ROWS) scroll = row - ROWS + 1;
  return r;
}

static void draw_cell(int i, int x, int y) {
  const pet_t *p = &pets[list[i]];
  bool sel = i == selected;
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
  gfx_round_gradient(x + 6, y + 6, w - 12, 52, 8, gfx_mix(COL_WHITE, tc, 60), gfx_mix(COL_WHITE, tc, 140));
  gfx_sprite(p->sprite, x + (w - 48) / 2, y + 8);
  const font_t *f = gfx_text_width(&font_medium, p->name) <= w - 12 ? &font_medium : &font_small;
  gfx_text_aligned(f, x, y + 61, w, ALIGN_CENTER, p->name, COL_TEXT);
  int pw = gfx_text_width(&font_small, ui_tier_name(p->tier)) + 10;
  ui_pill(x + (w - pw) / 2, y + 78, ui_tier_name(p->tier), tc, COL_WHITE);
  static const char *const labels[] = {"Puissance", "Possédés", "Équipés"};
  double values[] = {p->power, game.owned[list[selected]], game_equipped_count(list[selected])};
  char buf[24];
  int ly = y + 94;
  for (int i = 0; i < 3; i++, ly += 12) {
    gfx_text(&font_small, x + 8, ly, labels[i], COL_TEXT_DIM);
    gfx_text_aligned(&font_small, x, ly, w - 8, ALIGN_RIGHT, numfmt(values[i], buf), COL_TEXT);
  }
  gfx_text_aligned(&font_small, x, ly, w, ALIGN_CENTER, eggs[p->egg].name, COL_BLUE_DARK);
  gfx_fill_rect(x + 8, ly + 15, w - 16, 1, RGB(200, 210, 230));
  static const char *const hints[] = {"OK : équiper", "Effacer : retirer", "Toolbox : meilleurs"};
  for (int i = 0; i < 3; i++) gfx_text_aligned(&font_small, x, ly + 17 + i * 12, w, ALIGN_CENTER, hints[i], COL_TEXT_DIM);
}

static void draw(void) {
  char buf[48], n[12];
  numfmt(count, n);
  char *q = buf;
  for (const char *s = n; *s;) *q++ = *s++;
  for (const char *s = count > 1 ? " sortes de pets" : " sorte de pet"; *s;) *q++ = *s++;
  *q = 0;
  int hx = 10 + gfx_text_shadow(&font_medium, 10, CONTENT_Y + 4, buf, COL_WHITE, COL_SHADOW) + 8;
  int team = game_team_size();
  char t[16] = "Équipe 0/0";
  t[sizeof("Équipe ") - 1] = (char)('0' + team);
  t[sizeof("Équipe 0/") - 1] = (char)('0' + TEAM_SIZE);
  ui_pill(hx, CONTENT_Y + 5, t, team ? COL_GREEN_DARK : COL_LOCKED, COL_WHITE);
  if (!count) {
    ui_panel(10, CONTENT_Y + 30, 300, 120);
    gfx_text_aligned(&font_big, 10, CONTENT_Y + 50, 300, ALIGN_CENTER, "Aucun pet", COL_TEXT);
    gfx_text_aligned(&font_medium, 10, CONTENT_Y + 84, 300, ALIGN_CENTER, "Ouvre un œuf dans l'onglet", COL_TEXT_DIM);
    gfx_text_aligned(&font_medium, 10, CONTENT_Y + 102, 300, ALIGN_CENTER, "Œufs (touche 3) !", COL_TEXT_DIM);
    return;
  }
  for (int i = scroll * COLS; i < count && i < (scroll + ROWS) * COLS; i++) {
    int c = i % COLS, rr = i / COLS - scroll;
    draw_cell(i, GRID_X + c * CELL_W, GRID_Y + rr * CELL_H);
  }
  /* scroll indicator */
  int rows = (count + COLS - 1) / COLS;
  if (rows > ROWS) {
    int bar_h = ROWS * CELL_H - 6;
    gfx_round_rect(GRID_X + COLS * CELL_W, GRID_Y, 3, bar_h, 1, gfx_mix(COL_OUTLINE, COL_WHITE, 120));
    int th = bar_h * ROWS / rows;
    gfx_round_rect(GRID_X + COLS * CELL_W, GRID_Y + (bar_h - th) * scroll / (rows - ROWS), 3, th, 1, COL_WHITE);
  }
  draw_details(192, CONTENT_Y + 4, 122, CONTENT_H - 8);
}

const screen_t screen_pets = {"Pets", &spr_tab_pets, enter, update, draw};
