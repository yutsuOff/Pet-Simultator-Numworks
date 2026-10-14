/* INDEX screen: every pet of the game, in egg order. Hatched pets are in color,
 * the others are black silhouettes. */
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define COLS 10
#define ROWS 5
#define CELL 28
#define GRID_X 20
#define GRID_Y (CONTENT_Y + 24)

static int selected, scroll;

static void enter(void) {}

static int update(const input_t *in, uint32_t now) {
  int r = 0;
  if (input_pressed(in, KEY_LEFT) && selected > 0) selected--, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT) && selected < PET_COUNT - 1) selected++, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_UP) && selected >= COLS) selected -= COLS, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_DOWN)) {
    if (selected + COLS < PET_COUNT) selected += COLS, r |= SCREEN_REDRAW_CONTENT;
    else return SCREEN_FOCUS_TABS;
  }
  int row = selected / COLS;
  if (row < scroll) scroll = row;
  if (row >= scroll + ROWS) scroll = row - ROWS + 1;
  return r;
}

static void draw(void) {
  int found = 0;
  for (int p = 0; p < PET_COUNT; p++) found += game_discovered(p);
  char buf[32], n[12];
  numfmt(found, buf);
  int len = 0;
  while (buf[len]) len++;
  buf[len++] = '/';
  numfmt(PET_COUNT, n);
  for (int i = 0; n[i]; i++) buf[len++] = n[i];
  buf[len] = 0;
  gfx_text_shadow(&font_medium, 10, CONTENT_Y + 5, "Index", COL_WHITE, COL_SHADOW);
  ui_pill(58, CONTENT_Y + 6, buf, COL_BAR, COL_WHITE);
  gfx_progress_bar(130, CONTENT_Y + 8, 180, 8, found * 1000 / PET_COUNT, COL_GOLD, COL_HP_BACK);

  for (int i = scroll * COLS; i < PET_COUNT && i < (scroll + ROWS) * COLS; i++) {
    int x = GRID_X + (i % COLS) * CELL, y = GRID_Y + (i / COLS - scroll) * CELL;
    const pet_t *p = &pets[i];
    bool known = game_discovered(i);
    color_t bg = known ? gfx_mix(COL_WHITE, ui_tier_color(p->tier), 90) : RGB(150, 160, 185);
    if (i == selected) gfx_round_rect(x - 1, y - 1, CELL, CELL, 7, COL_GOLD);
    gfx_round_rect(x + 1, y + 1, CELL - 4, CELL - 4, 5, bg);
    if (known) gfx_sprite_half(p->sprite, x + 2, y + 2);
    else gfx_sprite_half_tint(p->sprite, x + 2, y + 2, RGB(40, 44, 70));
  }
  /* selected pet */
  int y = GRID_Y + ROWS * CELL + 1;
  ui_panel(6, y, 308, CONTENT_Y + CONTENT_H - y - 2);
  const pet_t *p = &pets[selected];
  bool known = game_discovered(selected);
  int x = 14 + gfx_text(&font_medium, 14, y + 6, known ? p->name : "???", COL_TEXT) + 8;
  if (known) ui_pill(x, y + 7, ui_tier_name(p->tier), ui_tier_color(p->tier), COL_WHITE);
  gfx_text_aligned(&font_small, 6, y + 8, 300, ALIGN_RIGHT, eggs[p->egg].name, COL_TEXT_DIM);
}

const screen_t screen_index = {"Index", &spr_tab_index, enter, update, draw};
