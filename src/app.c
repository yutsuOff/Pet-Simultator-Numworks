/* Main loop: title screen, tab navigation and screen dispatch. */
#include "game/game.h"
#include "generated/sprites.h"
#include "input/input.h"
#include "platform/platform.h"
#include "save/save.h"
#include "screens/admin.h"
#include "screens/screens.h"
#include "ui/ui.h"

#define VERSION "v0.5 · M4"

static const screen_t *const screens[] = {
  &screen_farm, &screen_pets, &screen_eggs, &screen_zones,
  &screen_upgrades, &screen_index, &screen_quests, &screen_items,
};
#define SCREEN_COUNT (int)(sizeof(screens) / sizeof(screens[0]))

static int current;
static bool tabs_focused;
static bool on_title = true;
static bool blink_on = true;
static bool save_details; /* title screen: Toolbox shows the save diagnostic */
static storage_result_t reported_result = STORAGE_OK;

/* --- Save status --- */

static const char *save_status_text(color_t *c) {
  switch (save_status()) {
    case SAVE_LOADED: *c = RGB(200, 255, 200); return "Partie chargée · sauvegarde auto";
    case SAVE_NEW_GAME: *c = COL_WHITE; return "Nouvelle partie · sauvegarde auto";
    case SAVE_UNREADABLE: *c = RGB(255, 210, 120); return "petsim.py illisible : pas de sauvegarde";
    case SAVE_TOO_NEW: *c = RGB(255, 210, 120); return "Sauvegarde d'une version plus récente";
    default: *c = RGB(255, 210, 120); return "Sauvegarde indisponible";
  }
}

static const char *storage_result_text(storage_result_t r) {
  switch (r) {
    case STORAGE_OK: return "OK";
    case STORAGE_FULL: return "stockage plein";
    case STORAGE_MOVED: return "petsim.py modifié";
    default: return "stockage illisible";
  }
}

static char *put_text(char *q, const char *s) {
  while (*s) *q++ = *s++;
  return q;
}

static char *put_hex(char *q, uint32_t v) {
  q = put_text(q, "0x");
  for (int i = 28; i >= 0; i -= 4) *q++ = "0123456789ABCDEF"[(v >> i) & 15];
  return q;
}

static char *put_dec(char *q, long v) {
  char tmp[12];
  int n = 0;
  do tmp[n++] = (char)('0' + v % 10), v /= 10;
  while (v);
  while (n) *q++ = tmp[--n];
  return q;
}

static void draw_save_details(void) {
  ui_panel(14, 24, 292, 192);
  gfx_text(&font_big, 28, 34, "Sauvegarde", COL_TEXT);
  char line[64], *q;
  int y = 64;
  uint32_t address;
  long used, size;
  if (save_storage_info(&address, &used, &size)) {
    q = put_hex(put_text(line, "Stockage Epsilon : "), address);
    *q = 0;
    gfx_text(&font_small, 28, y, line, COL_TEXT);
    q = put_text(put_dec(put_text(line, "Utilisé : "), used), " / ");
    q = put_text(put_dec(q, size), " octets");
    *q = 0;
    gfx_text(&font_small, 28, y + 16, line, COL_TEXT);
  } else {
    gfx_text(&font_small, 28, y, "Stockage Epsilon introuvable :", COL_RED);
    gfx_text(&font_small, 28, y + 16, "la partie ne sera pas enregistrée.", COL_TEXT);
  }
  color_t c;
  gfx_text(&font_small, 28, y + 32, save_status_text(&c), COL_TEXT);
  q = put_text(put_text(line, "Dernière écriture : "), storage_result_text(save_last_result()));
  *q = 0;
  gfx_text(&font_small, 28, y + 48, line, COL_TEXT);
  gfx_fill_rect(28, y + 68, 264, 1, RGB(200, 210, 230));
  gfx_text(&font_small, 28, y + 74, "Fichier : petsim.py (visible dans l'app Python).", COL_TEXT_DIM);
  gfx_text(&font_small, 28, y + 90, "Le supprimer recommence une nouvelle partie.", COL_TEXT_DIM);
  gfx_text(&font_small, 28, y + 106, "Test : joue, quitte avec Home, relance l'app.", COL_TEXT_DIM);
  gfx_text(&font_small, 28, y + 122, "Retour : fermer", COL_BLUE_DARK);
}

/* --- Quests: finished quests give stars, stars give the rank rewards --- */

static char quest_text_buf[48];

static int check_quests(uint32_t now) {
  bool ranked_up;
  int stars = quests_update(&ranked_up);
  if (!stars) return 0;
  char *q = quest_text_buf;
  if (ranked_up) {
    for (const char *s = "Nouveau rang : "; *s;) *q++ = *s++;
    for (const char *s = ranks[game.rank].name; *s;) *q++ = *s++;
    *q++ = ' ';
    *q++ = '!';
  } else {
    for (const char *s = "Quête terminée ! +"; *s;) *q++ = *s++;
    *q++ = (char)('0' + stars);
    for (const char *s = " ★"; *s;) *q++ = *s++;
  }
  *q = 0;
  ui_message(quest_text_buf, now);
  return SCREEN_REDRAW_ALL;
}

/* --- Title screen --- */

static void draw_title(void) {
  gfx_vgradient(0, 0, 320, 240, RGB(70, 150, 255), RGB(130, 220, 140));
  gfx_sprite(&spr_logo, (320 - spr_logo.width) / 2, 6);
  /* two pets of the Cracked Egg on each side of the logo */
  gfx_sprite(pets[0].sprite, 8, 120);
  gfx_sprite(pets[3].sprite, 264, 120);
  if (blink_on) {
    gfx_text_aligned(&font_medium, 1, 187, 320, ALIGN_CENTER, "Appuie sur OK pour jouer", COL_SHADOW);
    gfx_text_aligned(&font_medium, 0, 186, 320, ALIGN_CENTER, "Appuie sur OK pour jouer", COL_WHITE);
  }
  color_t c;
  const char *status = save_status_text(&c);
  gfx_text_aligned(&font_small, 1, 207, 320, ALIGN_CENTER, status, COL_SHADOW);
  gfx_text_aligned(&font_small, 0, 206, 320, ALIGN_CENTER, status, c);
  gfx_text_shadow(&font_small, 6, 225, VERSION, COL_WHITE, gfx_mix(COL_SHADOW, COL_GREEN_DARK, 120));
  int tw = gfx_text_width(&font_small, "Toolbox : sauvegarde");
  gfx_text_shadow(&font_small, 314 - tw, 225, "Toolbox : sauvegarde", COL_WHITE, gfx_mix(COL_SHADOW, COL_GREEN_DARK, 120));
  if (save_details) draw_save_details();
}

/* --- Game screens --- */

static void draw_game(void) {
  const zone_t *z = &zones[game.zone];
  ui_background(z->sky_top, z->sky_bottom);
  if (admin_is_open()) admin_draw();
  else screens[current]->draw();
  ui_message_draw();
  ui_top_bar(z->name, z->accent, game.coins, game.diamonds);
  /* active potions, after the zone badge */
  int bx = gfx_text_width(&font_medium, z->name) + 22;
  for (int t = 0; t < POTION_TYPE_COUNT; t++) {
    if (!game.boost_tier[t]) continue;
    gfx_sprite_half(potions[t * 3 + game.boost_tier[t] - 1].sprite, bx, 3);
    bx += 17;
  }
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
  static const int keys[] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8};
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
  int r = screens[current]->update(in, now);
  if (input_pressed(in, KEY_BACK) && !(r & SCREEN_USED_BACK) && current != 0) {
    switch_to(0);
    return SCREEN_REDRAW_ALL;
  }
  if (r & SCREEN_FOCUS_TABS) {
    tabs_focused = true;
    r |= SCREEN_REDRAW_ALL;
  }
  return r;
}

int app_main(void) {
  platform_init();
  game_random = platform_random;
  game_init();
  save_init();
  game_claim_index_rewards(); /* eggs completed before the index rewards existed */
  quests_refill();
  if (platform_debug_rich()) { /* screenshots of a later game */
    game.coins = 2e7, game.diamonds = 600;
    for (int i = 0; i < POTION_COUNT; i += 2) game_give_potion(i, 1 + i % 3);
    for (int i = 1; i < ENCHANT_COUNT; i += 3) game_give_enchant(i, 1);
    game.owned[HUGE_FIRST][VAR_NORMAL] = 1, game.owned[2][VAR_SHINY + VAR_GOLDEN] = 1, game.owned[1][VAR_RAINBOW] = 1;
    game.discovered[HUGE_FIRST / 8] |= (uint8_t)(1 << (HUGE_FIRST % 8));
  }
  game_debug_next_huge = platform_debug_huge();
  input_t in = {0};
  /* Ignore the keys still held from launching the app. */
  in.held = platform_keys();
  int redraw = SCREEN_REDRAW_ALL;
  uint32_t last = platform_millis();
  for (;;) {
    uint32_t now = platform_millis();
    uint32_t elapsed = now - last;
    last = now;
    input_update(&in, platform_keys(), now);
    if (admin_check_code(&in)) { /* the final OK opens the admin menu, nothing else */
      if (on_title) on_title = false, save_details = false, switch_to(0);
      gfx_render(draw_game, 0, 240);
      redraw = 0;
      if (!platform_end_frame()) break;
      continue;
    }
    if (admin_is_open() && !on_title) {
      redraw |= admin_update(&in, now);
      if (ui_message_expired(now)) redraw |= SCREEN_REDRAW_CONTENT;
      if (redraw & SCREEN_REDRAW_ALL) gfx_render(draw_game, 0, 240);
      else if (redraw & SCREEN_REDRAW_CONTENT) gfx_render(draw_game, CONTENT_Y, CONTENT_Y + CONTENT_H);
      redraw = 0;
      save_tick(now);
      if (!platform_end_frame()) break;
      continue;
    }
    if (input_pressed(&in, KEY_HOME)) break;
    if (on_title) {
      bool blink = (now / 500) % 2 == 0;
      if (blink != blink_on && !save_details) {
        blink_on = blink;
        gfx_render(draw_title, 180, 206);
      }
      if (input_pressed(&in, KEY_TOOLBOX) || (save_details && input_pressed(&in, KEY_BACK))) {
        save_details = !save_details;
        gfx_render(draw_title, 0, 240);
      } else if (input_pressed(&in, KEY_OK) || input_pressed(&in, KEY_EXE)) {
        save_details = false;
        on_title = false;
        switch_to(0);
        gfx_render(draw_game, 0, 240);
      } else if (redraw) {
        gfx_render(draw_title, 0, 240);
      }
    } else {
      redraw |= update_game(&in, now);
      if (ui_message_expired(now)) redraw |= SCREEN_REDRAW_CONTENT;
      /* potions run only while playing */
      int active = 0;
      for (int t = 0; t < POTION_TYPE_COUNT; t++) active += game.boost_tier[t] != 0;
      game_tick_boosts(elapsed > 1000 ? 1000 : elapsed);
      for (int t = 0; t < POTION_TYPE_COUNT; t++) active -= game.boost_tier[t] != 0;
      if (active) redraw |= SCREEN_REDRAW_ALL; /* a potion ended */
      redraw |= check_quests(now);
      save_tick(now);
      storage_result_t r = save_last_result();
      if (r != reported_result) { /* tell the player once when saving starts failing */
        reported_result = r;
        if (r != STORAGE_OK) {
          ui_message(r == STORAGE_FULL ? "Sauvegarde : stockage plein" : "Sauvegarde impossible", now);
          redraw |= SCREEN_REDRAW_CONTENT;
        }
      }
      if (redraw & SCREEN_REDRAW_ALL) gfx_render(draw_game, 0, 240);
      else if (redraw & SCREEN_REDRAW_CONTENT) gfx_render(draw_game, CONTENT_Y, CONTENT_Y + CONTENT_H);
    }
    redraw = 0;
    if (!platform_end_frame()) break;
  }
  if (!on_title) save_now();
  return 0;
}
