/* FARM screen: the breakables of the current zone. OK targets the selected object
 * (and gives it a hit); the equipped pets then attack the target on their own and
 * move to the next object when it breaks (AUTO). Some zones hold a machine in their
 * last slot (machines.c). */
#include "screens.h"
#include "../game/breakables.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define COLS 4
#define ROWS 2
#define SLOTS (COLS * ROWS)
#define MACHINE_SLOT (SLOTS - 1)
#define CARD_W 70
#define CARD_H 64
#define POP_MS 700
#define TAP_PILES 8 /* a hit without pets: 1/8 of a coin pile */

static const sprite_t *const sprites[BRK_COUNT] = {&spr_brk_coin, &spr_brk_coins, &spr_brk_crate, &spr_brk_gift,
                                                   &spr_brk_chest, &spr_brk_gem, &spr_brk_gems};

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
static char drop_text[48];

static int zone_machine(void) { return game_zone_machine(game.zone); }

static bool is_machine(int slot) { return slot == MACHINE_SLOT && zone_machine() != NO_MACHINE; }

/* --- Breakables --- */

static void spawn(slot_t *s) {
  int r = (int)(platform_random() % 1000), type = 0;
  while (type < BRK_COUNT - 1 && r >= breakables[type].weight) r -= breakables[type++].weight;
  s->type = (uint8_t)type;
  s->hp = breakable_hp(type);
  s->respawn_at = 0;
}

static void reset_zone(void) {
  for (int i = 0; i < SLOTS; i++) {
    spawn(&slots[i]);
    slots[i].pop_until = 0;
  }
  /* always start with plain coin piles in the first row */
  for (int i = 0; i < COLS; i++) {
    slots[i].type = BRK_PILE;
    slots[i].hp = breakable_hp(BRK_PILE);
  }
  farm_zone = game.zone;
  target = -1;
  machine_close();
}

static void enter(void) {
  if (farm_zone != game.zone) reset_zone();
}

static int next_alive(int from) {
  for (int k = 1; k <= SLOTS; k++) {
    int i = (from + k) % SLOTS;
    if (slots[i].respawn_at == 0 && !is_machine(i)) return i;
  }
  return -1;
}

/* Applies damage to the target; returns the redraw bits. */
static int hit(double damage, uint32_t now) {
  slot_t *s = &slots[target];
  s->hp -= damage;
  if (s->hp > 0) return 0;
  double value = breakable_reward(s->type);
  int item = breakable_destroyed(s->type, value);
  if (item >= 0) {
    char name[40], *q = drop_text;
    ui_item_name(item, name);
    for (const char *t = "Trouvé : "; *t;) *q++ = *t++;
    for (const char *t = name; *t;) *q++ = *t++;
    *q = 0;
    ui_message(drop_text, now);
  }
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
  static const input_t no_keys;
  bool in_machine = machine_is_open();
  if (in_machine) {
    result |= machine_update(in, now);
    in = &no_keys; /* the pets keep farming behind the machine */
  }
  int col = selected % COLS, row = selected / COLS;
  if (input_pressed(in, KEY_LEFT) && col > 0) selected--, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT) && col < COLS - 1) selected++, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_UP) && row > 0) selected -= COLS, result |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_DOWN)) {
    if (row < ROWS - 1) selected += COLS, result |= SCREEN_REDRAW_CONTENT;
    else result |= SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
    if (is_machine(selected)) {
      machine_open(zone_machine());
      result |= SCREEN_REDRAW_CONTENT;
    } else if (slots[selected].respawn_at == 0) {
      target = selected;
      double tap = (zones[game.zone].pile_hp / TAP_PILES + game_team_power() / 5) * game_tap_bonus();
      result |= SCREEN_REDRAW_CONTENT | hit(tap, now);
    }
  }
  /* pets attack the target continuously */
  double power = game_team_power();
  if (target >= 0 && power > 0 && dt) {
    result |= hit(power * dt / 1000.0, now);
    if (!in_machine && (now / 100) != ((now - dt) / 100)) result |= SCREEN_REDRAW_CONTENT;
  }
  if (target < 0 && power > 0) target = next_alive(SLOTS - 1); /* AUTO */
  if (machine_is_open()) return result;
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
  /* animated pets in the team bar (rainbow, shiny) */
  for (int i = 0; i < game_team_slots(); i++) {
    if (game.team[i].pet != NO_PET && game.team[i].variant != VAR_NORMAL && (now / 120) != ((now - 33) / 120)) {
      result |= SCREEN_REDRAW_CONTENT;
    }
  }
  return result;
}

static void draw_slot(int i) {
  const slot_t *s = &slots[i];
  int x = 8 + (i % COLS) * (CARD_W + 8);
  int y = CONTENT_Y + 10 + (i / COLS) * (CARD_H + 8);
  bool sel = i == selected;
  if (is_machine(i)) {
    machine_draw_card(zone_machine(), x, y, CARD_W, CARD_H, sel);
    return;
  }
  const sprite_t *sprite = sprites[s->type];
  bool diamonds = breakables[s->type].gives_diamonds;
  bool alive = s->respawn_at == 0;
  if (i == target && !sel) gfx_round_rect(x - 2, y - 2, CARD_W + 4, CARD_H + 4, 9, COL_RED);
  if (alive) ui_card(x, y, CARD_W, CARD_H, sel, COL_PANEL_TOP, COL_PANEL_BOT);
  else ui_card(x, y, CARD_W, CARD_H, sel, RGB(200, 205, 215), RGB(170, 175, 190));
  if (sel) y -= 2;
  int sx = x + (CARD_W - sprite->width) / 2;
  if (alive) {
    gfx_sprite(sprite, sx, y + 4);
  } else {
    gfx_sprite_tint(sprite, sx, y + 4, RGB(140, 145, 160));
    char buf[8];
    uint32_t left = s->respawn_at - clock_ms;
    buf[0] = (char)('0' + (left + 999) / 1000);
    buf[1] = 's';
    buf[2] = 0;
    gfx_text_aligned(&font_medium, x, y + 13, CARD_W, ALIGN_CENTER, buf, COL_WHITE);
  }
  /* reward line */
  char amount[24];
  numfmt(breakable_reward(s->type), amount);
  const sprite_t *icon = diamonds ? &spr_gem : &spr_coin;
  int tw = gfx_text_width(&font_small, amount) + icon->width + 3;
  int rx = x + (CARD_W - tw) / 2;
  gfx_sprite(icon, rx, y + 35);
  gfx_text(&font_small, rx + icon->width + 3, y + 35, amount, COL_TEXT);
  /* HP bar */
  int permille = alive ? (int)(s->hp * 1000 / breakable_hp(s->type)) : 0;
  gfx_progress_bar(x + 7, y + CARD_H - 14, CARD_W - 14, 7, permille, i == target ? COL_RED : COL_GREEN,
                   COL_HP_BACK);
  /* reward pop animation: "+5" rising above the card */
  if (s->pop_until && (int32_t)(clock_ms - s->pop_until) < 0) {
    int age = POP_MS - (int)(s->pop_until - clock_ms);
    char pop[24] = "+";
    numfmt(s->pop_value, pop + 1);
    gfx_text_shadow(&font_big, x + 8, y + 6 - age * 18 / POP_MS, pop, diamonds ? COL_GEM : COL_GOLD, COL_SHADOW);
  }
}

static void draw(void) {
  if (machine_is_open()) {
    machine_draw(clock_ms);
    return;
  }
  for (int i = 0; i < SLOTS; i++) draw_slot(i);
  /* team bar: the equipped pets hop while they attack */
  int y = CONTENT_Y + 156;
  ui_panel(6, y, 308, 36);
  bool attacking = target >= 0;
  int team = game_team_slots();
  for (int i = 0; i < team; i++) {
    int px = 12 + i * 27;
    gfx_round_rect(px, y + 5, 26, 26, 6, RGB(205, 215, 235));
    if (game.team[i].pet == NO_PET) continue;
    int hop = attacking && ((clock_ms / 150 + i) & 1) ? -3 : 0;
    ui_pet(game.team[i].pet, game.team[i].variant, px + 1, y + 6 + hop, PET_HALF, clock_ms);
  }
  char buf[24];
  double power = game_team_power();
  int tx = 12 + team * 27 + 6;
  if (power > 0) {
    gfx_text(&font_small, tx, y + 4, "Dégâts/s", COL_TEXT_DIM);
    gfx_text(&font_medium, tx, y + 17, numfmt(power, buf), COL_TEXT);
    if (team <= 6) ui_pill(262, y + 11, "AUTO", COL_GREEN, COL_WHITE);
  } else {
    gfx_text(&font_small, tx, y + 4, "OK : frapper l'objet", COL_TEXT);
    gfx_text(&font_small, tx, y + 18, "Ouvre un œuf (onglet 3) !", COL_TEXT_DIM);
  }
}

const screen_t screen_farm = {"Farm", &spr_tab_farm, enter, update, draw};
