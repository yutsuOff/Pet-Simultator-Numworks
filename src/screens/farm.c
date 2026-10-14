/* FARM screen. M0: interactive layout demo with placeholder breakables
 * (OK hits the selected object). Real farming with pets arrives in M1. */
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define COLS 4
#define ROWS 2
#define SLOTS (COLS * ROWS)
#define CARD_W 70
#define CARD_H 64
#define RESPAWN_MS 3000
#define POP_MS 700

typedef struct {
  const char *name;
  const sprite_t *sprite;
  int hp;
  double reward;
  bool gives_diamonds;
} breakable_type_t;

static const breakable_type_t types[] = {
  {"Coins", &spr_brk_coins, 4, 5, false},
  {"Coffre", &spr_brk_chest, 12, 30, false},
  {"Cadeau", &spr_brk_present, 8, 15, false},
  {"Diamants", &spr_brk_gems, 10, 1, true},
};

typedef struct {
  uint8_t type;
  int hp;
  uint32_t respawn_at; /* 0 when alive */
  uint32_t pop_until;  /* reward text animation after destruction */
} slot_t;

static slot_t slots[SLOTS];
static int selected;
static uint32_t clock_ms;
static bool initialized;

static void enter(void) {
  if (initialized) return;
  static const uint8_t layout[SLOTS] = {0, 1, 0, 2, 0, 3, 0, 0};
  for (int i = 0; i < SLOTS; i++) {
    slots[i].type = layout[i];
    slots[i].hp = types[layout[i]].hp;
  }
  initialized = true;
}

static int update(const input_t *in, uint32_t now) {
  int result = 0;
  clock_ms = now;
  int col = selected % COLS, row = selected / COLS;
  if (input_pressed(in, KEY_LEFT) && col > 0) selected--, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT) && col < COLS - 1) selected++, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_UP) && row > 0) selected -= COLS, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_DOWN)) {
    if (row < ROWS - 1) selected += COLS, result |= SCREEN_REDRAW_CONTENT;
    else result |= SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
    slot_t *s = &slots[selected];
    if (s->respawn_at == 0) {
      s->hp--;
      result |= SCREEN_REDRAW_CONTENT;
      if (s->hp <= 0) {
        const breakable_type_t *t = &types[s->type];
        if (t->gives_diamonds) game.diamonds += t->reward;
        else game.coins += t->reward;
        s->respawn_at = now + RESPAWN_MS;
        s->pop_until = now + POP_MS;
        result |= SCREEN_REDRAW_ALL;
      }
    }
  }
  for (int i = 0; i < SLOTS; i++) {
    slot_t *s = &slots[i];
    if (s->respawn_at && (int32_t)(now - s->respawn_at) >= 0) {
      s->respawn_at = 0;
      s->hp = types[s->type].hp;
      result |= SCREEN_REDRAW_CONTENT;
    }
    /* animate the reward pop and the countdown (redraw a few times per second) */
    if (s->pop_until && (int32_t)(now - s->pop_until) < 0) result |= SCREEN_REDRAW_CONTENT;
    if (s->respawn_at && ((now / 100) != ((now - 33) / 100))) result |= SCREEN_REDRAW_CONTENT;
  }
  return result;
}

static void draw_slot(int i) {
  const slot_t *s = &slots[i];
  const breakable_type_t *t = &types[s->type];
  int x = 8 + (i % COLS) * (CARD_W + 8);
  int y = CONTENT_Y + 10 + (i / COLS) * (CARD_H + 8);
  bool sel = i == selected;
  bool alive = s->respawn_at == 0;
  if (alive) ui_card(x, y, CARD_W, CARD_H, sel, COL_PANEL_TOP, COL_PANEL_BOT);
  else ui_card(x, y, CARD_W, CARD_H, sel, RGB(200, 205, 215), RGB(170, 175, 190));
  if (sel) y -= 2;
  int sx = x + (CARD_W - t->sprite->width) / 2;
  if (alive) {
    gfx_sprite(t->sprite, sx, y + 6);
  } else {
    gfx_sprite_tint(t->sprite, sx, y + 6, RGB(140, 145, 160));
    char buf[8];
    uint32_t left = s->respawn_at - clock_ms;
    buf[0] = (char)('0' + (left + 999) / 1000);
    buf[1] = 's';
    buf[2] = 0;
    gfx_text_aligned(&font_medium, x, y + 13, CARD_W, ALIGN_CENTER, buf, COL_WHITE);
  }
  /* reward line */
  char amount[24];
  numfmt(t->reward, amount);
  const sprite_t *icon = t->gives_diamonds ? &spr_gem : &spr_coin;
  int tw = gfx_text_width(&font_small, amount) + icon->width + 3;
  int rx = x + (CARD_W - tw) / 2;
  gfx_sprite(icon, rx, y + 35);
  gfx_text(&font_small, rx + icon->width + 3, y + 35, amount, COL_TEXT);
  /* HP bar */
  int permille = alive ? s->hp * 1000 / t->hp : 0;
  gfx_progress_bar(x + 7, y + CARD_H - 14, CARD_W - 14, 7, permille, COL_GREEN, COL_HP_BACK);
  /* reward pop animation: "+5" rising above the card */
  if (s->pop_until && (int32_t)(clock_ms - s->pop_until) < 0) {
    int age = POP_MS - (int)(s->pop_until - clock_ms);
    char pop[24] = "+";
    numfmt(t->reward, pop + 1);
    gfx_text_shadow(&font_big, x + 12, y + 6 - age * 18 / POP_MS, pop,
                    t->gives_diamonds ? COL_GEM : COL_GOLD, COL_SHADOW);
  }
}

static void draw(void) {
  for (int i = 0; i < SLOTS; i++) draw_slot(i);
  /* equipped pets bar */
  int y = CONTENT_Y + 156;
  ui_panel(6, y, 308, 36);
  gfx_text(&font_small, 14, y + 4, "Pets équipés", COL_TEXT_DIM);
  for (int i = 0; i < 5; i++) {
    int px = 14 + i * 26;
    gfx_round_rect(px, y + 17, 22, 14, 4, RGB(205, 215, 235));
  }
  gfx_text(&font_small, 156, y + 4, "Démo M0 : OK = frapper", COL_TEXT);
  gfx_text(&font_small, 156, y + 18, "Les pets arrivent en M1", COL_TEXT_DIM);
}

const screen_t screen_farm = {"Farm", &spr_tab_farm, enter, update, draw};
