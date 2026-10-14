/* ZONES screen: the areas of Pet Simulator 99, unlocked in order with coins.
 * OK teleports to an unlocked zone, or buys the next one. */
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define CARD_H 46
#define ROWS 4
#define LIST_Y (CONTENT_Y + 6)

static int selected, scroll;
static char message[48];

static void enter(void) {
  selected = game.zone;
  scroll = selected - 1;
  if (scroll > ZONE_COUNT - ROWS) scroll = ZONE_COUNT - ROWS;
  if (scroll < 0) scroll = 0;
}

static void say(const char *a, const char *b, uint32_t now) {
  char *q = message;
  while (*a && q < message + sizeof(message) - 1) *q++ = *a++;
  while (*b && q < message + sizeof(message) - 1) *q++ = *b++;
  *q = 0;
  ui_message(message, now);
}

static int update(const input_t *in, uint32_t now) {
  int r = 0;
  if (input_pressed(in, KEY_UP) && selected > 0) selected--, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_DOWN)) {
    if (selected < ZONE_COUNT - 1) selected++, r |= SCREEN_REDRAW_CONTENT;
    else return SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_LEFT)) selected = selected > ROWS ? selected - ROWS : 0, r |= SCREEN_REDRAW_CONTENT;
  if (input_pressed(in, KEY_RIGHT)) {
    selected = selected + ROWS < ZONE_COUNT ? selected + ROWS : ZONE_COUNT - 1;
    r |= SCREEN_REDRAW_CONTENT;
  }
  if (input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE)) {
    r |= SCREEN_REDRAW_ALL;
    if (selected < game.zones_unlocked) {
      game.zone = (uint8_t)selected;
      say("Bienvenue : ", zones[selected].name, now);
    } else if (selected > game.zones_unlocked) {
      ui_message("Débloque d'abord les zones précédentes", now);
    } else if (game_unlock_next_zone()) {
      say("Zone débloquée : ", zones[selected].name, now);
    } else {
      char need[24];
      numfmt(zones[selected].cost - game.coins, need);
      say("Il te manque des coins : ", need, now);
    }
  }
  if (selected < scroll) scroll = selected;
  if (selected >= scroll + ROWS) scroll = selected - ROWS + 1;
  return r;
}

static void draw_card(int z, int y) {
  const zone_t *zone = &zones[z];
  bool sel = z == selected;
  bool unlocked = z < game.zones_unlocked;
  bool next = z == game.zones_unlocked;
  int x = 6, w = 300;
  if (unlocked) ui_card(x, y, w, CARD_H - 4, sel, gfx_mix(COL_WHITE, zone->sky_top, 120), zone->sky_bottom);
  else ui_card(x, y, w, CARD_H - 4, sel, RGB(205, 210, 220), RGB(175, 180, 195));
  if (sel) y -= 2;
  gfx_sprite(zone->thumb, x + 4, y + 3);
  if (!unlocked) gfx_blend_rect(x + 4, y + 3, 64, 36, COL_SHADOW, next ? 100 : 170);
  char num[20] = "Zone ";
  numfmt(z + 1, num + 5);
  gfx_text(&font_small, x + 74, y + 4, num, unlocked ? COL_TEXT : COL_TEXT_DIM);
  gfx_text(&font_medium, x + 74 + gfx_text_width(&font_small, num) + 5, y + 2, zone->name,
           unlocked ? COL_TEXT : COL_TEXT_DIM);
  /* eggs of the zone */
  int ex = x + 74;
  for (int i = 0; i < zone->egg_count; i++) {
    const egg_t *egg = &eggs[zone->first_egg + i];
    if (unlocked || next) gfx_sprite_half(egg->sprite, ex, y + 17);
    else gfx_sprite_half_tint(egg->sprite, ex, y + 17, RGB(120, 125, 145));
    ex += 24;
  }
  gfx_text(&font_small, ex + 2, y + 23, zone->egg_count > 1 ? "2 œufs" : eggs[zone->first_egg].name,
           COL_TEXT_DIM);
  /* status */
  int sx = x + w - 8;
  if (z == game.zone) {
    sx -= gfx_text_width(&font_small, "Tu es ici") + 10;
    ui_pill(sx, y + 14, "Tu es ici", COL_GREEN_DARK, COL_WHITE);
  } else if (unlocked) {
    sx -= gfx_text_width(&font_small, "Débloquée") + 10;
    ui_pill(sx, y + 14, "Débloquée", COL_BLUE, COL_WHITE);
  } else {
    char price[24];
    numfmt(zone->cost, price);
    int pw = gfx_text_width(&font_medium, price) + spr_coin.width + 14;
    sx -= pw;
    color_t bg = next && game.coins >= zone->cost ? COL_GREEN_DARK : next ? COL_BAR_LIGHT : COL_LOCKED;
    gfx_round_rect(sx, y + 11, pw, 20, 10, bg);
    ui_coin_amount(sx + 5, y + 14, &font_medium, zone->cost, COL_WHITE);
  }
}

static void draw(void) {
  for (int i = 0; i < ROWS && scroll + i < ZONE_COUNT; i++) draw_card(scroll + i, LIST_Y + i * CARD_H);
  /* scroll indicator */
  int bar_h = ROWS * CARD_H - 6;
  gfx_round_rect(312, LIST_Y, 4, bar_h, 2, gfx_mix(COL_OUTLINE, COL_WHITE, 120));
  int th = bar_h * ROWS / ZONE_COUNT;
  gfx_round_rect(312, LIST_Y + (bar_h - th) * scroll / (ZONE_COUNT - ROWS), 4, th, 2, COL_WHITE);
}

const screen_t screen_zones = {"Zones", &spr_tab_zones, enter, update, draw};
