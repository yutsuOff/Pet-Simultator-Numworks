/* FARM screen: the breakables of the current zone. OK targets the selected object
 * (and gives it a hit); the equipped pets then attack the target on their own and
 * move to the next object when it breaks (AUTO). */
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
#define POP_MS 700
#define TAP_PILES 8 /* a hit without pets: 1/8 of a coin pile */

/* HP and rewards are multiples of the zone's coin pile (zone_t.pile_hp, pile_reward).
 * Coins < coin pile < crate < gift < chest;  diamond < diamond pile. */
typedef struct {
  const sprite_t *sprite;
  double hp;
  double reward;
  bool gives_diamonds; /* reward x (1 + zone / 4) diamonds */
  uint8_t weight;      /* spawn chance in %, the weights sum to 100 */
} breakable_type_t;

#define PILE 1 /* the coin pile, reference of the zone balancing */
static const breakable_type_t types[] = {
  {&spr_brk_coin, 0.5, 0.5, false, 30},
  {&spr_brk_coins, 1.0, 1.0, false, 28},
  {&spr_brk_crate, 2.5, 3.0, false, 14},
  {&spr_brk_gift, 4.0, 5.0, false, 8},
  {&spr_brk_chest, 8.0, 12.0, false, 4},
  {&spr_brk_gem, 1.5, 1.0, true, 12},
  {&spr_brk_gems, 4.0, 4.0, true, 4},
};
#define TYPE_COUNT (int)(sizeof(types) / sizeof(types[0]))

typedef struct {
  uint8_t type;
  double hp;
  uint32_t respawn_at; /* 0 when alive */
  uint32_t pop_until;  /* reward text animation after destruction */
  double pop_value;
} slot_t;

static slot_t slots[SLOTS];
static int selected;
static int target = -1;
static uint32_t clock_ms, last_tick;
static int farm_zone = -1;

static double max_hp(const slot_t *s) { return types[s->type].hp * zones[game.zone].pile_hp; }

static double reward(const slot_t *s) {
  const breakable_type_t *t = &types[s->type];
  if (t->gives_diamonds) return t->reward * (1 + game.zone / 4) * game_diamond_bonus();
  return t->reward * zones[game.zone].pile_reward * game_coin_bonus();
}

static void spawn(slot_t *s) {
  int r = (int)(platform_random() % 100), type = 0;
  while (type < TYPE_COUNT - 1 && r >= types[type].weight) r -= types[type++].weight;
  s->type = (uint8_t)type;
  s->hp = max_hp(s);
  s->respawn_at = 0;
}

static void reset_zone(void) {
  for (int i = 0; i < SLOTS; i++) {
    spawn(&slots[i]);
    slots[i].pop_until = 0;
  }
  /* always start with plain coin piles in the first row */
  for (int i = 0; i < COLS; i++) {
    slots[i].type = PILE;
    slots[i].hp = max_hp(&slots[i]);
  }
  farm_zone = game.zone;
  target = -1;
}

static void enter(void) {
  if (farm_zone != game.zone) reset_zone();
}

static int next_alive(int from) {
  for (int k = 1; k <= SLOTS; k++) {
    int i = (from + k) % SLOTS;
    if (slots[i].respawn_at == 0) return i;
  }
  return -1;
}

/* Applies damage to the target; returns the redraw bits. */
static int hit(double damage, uint32_t now) {
  slot_t *s = &slots[target];
  s->hp -= damage;
  if (s->hp > 0) return 0;
  double value = reward(s);
  if (types[s->type].gives_diamonds) game.diamonds += value;
  else game.coins += value;
  s->pop_value = value;
  s->pop_until = now + POP_MS;
  s->respawn_at = now + game_respawn_ms();
  target = next_alive(target);
  return SCREEN_REDRAW_ALL;
}

static int update(const input_t *in, uint32_t now) {
  int result = 0;
  if (farm_zone != game.zone) reset_zone();
  uint32_t dt = last_tick ? now - last_tick : 0;
  if (dt > 200) dt = 200; /* screen was hidden: no catch-up */
  last_tick = now;
  clock_ms = now;
  int col = selected % COLS, row = selected / COLS;
  if (input_pressed(in, KEY_LEFT) && col > 0) selected--, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT) && col < COLS - 1) selected++, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_UP) && row > 0) selected -= COLS, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_DOWN)) {
    if (row < ROWS - 1) selected += COLS, result |= SCREEN_REDRAW_CONTENT;
    else result |= SCREEN_FOCUS_TABS;
  }
  if ((input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) && slots[selected].respawn_at == 0) {
    target = selected;
    double tap = (zones[game.zone].pile_hp / TAP_PILES + game_team_power() / 5) * game_tap_bonus();
    result |= SCREEN_REDRAW_CONTENT | hit(tap, now);
  }
  /* pets attack the target continuously */
  double power = game_team_power();
  if (target >= 0 && power > 0 && dt) {
    result |= hit(power * dt / 1000.0, now);
    if ((now / 100) != ((now - dt) / 100)) result |= SCREEN_REDRAW_CONTENT;
  }
  if (target < 0 && power > 0) target = next_alive(SLOTS - 1); /* AUTO */
  for (int i = 0; i < SLOTS; i++) {
    slot_t *s = &slots[i];
    if (s->respawn_at && (int32_t)(now - s->respawn_at) >= 0) {
      spawn(s);
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
  if (i == target && !sel) gfx_round_rect(x - 2, y - 2, CARD_W + 4, CARD_H + 4, 9, COL_RED);
  if (alive) ui_card(x, y, CARD_W, CARD_H, sel, COL_PANEL_TOP, COL_PANEL_BOT);
  else ui_card(x, y, CARD_W, CARD_H, sel, RGB(200, 205, 215), RGB(170, 175, 190));
  if (sel) y -= 2;
  int sx = x + (CARD_W - t->sprite->width) / 2;
  if (alive) {
    gfx_sprite(t->sprite, sx, y + 4);
  } else {
    gfx_sprite_tint(t->sprite, sx, y + 4, RGB(140, 145, 160));
    char buf[8];
    uint32_t left = s->respawn_at - clock_ms;
    buf[0] = (char)('0' + (left + 999) / 1000);
    buf[1] = 's';
    buf[2] = 0;
    gfx_text_aligned(&font_medium, x, y + 13, CARD_W, ALIGN_CENTER, buf, COL_WHITE);
  }
  /* reward line */
  char amount[24];
  numfmt(reward(s), amount);
  const sprite_t *icon = t->gives_diamonds ? &spr_gem : &spr_coin;
  int tw = gfx_text_width(&font_small, amount) + icon->width + 3;
  int rx = x + (CARD_W - tw) / 2;
  gfx_sprite(icon, rx, y + 35);
  gfx_text(&font_small, rx + icon->width + 3, y + 35, amount, COL_TEXT);
  /* HP bar */
  int permille = alive ? (int)(s->hp * 1000 / max_hp(s)) : 0;
  gfx_progress_bar(x + 7, y + CARD_H - 14, CARD_W - 14, 7, permille, i == target ? COL_RED : COL_GREEN,
                   COL_HP_BACK);
  /* reward pop animation: "+5" rising above the card */
  if (s->pop_until && (int32_t)(clock_ms - s->pop_until) < 0) {
    int age = POP_MS - (int)(s->pop_until - clock_ms);
    char pop[24] = "+";
    numfmt(s->pop_value, pop + 1);
    gfx_text_shadow(&font_big, x + 8, y + 6 - age * 18 / POP_MS, pop,
                    t->gives_diamonds ? COL_GEM : COL_GOLD, COL_SHADOW);
  }
}

static void draw(void) {
  for (int i = 0; i < SLOTS; i++) draw_slot(i);
  /* team bar: the equipped pets hop while they attack */
  int y = CONTENT_Y + 156;
  ui_panel(6, y, 308, 36);
  bool attacking = target >= 0;
  int slots = game_team_slots();
  for (int i = 0; i < slots; i++) {
    int px = 12 + i * 27;
    gfx_round_rect(px, y + 5, 26, 26, 6, RGB(205, 215, 235));
    if (game.team[i] == NO_PET) continue;
    const pet_t *p = &pets[game.team[i]];
    int hop = attacking && ((clock_ms / 150 + i) & 1) ? -3 : 0;
    gfx_sprite_half(p->sprite, px + 1, y + 6 + hop);
  }
  char buf[24];
  double power = game_team_power();
  int tx = 12 + slots * 27 + 6;
  if (power > 0) {
    gfx_text(&font_small, tx, y + 4, "Dégâts/s", COL_TEXT_DIM);
    gfx_text(&font_medium, tx, y + 17, numfmt(power, buf), COL_TEXT);
    if (slots <= 6) ui_pill(262, y + 11, "AUTO", COL_GREEN, COL_WHITE);
  } else {
    gfx_text(&font_small, tx, y + 4, "OK : frapper l'objet", COL_TEXT);
    gfx_text(&font_small, tx, y + 18, "Ouvre un œuf (onglet 3) !", COL_TEXT_DIM);
  }
}

const screen_t screen_farm = {"Farm", &spr_tab_farm, enter, update, draw};
