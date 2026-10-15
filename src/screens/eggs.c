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
static int hatch_egg, hatch_count;
static hatch_t hatch[MAX_HATCH];
static int hatch_tier; /* best tier of the batch: drives the animation */
static bool hatch_huge; /* a huge in the batch: special animation */
static char reward_text[48];
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

static uint32_t shake_ms(void) { return hatch_huge ? 2600 : SHAKE_MS + hatch_tier * 250; }
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

/* Opens up to game_hatch_count() eggs at once (as many as the coins allow). */
static void open_egg(uint32_t now) {
  if (!game_egg_available(selected)) {
    ui_message("Débloque d'abord sa zone (onglet 4)", now);
    return;
  }
  hatch_count = 0;
  hatch_tier = 0;
  hatch_huge = false;
  for (int i = 0; i < game_hatch_count(); i++) {
    if (!game_open_egg(selected, &hatch[hatch_count])) break;
    const pet_t *p = &pets[hatch[hatch_count].pet];
    if (p->tier > hatch_tier) hatch_tier = p->tier;
    hatch_huge |= p->huge;
    hatch_count++;
  }
  if (!hatch_count) {
    ui_message("Pas assez de coins", now);
    return;
  }
  hatching = true;
  hatch_egg = selected;
  hatch_start = now;
  double diamonds = game_claim_index_rewards();
  if (diamonds > 0) {
    char n[16];
    char *q = reward_text;
    for (const char *s = "Œuf complété ! +"; *s;) *q++ = *s++;
    for (const char *s = numfmt(diamonds, n); *s;) *q++ = *s++;
    for (const char *s = " diamants"; *s;) *q++ = *s++;
    *q = 0;
    ui_message(reward_text, now);
  }
}

static int update_hatch(const input_t *in, uint32_t now) {
  uint32_t t = now - hatch_start;
  int r = t < reveal_ms() + 400 ? SCREEN_REDRAW_CONTENT : 0;
  for (int k = 0; k < hatch_count; k++) {
    if (hatch[k].variant != VAR_NORMAL && (now / 100) != ((now - 33) / 100)) r |= SCREEN_REDRAW_CONTENT;
  }
  if (hatch_tier >= TIER_MYTHICAL && (now / 100) != ((now - 33) / 100)) r |= SCREEN_REDRAW_CONTENT;
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
  int nw = gfx_text(&font_small, LIST_X + 32, y + 1, egg->name, available ? COL_TEXT : COL_TEXT_DIM);
  if (game_egg_complete(e)) gfx_text(&font_small, LIST_X + 36 + nw, y + 1, "★", COL_GOLD_DARK);
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
                     : game.coins < egg->cost      ? "Pas assez de coins"
                     : game_hatch_count() == 1     ? "OK : ouvrir l'œuf"
                     : game_hatch_count() == 2     ? "OK : ouvrir 2 œufs"
                                                   : "OK : ouvrir 3 œufs";
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

/* Horizontal center of egg/pet number i of the batch. */
static int column(int i) { return hatch_count == 1 ? 160 : hatch_count == 2 ? 104 + i * 112 : 56 + i * 104; }

static void chance_text(int pet, int variant, char *buf) {
  buf[0] = 0;
  if (pets[pet].huge) { /* "1/75 000" */
    unsigned k = huge_odds[variant] / 1000;
    *buf++ = '1', *buf++ = '/';
    if (k >= 100) *buf++ = (char)('0' + k / 100);
    *buf++ = (char)('0' + k / 10 % 10), *buf++ = (char)('0' + k % 10);
    for (const char *src = " 000"; (*buf++ = *src++);) {}
    return;
  }
  for (int s = eggs[hatch_egg].first_slot; s < eggs[hatch_egg].first_slot + eggs[hatch_egg].slot_count; s++) {
    if (egg_slots[s].pet != pet) continue;
    if (egg_slots[s].secret) buf[0] = '?', buf[1] = '?', buf[2] = 0;
    else pctfmt(egg_slot_chance(s), buf);
  }
}

/* "Golden", "Shiny Rainbow"... or NULL for a normal pet */
static const char *variant_label(int v) {
  static const char *const labels[VARIANT_COUNT] = {0, "Golden", "Rainbow", "Shiny", "Shiny Golden", "Shiny Rainbow"};
  return labels[v];
}

static void draw_star(int x, int y, int r, color_t c) { /* 4-branch sparkle */
  for (int k = 0; k <= r; k++) {
    int w = (r - k) / 2;
    gfx_fill_rect(x - w, y - k, 2 * w + 1, 1, c);
    gfx_fill_rect(x - w, y + k, 2 * w + 1, 1, c);
    gfx_fill_rect(x - k, y - w, 1, 2 * w + 1, c);
    gfx_fill_rect(x + k, y - w, 1, 2 * w + 1, c);
  }
}

static void draw_hatch(void) {
  uint32_t t = clock_ms - hatch_start;
  color_t tc = ui_tier_color(hatch_tier);
  int cy = CONTENT_Y + 84;
  if (t < reveal_ms()) {
    gfx_vgradient(0, CONTENT_Y, 320, CONTENT_H, COL_BAR_LIGHT, COL_SHADOW);
    uint32_t shake = shake_ms();
    if (hatch_huge && t > 600) { /* the screen darkens and pulses before a huge */
      int pulse = (int)((t / 120) % 2) * 40;
      gfx_blend_rect(0, CONTENT_Y, 320, CONTENT_H, COL_RARITY_EXCLUSIVE, 40 + pulse);
    }
    if (t < shake + CRACK_MS) {
      int amp = t < shake ? 1 + (int)(t * (hatch_huge ? 10 : 6) / shake) : 7;
      for (int i = 0; i < hatch_count; i++) {
        int cx = column(i);
        int dx = ((t / 70 + i) & 1) ? amp : -amp;
        if (hatch_count == 1) gfx_round_rect(cx - 60, cy - 60, 120, 120, 60, gfx_mix(COL_BAR_LIGHT, COL_WHITE, 40));
        gfx_sprite_scaled(eggs[hatch_egg].sprite, cx - 48 + dx, cy - 48, 2);
        if (t >= shake) draw_crack(cx + dx, cy, 1 + (int)((t - shake) * 7 / CRACK_MS));
      }
      const char *label = hatch_huge && t > 1200 ? "Quelque chose d'énorme arrive..." : eggs[hatch_egg].name;
      gfx_text_aligned(&font_medium, 0, CONTENT_Y + CONTENT_H - 26, 320, ALIGN_CENTER, label, COL_WHITE);
    } else {
      /* flash, tinted by the rarity */
      int a = 255 - (int)((t - shake - CRACK_MS) * 255 / FLASH_MS);
      gfx_fill_rect(0, CONTENT_Y, 320, CONTENT_H, gfx_mix(COL_WHITE, tc, hatch_tier * 40));
      gfx_blend_rect(0, CONTENT_Y, 320, CONTENT_H, COL_WHITE, a);
    }
    return;
  }
  /* reveal */
  uint32_t rt = t - reveal_ms();
  cy = CONTENT_Y + 70;
  if (hatch_tier >= TIER_MYTHICAL) draw_rainbow(CONTENT_Y, CONTENT_H, clock_ms);
  else gfx_vgradient(0, CONTENT_Y, 320, CONTENT_H, gfx_mix(COL_WHITE, tc, 90), gfx_mix(tc, COL_SHADOW, 40));
  if (hatch_huge) { /* sparkles all over the screen and a big title */
    for (int i = 0; i < 10; i++) {
      uint32_t h = (clock_ms / 180 + (uint32_t)i * 13) * 2654435761u;
      draw_star((int)(h % 320), CONTENT_Y + 4 + (int)((h >> 10) % (CONTENT_H - 8)), 2 + (int)((h >> 20) % 4), COL_WHITE);
    }
  }
  int bounce = rt < 300 ? (int)(300 - rt) / 15 : 0;
  bool big = hatch_count == 1 && hatch_huge;
  for (int k = 0; k < hatch_count; k++) {
    const pet_t *p = &pets[hatch[k].pet];
    color_t pc = ui_tier_color(p->tier);
    int cx = column(k), pcy = big ? CONTENT_Y + 80 : cy;
    for (int i = 3; i >= 1; i--) {
      int r = big ? 42 + i * 9 : hatch_count == 1 ? 38 + i * 8 : 30 + i * 6;
      gfx_round_rect(cx - r, pcy - r + 4, 2 * r, 2 * r, r, gfx_mix(pc, COL_WHITE, 70 + i * 50));
    }
    ui_pet(hatch[k].pet, hatch[k].variant, cx - 48, pcy - 44 - bounce, PET_X2, clock_ms);
    char name[48], chance[16];
    ui_pet_name(hatch[k].pet, hatch[k].variant, name);
    chance_text(hatch[k].pet, hatch[k].variant, chance);
    const char *tier = ui_tier_name(p->tier);
    const char *label = variant_label(hatch[k].variant);
    int pw = gfx_text_width(&font_small, tier) + 10, cw = gfx_text_width(&font_small, chance) + 10;
    int ty = CONTENT_Y + 138;
    if (big) {
      int sw = gfx_text_width(&font_big, "HUGE !");
      int wob = (int)((clock_ms / 90) % 4) - 2;
      gfx_text_shadow(&font_big, 160 - sw / 2 + wob, CONTENT_Y + 6, "HUGE !", COL_GOLD, COL_SHADOW);
      gfx_text_aligned(&font_big, 1, CONTENT_Y + 147, 320, ALIGN_CENTER, name, COL_SHADOW);
      gfx_text_aligned(&font_big, 0, CONTENT_Y + 146, 320, ALIGN_CENTER, name, COL_WHITE);
      int px = (320 - pw - cw - 6) / 2;
      ui_pill(px, CONTENT_Y + 166, tier, gfx_mix(pc, COL_SHADOW, 50), COL_WHITE);
      ui_pill(px + pw + 6, CONTENT_Y + 166, chance, COL_BAR, COL_WHITE);
    } else if (hatch_count == 1) {
      if (hatch[k].is_new) ui_pill(cx + 40, CONTENT_Y + 10, "NOUVEAU !", COL_GOLD_DARK, COL_WHITE);
      if (label) ui_pill(cx - 100, CONTENT_Y + 10, label, COL_BAR, COL_GOLD);
      gfx_text_aligned(&font_big, 1, ty + 1, 320, ALIGN_CENTER, name, COL_SHADOW);
      gfx_text_aligned(&font_big, 0, ty, 320, ALIGN_CENTER, name, COL_WHITE);
      int px = (320 - pw - cw - 6) / 2;
      ui_pill(px, ty + 25, tier, gfx_mix(pc, COL_SHADOW, 50), COL_WHITE);
      ui_pill(px + pw + 6, ty + 25, chance, COL_BAR, COL_WHITE);
    } else {
      if (hatch[k].is_new || p->huge) {
        const char *tag = p->huge ? "HUGE !" : "NOUVEAU";
        int nw = gfx_text_width(&font_small, tag) + 10;
        ui_pill(cx - nw / 2, CONTENT_Y + 6, tag, p->huge ? COL_RARITY_EXCLUSIVE : COL_GOLD_DARK, COL_WHITE);
      }
      gfx_text_aligned(&font_small, cx - 52 + 1, ty + 1, 104, ALIGN_CENTER, name, COL_SHADOW);
      gfx_text_aligned(&font_small, cx - 52, ty, 104, ALIGN_CENTER, name, COL_WHITE);
      ui_pill(cx - pw / 2, ty + 15, tier, gfx_mix(pc, COL_SHADOW, 50), COL_WHITE);
      ui_pill(cx - cw / 2, ty + 31, chance, COL_BAR, COL_WHITE);
    }
  }
  gfx_text_aligned(&font_small, 0, CONTENT_Y + CONTENT_H - 13, 320, ALIGN_CENTER,
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
