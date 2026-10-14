/* Main loop: title screen, tab navigation and screen dispatch. */
#include "game/game.h"
#include "game/zones.h"
#include "generated/sprites.h"
#include "input/input.h"
#include "platform/platform.h"
#include "screens/screens.h"
#include "ui/ui.h"

#define VERSION "v0.1 · M0"

static const screen_t *const screens[] = {
  &screen_farm, &screen_pets, &screen_eggs, &screen_zones,
  &screen_upgrades, &screen_index, &screen_quests,
};
#define SCREEN_COUNT (int)(sizeof(screens) / sizeof(screens[0]))

static int current;
static bool tabs_focused;
static bool on_title = true;
static bool blink_on = true;

/* --- Title screen --- */

static void draw_title(void) {
  gfx_vgradient(0, 0, 320, 240, RGB(70, 150, 255), RGB(130, 220, 140));
  gfx_round_rect(30, 34, 260, 92, 18, gfx_mix(COL_OUTLINE, RGB(70, 150, 255), 60));
  gfx_round_gradient(34, 38, 252, 84, 15, RGB(255, 225, 90), COL_GOLD_DARK);
  int y = 50;
  gfx_text_aligned(&font_big, 36, y + 2, 252, ALIGN_CENTER, "PET SIMULATOR", COL_SHADOW);
  gfx_text_aligned(&font_big, 34, y, 252, ALIGN_CENTER, "PET SIMULATOR", COL_WHITE);
  gfx_text_aligned(&font_medium, 35, y + 33, 252, ALIGN_CENTER, "NumWorks Edition", COL_SHADOW);
  gfx_text_aligned(&font_medium, 34, y + 32, 252, ALIGN_CENTER, "NumWorks Edition", COL_WHITE);
  const sprite_t *art[] = {&spr_brk_coins, &spr_brk_chest, &spr_brk_present, &spr_brk_gems};
  for (int i = 0; i < 4; i++) gfx_sprite(art[i], 62 + i * 52, 142);
  if (blink_on) {
    gfx_text_aligned(&font_medium, 1, 191, 320, ALIGN_CENTER, "Appuie sur OK pour jouer", COL_SHADOW);
    gfx_text_aligned(&font_medium, 0, 190, 320, ALIGN_CENTER, "Appuie sur OK pour jouer", COL_WHITE);
  }
  gfx_text(&font_small, 6, 224, VERSION, gfx_mix(COL_WHITE, COL_SHADOW, 80));
}

/* --- Game screens --- */

static void draw_game(void) {
  const zone_t *z = &zones[game.zone];
  ui_background(z->sky_top, z->sky_bottom);
  screens[current]->draw();
  ui_top_bar(z->name, z->accent, game.coins, game.diamonds);
  tab_t tabs[SCREEN_COUNT];
  for (int i = 0; i < SCREEN_COUNT; i++) {
    tabs[i].label = screens[i]->label;
    tabs[i].icon = screens[i]->icon;
  }
  ui_tab_bar(tabs, SCREEN_COUNT, current, tabs_focused);
}

static void switch_to(int index) {
  if (index < 0) index = SCREEN_COUNT - 1;
  if (index >= SCREEN_COUNT) index = 0;
  current = index;
  screens[current]->enter();
}

static int shortcut_tab(const input_t *in) {
  static const int keys[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7};
  for (int i = 0; i < SCREEN_COUNT; i++) {
    if (input_pressed(in, keys[i])) return i;
  }
  return -1;
}

/* Returns the redraw bits. */
static int update_game(const input_t *in, uint32_t now) {
  int tab = shortcut_tab(in);
  if (tab >= 0) {
    switch_to(tab);
    tabs_focused = false;
    return SCREEN_REDRAW_ALL;
  }
  if (tabs_focused) {
    if (input_pressed(in, KEY_LEFT) || input_pressed(in, KEY_RIGHT)) {
      switch_to(current + (input_pressed(in, KEY_RIGHT) ? 1 : -1));
      return SCREEN_REDRAW_ALL;
    }
    if (input_pressed(in, KEY_UP) || input_pressed(in, KEY_OK) || input_pressed(in, KEY_EXE) ||
        input_pressed(in, KEY_BACK)) {
      tabs_focused = false;
      return SCREEN_REDRAW_ALL;
    }
    /* keep the current screen alive (timers) without giving it the keys */
    input_t none = {0};
    return screens[current]->update(&none, now) & ~SCREEN_FOCUS_TABS;
  }
  if (input_pressed(in, KEY_BACK) && current != 0) {
    switch_to(0);
    return SCREEN_REDRAW_ALL;
  }
  int r = screens[current]->update(in, now);
  if (r & SCREEN_FOCUS_TABS) {
    tabs_focused = true;
    r |= SCREEN_REDRAW_ALL;
  }
  return r;
}

int app_main(void) {
  platform_init();
  game_init();
  input_t in = {0};
  /* Ignore the keys still held from launching the app. */
  in.held = platform_keys();
  int redraw = SCREEN_REDRAW_ALL;
  for (;;) {
    uint32_t now = platform_millis();
    input_update(&in, platform_keys(), now);
    if (input_pressed(&in, KEY_HOME)) break;
    if (on_title) {
      bool blink = (now / 500) % 2 == 0;
      if (blink != blink_on) {
        blink_on = blink;
        gfx_render(draw_title, 185, 210);
      }
      if (input_pressed(&in, KEY_OK) || input_pressed(&in, KEY_EXE)) {
        on_title = false;
        switch_to(0);
        gfx_render(draw_game, 0, 240);
      } else if (redraw) {
        gfx_render(draw_title, 0, 240);
      }
    } else {
      redraw |= update_game(&in, now);
      if (redraw & SCREEN_REDRAW_ALL) gfx_render(draw_game, 0, 240);
      else if (redraw & SCREEN_REDRAW_CONTENT) gfx_render(draw_game, CONTENT_Y, CONTENT_Y + CONTENT_H);
    }
    redraw = 0;
    if (!platform_end_frame()) break;
  }
  return 0;
}
