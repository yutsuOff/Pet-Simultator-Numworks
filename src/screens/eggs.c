/* EGGS screen: eggs of the unlocked zones (and a preview of the next zone), their
 * pets with the exact hatch chances, and the hatching animation. */
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define ROW_H 27
#define ROWS 7
#define LIST_X 6
#define LIST_W 146
#define LIST_Y (CONTENT_Y + 4)
#define PANEL_X 158
#define PANEL_W 156

/* Hatching animation (ms from the purchase). Rarer pets shake longer. */
#define SHAKE_MS 900
#define CRACK_MS 400
#define FLASH_MS 350

static int selected, scroll;
static bool hatching;
static int hatch_egg, hatch_pet;
static bool hatch_new;
static uint32_t hatch_start, clock_ms;

static int visible_count(void) {
  /* eggs of the unlocked zones + those of the next zone, to show what comes next */
  int n = 0;
  while (n < EGG_COUNT && eggs[n].zone <= game.zones_unlocked) n++;
  return n;
}

static int last_available(void) {
  int e = 0;
  while (e + 1 < EGG_COUNT && game_egg_available(e + 1)) e++;
  return e;
}

static uint32_t shake_ms(void) { return SHAKE_MS + pets[hatch_pet].tier * 250; }
static uint32_t reveal_ms(void) { return shake_ms() + CRACK_MS + FLASH_MS; }

static void enter(void) {
  hatching = false;
  /* start on the best egg of the current zone the player can pay for */
  const zone_t *z = &zones[game.zone];
  selected = z->first_egg + z->egg_count - 1;
  while (selected > z->first_egg && game.coins < eggs[selected].cost) selected--;
  scroll = selected - ROWS + 2;
  if (scroll > visible_count() - ROWS) scroll = visible_count() - ROWS;
  if (scroll < 0) scroll = 0;
}

static void open_egg(uint32_t now) {
  bool is_new;
  if (!game_egg_available(selected)) {
    ui_message("Débloque d'abord sa zone (onglet 4)", now);
    return;
  }
  int pet = game_open_egg(selected, platform_random, &is_new);
  if (pet < 0) {
    ui_message("Pas assez de coins", now);
    return;
  }
  hatching = true;
  hatch_egg = selected;
  hatch_pet = pet;
  hatch_new = is_new;
  hatch_start = now;
}

static int update_hatch(const input_t *in, uint32_t now) {
  uint32_t t = now - hatch_start;
  int r = t < reveal_ms() + 400 ? SCREEN_REDRAW_CONTENT : 0;
  if (pets[hatch_pet].tier == TIER_MYTHICAL && (now / 100) != ((now - 33) / 100)) r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_BACK)) {
    hatching = false;
    return SCREEN_REDRAW_CONTENT | SCREEN_USED_BACK;
  }
  if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
    if (t < reveal_ms()) {
      hatch_start = now - reveal_ms(); /* skip the animation */
    } else {
      selected = hatch_egg;
      hatching = false;
      open_egg(now);
    }
    r |= SCREEN_REDRAW_ALL;
  }
  return r;
}

static int update(const input_t *in, uint32_t now) {
  clock_ms = now;
  if (hatching) return update_hatch(in, now);
  int r = 0, n = visible_count();
  if (input_pressed(in, KEY_UP) && selected > 0) selected--, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_DOWN)) {
    if (selected < n - 1) selected++, r |= SCREEN_REDRAW_CONTENT;
    else return SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_LEFT) && selected > 0) {
    selected = selected > ROWS ? selected - ROWS : 0;
    r |= SCREEN_REDRAW_CONTENT;
  }
  if (input_pressed(in, KEY_RIGHT) && selected < n - 1) {
    selected = selected + ROWS < n ? selected + ROWS : n - 1;
    r |= SCREEN_REDRAW_CONTENT;
  }
  if (input_pressed(in, KEY_TOOLBOX)) selected = last_available(), r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
    open_egg(now);
    r |= SCREEN_REDRAW_ALL;
  }
  if (selected < scroll) scroll = selected;
  if (selected >= scroll + ROWS) scroll = selected - ROWS + 1;
  return r;
}

/* --- List and details --- */

static void draw_row(int e, int y) {
  const egg_t *egg = &eggs[e];
  bool sel = e == selected;
  bool available = game_egg_available(e);
  if (available) ui_card(LIST_X, y, LIST_W, ROW_H - 2, sel, COL_PANEL_TOP, COL_PANEL_BOT);
  else ui_card(LIST_X, y, LIST_W, ROW_H - 2, sel, RGB(205, 210, 220), RGB(180, 185, 200));
  if (sel) y -= 2;
  if (available) gfx_sprite_half(egg->sprite, LIST_X + 4, y + 1);
  else gfx_sprite_half_tint(egg->sprite, LIST_X + 4, y + 1, RGB(120, 125, 145));
  gfx_text(&font_small, LIST_X + 32, y + 1, egg->name, available ? COL_TEXT : COL_TEXT_DIM);
  if (available) {
    ui_coin_amount(LIST_X + 32, y + 12, &font_small, egg->cost, game.coins >= egg->cost ? COL_TEXT : COL_RED);
  } else {
    gfx_text(&font_small, LIST_X + 32, y + 12, "Zone suivante", COL_TEXT_DIM);
  }
}

static void draw_details(void) {
  const egg_t *egg = &eggs[selected];
  int x = PANEL_X, y = CONTENT_Y + 4, w = PANEL_W;
  ui_panel(x, y, w, CONTENT_H - 8);
  gfx_round_gradient(x + 6, y + 6, 52, 52, 26, RGB(255, 245, 210), RGB(250, 215, 140));
  gfx_sprite(egg->sprite, x + 8, y + 8);
  const font_t *f = gfx_text_width(&font_medium, egg->name) <= w - 66 ? &font_medium : &font_small;
  gfx_text(f, x + 62, y + 8, egg->name, COL_TEXT);
  ui_coin_amount(x + 62, y + 26, &font_small, egg->cost, COL_TEXT);
  gfx_text(&font_small, x + 62, y + 41, zones[egg->zone].name, COL_TEXT_DIM);
  gfx_fill_rect(x + 6, y + 60, w - 12, 1, RGB(200, 210, 230));
  for (int i = 0; i < egg->slot_count; i++) {
    const egg_slot_t *s = &egg_slots[egg->first_slot + i];
    const pet_t *p = &pets[s->pet];
    int ry = y + 63 + i * (egg->slot_count > 4 ? 21 : 25);
    bool known = game_discovered(s->pet);
    if (known) gfx_sprite_half(p->sprite, x + 6, ry);
    else gfx_sprite_half_tint(p->sprite, x + 6, ry, RGB(40, 44, 70));
    char chance[16];
    if (s->secret) chance[0] = '?', chance[1] = '?', chance[2] = 0;
    else pctfmt(egg_slot_chance(egg->first_slot + i), chance);
    gfx_text(&font_small, x + 34, ry, known ? p->name : "???", known ? COL_TEXT : COL_TEXT_DIM);
    gfx_text(&font_small, x + 34, ry + 11, chance, gfx_mix(ui_tier_color(p->tier), COL_SHADOW, 70));
  }
  const char *hint = !game_egg_available(selected) ? "Zone verrouillée"
                     : game.coins >= egg->cost     ? "OK : ouvrir l'œuf"
                                                   : "Pas assez de coins";
  gfx_text_aligned(&font_small, x, y + 169, w, ALIGN_CENTER, hint,
                   game.coins >= egg->cost && game_egg_available(selected) ? COL_GREEN_DARK : COL_TEXT_DIM);
}

/* --- Hatching animation --- */

static void draw_rainbow(int y, int h, uint32_t t) {
  for (int j = 0; j < h; j += 4) {
    int hue = (j * 3 + (int)(t / 8)) % 360;
    int seg = hue / 60, f = (hue % 60) * 255 / 60;
    int r = 0, g = 0, b = 0;
    switch (seg) {
      case 0: r = 255, g = f; break;
      case 1: r = 255 - f, g = 255; break;
      case 2: g = 255, b = f; break;
      case 3: g = 255 - f, b = 255; break;
      case 4: r = f, b = 255; break;
      default: r = 255, b = 255 - f; break;
    }
    gfx_fill_rect(0, y + j, 320, 4, gfx_mix(RGB(r, g, b), COL_WHITE, 90));
  }
}

static void draw_crack(int cx, int cy, int segments) {
  static const int8_t pts[][2] = {{-40, -4}, {-28, 6}, {-16, -6}, {-4, 8}, {8, -5}, {20, 7}, {32, -4}, {42, 4}};
  for (int i = 0; i < segments && i < 7; i++) {
    int x0 = pts[i][0], y0 = pts[i][1], x1 = pts[i + 1][0], y1 = pts[i + 1][1];
    for (int k = 0; k <= 12; k++) {
      int x = cx + x0 + (x1 - x0) * k / 12, y = cy + y0 + (y1 - y0) * k / 12;
      gfx_fill_rect(x - 1, y - 1, 3, 3, COL_SHADOW);
    }
  }
}

static void draw_hatch(void) {
  uint32_t t = clock_ms - hatch_start;
  const pet_t *p = &pets[hatch_pet];
  color_t tc = ui_tier_color(p->tier);
  int cx = 160, cy = CONTENT_Y + 84;
  if (t < reveal_ms()) {
    gfx_vgradient(0, CONTENT_Y, 320, CONTENT_H, COL_BAR_LIGHT, COL_SHADOW);
    gfx_round_rect(cx - 60, cy - 60, 120, 120, 60, gfx_mix(COL_BAR_LIGHT, COL_WHITE, 40));
    uint32_t shake = shake_ms();
    if (t < shake + CRACK_MS) {
      int amp = t < shake ? 1 + (int)(t * 6 / shake) : 7;
      int dx = ((t / 70) & 1) ? amp : -amp;
      gfx_sprite_scaled(eggs[hatch_egg].sprite, cx - 48 + dx, cy - 48, 2);
      if (t >= shake) draw_crack(cx + dx, cy, 1 + (int)((t - shake) * 7 / CRACK_MS));
      gfx_text_aligned(&font_medium, 0, CONTENT_Y + CONTENT_H - 26, 320, ALIGN_CENTER, eggs[hatch_egg].name,
                       COL_WHITE);
    } else {
      /* flash, tinted by the rarity */
      int a = 255 - (int)((t - shake - CRACK_MS) * 255 / FLASH_MS);
      gfx_fill_rect(0, CONTENT_Y, 320, CONTENT_H, gfx_mix(COL_WHITE, tc, p->tier * 40));
      gfx_blend_rect(0, CONTENT_Y, 320, CONTENT_H, COL_WHITE, a);
    }
    return;
  }
  /* reveal */
  uint32_t rt = t - reveal_ms();
  cy = CONTENT_Y + 70;
  if (p->tier == TIER_MYTHICAL) draw_rainbow(CONTENT_Y, CONTENT_H, clock_ms);
  else gfx_vgradient(0, CONTENT_Y, 320, CONTENT_H, gfx_mix(COL_WHITE, tc, 90), gfx_mix(tc, COL_SHADOW, 40));
  for (int i = 3; i >= 1; i--) {
    int r = 38 + i * 8;
    gfx_round_rect(cx - r, cy - r + 4, 2 * r, 2 * r, r, gfx_mix(tc, COL_WHITE, 70 + i * 50));
  }
  int bounce = rt < 300 ? (int)(300 - rt) / 15 : 0;
  gfx_sprite_scaled(p->sprite, cx - 48, cy - 44 - bounce, 2);
  if (hatch_new) ui_pill(cx + 40, CONTENT_Y + 10, "NOUVEAU !", COL_GOLD_DARK, COL_WHITE);
  int ty = CONTENT_Y + 138;
  gfx_text_aligned(&font_big, 1, ty + 1, 320, ALIGN_CENTER, p->name, COL_SHADOW);
  gfx_text_aligned(&font_big, 0, ty, 320, ALIGN_CENTER, p->name, COL_WHITE);
  char chance[16] = "";
  for (int s = eggs[hatch_egg].first_slot; s < eggs[hatch_egg].first_slot + eggs[hatch_egg].slot_count; s++) {
    if (egg_slots[s].pet != hatch_pet) continue;
    if (egg_slots[s].secret) chance[0] = '?', chance[1] = '?', chance[2] = 0;
    else pctfmt(egg_slot_chance(s), chance);
  }
  const char *tier = ui_tier_name(p->tier);
  int pw = gfx_text_width(&font_small, tier) + 10, cw = gfx_text_width(&font_small, chance) + 10;
  int px = (320 - pw - cw - 6) / 2;
  ui_pill(px, ty + 25, tier, gfx_mix(tc, COL_SHADOW, 50), COL_WHITE);
  ui_pill(px + pw + 6, ty + 25, chance, COL_BAR, COL_WHITE);
  gfx_text_aligned(&font_small, 0, CONTENT_Y + CONTENT_H - 14, 320, ALIGN_CENTER,
                   "OK : rouvrir   ·   Retour : fermer", COL_WHITE);
}

static void draw(void) {
  if (hatching) {
    draw_hatch();
    return;
  }
  int n = visible_count();
  for (int i = 0; i < ROWS && scroll + i < n; i++) draw_row(scroll + i, LIST_Y + i * ROW_H);
  draw_details();
}

const screen_t screen_eggs = {"Œufs", &spr_tab_eggs, enter, update, draw};
