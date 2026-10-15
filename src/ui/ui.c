#include "ui.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../game/game.h"

void ui_background(color_t top, color_t bottom) {
  gfx_vgradient(0, CONTENT_Y, 320, CONTENT_H, top, bottom);
}

static int amount(int right, int y, const sprite_t *icon, double value) {
  char buf[24];
  numfmt(value, buf);
  int tw = gfx_text_width(&font_medium, buf);
  int w = tw + icon->width + 14;
  int x = right - w;
  gfx_round_rect(x, y, w, 18, 9, COL_BAR_LIGHT);
  gfx_sprite(icon, x + 3, y + 2);
  gfx_text(&font_medium, x + icon->width + 7, y + 2, buf, COL_WHITE);
  return x;
}

void ui_top_bar(const char *zone_name, color_t zone_color, double coins, double diamonds) {
  gfx_vgradient(0, 0, 320, TOPBAR_H, COL_BAR_LIGHT, COL_BAR);
  gfx_fill_rect(0, TOPBAR_H - 1, 320, 1, COL_SHADOW);
  /* zone badge */
  int tw = gfx_text_width(&font_medium, zone_name);
  gfx_round_gradient(3, 2, tw + 14, 18, 9, gfx_mix(zone_color, COL_WHITE, 70), zone_color);
  gfx_text_shadow(&font_medium, 10, 4, zone_name, COL_WHITE, COL_SHADOW);
  int x = amount(317, 2, &spr_gem, diamonds);
  amount(x - 4, 2, &spr_coin, coins);
}

void ui_tab_bar(const tab_t *tabs, int count, int current, bool focused) {
  int y = 240 - TABBAR_H;
  gfx_vgradient(0, y, 320, TABBAR_H, COL_BAR_LIGHT, COL_BAR);
  gfx_fill_rect(0, y, 320, 1, COL_SHADOW);
  /* The current tab is wide (icon + label), the others show their icon only. */
  int label_w = gfx_text_width(&font_small, tabs[current].label);
  int wide = label_w + 30;
  int narrow = (320 - 4 - wide) / (count - 1);
  int x = 2;
  for (int i = 0; i < count; i++) {
    int w = i == current ? wide : narrow;
    if (i == current) {
      color_t top = focused ? COL_GOLD : RGB(90, 160, 255);
      color_t bottom = focused ? COL_GOLD_DARK : COL_BLUE_DARK;
      gfx_round_gradient(x + 1, y + 3, w - 2, TABBAR_H - 5, 8, top, bottom);
      gfx_sprite(tabs[i].icon, x + 7, y + 4);
      gfx_text_shadow(&font_small, x + 26, y + 6, tabs[i].label, COL_WHITE, COL_SHADOW);
    } else {
      gfx_sprite_tint(tabs[i].icon, x + (w - 16) / 2, y + 4, RGB(150, 165, 210));
    }
    x += w;
  }
}

void ui_panel(int x, int y, int w, int h) {
  gfx_round_rect(x, y, w, h, 8, COL_OUTLINE);
  gfx_round_gradient(x + 2, y + 2, w - 4, h - 4, 6, COL_PANEL_TOP, COL_PANEL_BOT);
}

void ui_card(int x, int y, int w, int h, bool selected, color_t top, color_t bottom) {
  if (selected) {
    y -= 2;
    gfx_round_rect(x + 1, y + 4, w, h, 7, gfx_mix(COL_SHADOW, COL_WHITE, 60)); /* drop shadow */
    gfx_round_rect(x - 1, y - 1, w + 2, h + 2, 8, COL_GOLD);
    gfx_round_gradient(x + 2, y + 2, w - 4, h - 4, 5, top, bottom);
    gfx_round_frame(x - 1, y - 1, w + 2, h + 2, 8, 3, COL_GOLD);
  } else {
    gfx_round_rect(x, y, w, h, 7, COL_OUTLINE);
    gfx_round_gradient(x + 2, y + 2, w - 4, h - 4, 5, top, bottom);
  }
}

void ui_button(int x, int y, int w, int h, const char *label, bool selected, color_t color) {
  gfx_round_rect(x, y + 2, w, h, h / 2, gfx_mix(color, COL_SHADOW, 140));
  gfx_round_gradient(x, y, w, h, h / 2, gfx_mix(color, COL_WHITE, 80), color);
  if (selected) gfx_round_frame(x - 2, y - 2, w + 4, h + 6, h / 2 + 2, 2, COL_GOLD);
  gfx_text_aligned(&font_medium, x + 1, y + (h - 14) / 2 + 1, w, ALIGN_CENTER, label, COL_SHADOW);
  gfx_text_aligned(&font_medium, x, y + (h - 14) / 2, w, ALIGN_CENTER, label, COL_WHITE);
}

int ui_pill(int x, int y, const char *text, color_t bg, color_t fg) {
  int w = gfx_text_width(&font_small, text) + 10;
  gfx_round_rect(x, y, w, 14, 7, bg);
  gfx_text(&font_small, x + 5, y + 1, text, fg);
  return w;
}

color_t ui_tier_color(int tier) {
  static const color_t colors[TIER_COUNT] = {COL_RARITY_BASIC, COL_RARITY_RARE, COL_RARITY_EPIC,
                                             COL_RARITY_LEGENDARY, COL_RARITY_MYTHICAL, COL_RARITY_EXCLUSIVE};
  return colors[tier];
}

const char *ui_tier_name(int tier) {
  static const char *const names[TIER_COUNT] = {"Basique", "Rare", "Épique", "Légendaire", "Mythique", "Exclusif"};
  return names[tier];
}

int ui_coin_amount(int x, int y, const font_t *f, double value, color_t c) {
  char buf[24];
  numfmt(value, buf);
  gfx_sprite(&spr_coin, x, y + (f->line_height - spr_coin.height) / 2);
  return spr_coin.width + 3 + gfx_text(f, x + spr_coin.width + 3, y, buf, c);
}

/* --- Short message shown over the content ("Pas assez de coins"...) --- */

#define MESSAGE_MS 1800
static const char *message;
static uint32_t message_until;

void ui_message(const char *text, uint32_t now) {
  message = text;
  message_until = now + MESSAGE_MS;
}

bool ui_message_expired(uint32_t now) {
  if (message && (int32_t)(now - message_until) >= 0) {
    message = 0;
    return true;
  }
  return false;
}

void ui_message_draw(void) {
  if (!message) return;
  int w = gfx_text_width(&font_medium, message) + 24;
  int x = (320 - w) / 2, y = CONTENT_Y + CONTENT_H - 34;
  gfx_round_rect(x, y + 2, w, 24, 12, COL_SHADOW);
  gfx_round_gradient(x, y, w, 24, 12, COL_BAR_LIGHT, COL_BAR);
  gfx_text(&font_medium, x + 12, y + 5, message, COL_WHITE);
}

/* --- Pets with their variant --- */

void ui_sparkles(int x, int y, int w, int h, uint32_t t) {
  for (int i = 0; i < 3; i++) {
    uint32_t k = t / 260 + (uint32_t)i * 7;
    uint32_t hash = k * 2654435761u;
    int sx = x + (int)(hash % (uint32_t)w), sy = y + (int)((hash >> 12) % (uint32_t)h);
    int size = 1 + (int)((t / 65 + (uint32_t)i) % 4 == 0) + (int)((t / 130 + (uint32_t)i) % 3 == 0);
    gfx_fill_rect(sx - size, sy, 2 * size + 1, 1, COL_WHITE);
    gfx_fill_rect(sx, sy - size, 1, 2 * size + 1, COL_WHITE);
  }
}

void ui_pet(int pet, int variant, int x, int y, int size, uint32_t t) {
  const sprite_t *s = pets[pet].sprite;
  int level = variant_level(variant);
  gfx_set_effect(level == VAR_GOLDEN ? GFX_EFFECT_GOLDEN : level == VAR_RAINBOW ? GFX_EFFECT_RAINBOW : GFX_EFFECT_NONE,
                 (int)(t / 12));
  int px = 48;
  if (size == PET_HALF) gfx_sprite_half(s, x, y), px = 24;
  else if (size == PET_FULL) gfx_sprite(s, x, y);
  else gfx_sprite_scaled(s, x, y, size == PET_X2 ? 2 : 3), px = size == PET_X2 ? 96 : 144;
  gfx_set_effect(GFX_EFFECT_NONE, 0);
  if (variant_shiny(variant)) ui_sparkles(x, y, px, px, t);
}

char *ui_pet_name(int pet, int variant, char *buf) {
  char *q = buf;
  static const char *const prefix[3] = {"", "Golden ", "Rainbow "};
  if (variant_shiny(variant)) for (const char *s = "Shiny "; *s;) *q++ = *s++;
  for (const char *s = prefix[variant_level(variant)]; *s;) *q++ = *s++;
  for (const char *s = pets[pet].name; *s;) *q++ = *s++;
  *q = 0;
  return buf;
}

/* --- Items --- */

static const char *const potion_names[POTION_TYPE_COUNT] = {"Coins", "Dégâts", "Diamants", "Chance", "Trésor"};
static const char *const enchant_names[ENCHANT_TYPE_COUNT] = {"Coins", "Diamants", "Chance", "Force", "Coup",
                                                              "Trésor"};

char *ui_item_name(int item, char *buf) {
  static const char *const roman[4] = {"", " I", " II", " III"};
  bool enchant = item >= ITEM_ENCHANT;
  const char *kind = enchant ? "Enchant " : "Potion ";
  const char *name = enchant ? enchant_names[enchants[item - ITEM_ENCHANT].type] : potion_names[potions[item].type];
  int tier = enchant ? enchants[item - ITEM_ENCHANT].tier : potions[item].tier;
  char *q = buf;
  for (const char *s = kind; *s;) *q++ = *s++;
  for (const char *s = name; *s;) *q++ = *s++;
  for (const char *s = roman[tier]; *s;) *q++ = *s++;
  *q = 0;
  return buf;
}

const sprite_t *ui_item_sprite(int item) {
  return item >= ITEM_ENCHANT ? enchants[item - ITEM_ENCHANT].sprite : potions[item].sprite;
}
