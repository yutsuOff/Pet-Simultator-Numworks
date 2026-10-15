/* Machines of the Farm screen (PS99): one stands in the last slot of some zones and opens
 * over the Farm content (the pets keep farming behind it).
 *   Golden / Rainbow: 10 identical pets -> 1 of the next level        (zones 7, 14)
 *   Potions / Enchants: 3 items of a tier -> 1 of the next tier        (zones 38, 55)
 *   Fusion: 3 to 12 pets -> 1 stronger pet                             (zone 50)
 *   Daycare: pets stay 10 min, then come back with rewards             (zone 58) */
#include "screens.h"
#include "../game/game.h"
#include "../game/numfmt.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

#define COLS 6
#define PANEL_X 8
#define PANEL_Y (CONTENT_Y + 4)
#define PANEL_W 304
#define PANEL_H (CONTENT_H - 8)
#define FOOTER_Y (PANEL_Y + PANEL_H - 16)

typedef struct {
  const char *card, *title, *help;
  const sprite_t *sprite;
  color_t top, bottom;
} machine_info_t;

static const machine_info_t info[MACHINE_COUNT] = {
  [MACHINE_GOLDEN] = {"Golden", "Machine Golden", "10 pets identiques → 1 golden", &spr_machine_gold,
                      RGB(255, 240, 180), RGB(240, 200, 90)},
  [MACHINE_RAINBOW] = {"Rainbow", "Machine Rainbow", "10 golden identiques → 1 rainbow", &spr_machine_rainbow,
                       RGB(235, 215, 255), RGB(190, 160, 240)},
  [MACHINE_POTIONS] = {"Potions", "Améliore les potions", "3 potions → 1 du niveau suivant", &spr_machine_potions,
                       RGB(255, 220, 245), RGB(225, 150, 230)},
  [MACHINE_FUSE] = {"Fusion", "Machine de fusion", "3 à 12 pets → 1 pet plus fort", &spr_machine_fuse,
                    RGB(210, 240, 255), RGB(110, 190, 240)},
  [MACHINE_ENCHANTS] = {"Enchants", "Améliore les enchants", "3 enchants → 1 du niveau suivant", &spr_machine_enchants,
                        RGB(220, 225, 255), RGB(130, 140, 240)},
  [MACHINE_DAYCARE] = {"Daycare", "Daycare", "Tes pets reviennent avec des cadeaux", &spr_machine_daycare,
                       RGB(240, 255, 215), RGB(170, 225, 130)},
};

static int open_machine = NO_MACHINE;
static int sel, scroll;
static bool on_slots;               /* daycare: the enrolled pets row is selected */
static int slot_sel;
static uint16_t list[PET_COUNT * VARIANT_COUNT]; /* pet | variant << 8 */
static int count;
static team_slot_t fuse_in[FUSE_MAX];
static int fuse_n;
static bool has_result;
static hatch_t result;
static uint32_t clock_ms;
static char text[64];

static int entry_pet(int i) { return list[i] & 0xFF; }
static int entry_variant(int i) { return list[i] >> 8; }

/* Rows of the pet / item grid of each machine. */
static int grid_rows(void) {
  return open_machine == MACHINE_GOLDEN || open_machine == MACHINE_RAINBOW ? 3 : 2;
}

static int item_base(void) { return open_machine == MACHINE_ENCHANTS ? ITEM_ENCHANT : 0; }

static int fuse_chosen(int pet, int variant) {
  int n = 0;
  for (int i = 0; i < fuse_n; i++) n += fuse_in[i].pet == pet && fuse_in[i].variant == variant;
  return n;
}

/* Copies that can still be used (not equipped, not already in the fusion). */
static int pet_left(int i) {
  int p = entry_pet(i), v = entry_variant(i);
  return game_machine_available(p, v) - (open_machine == MACHINE_FUSE ? fuse_chosen(p, v) : 0);
}

static void build(void) {
  count = 0;
  if (open_machine == MACHINE_POTIONS || open_machine == MACHINE_ENCHANTS) {
    /* tiers I and II of every type */
    int types = open_machine == MACHINE_POTIONS ? POTION_TYPE_COUNT : ENCHANT_TYPE_COUNT;
    for (int t = 0; t < types; t++) list[count++] = (uint16_t)(t * 3), list[count++] = (uint16_t)(t * 3 + 1);
  } else if (open_machine == MACHINE_GOLDEN || open_machine == MACHINE_RAINBOW) {
    int level = open_machine == MACHINE_GOLDEN ? VAR_NORMAL : VAR_GOLDEN;
    for (int shiny = 0; shiny < 2; shiny++) {
      for (int p = 0; p < HUGE_FIRST; p++) {
        int v = level + shiny * VAR_SHINY;
        if (game.owned[p][v]) list[count++] = (uint16_t)(p | v << 8);
      }
    }
  } else {
    /* fusion and daycare: every pet with a free copy (shiny ones last) */
    for (int shiny = 0; shiny < 2; shiny++) {
      for (int p = 0; p < HUGE_FIRST; p++) {
        for (int level = 0; level < 3; level++) {
          int v = level + shiny * VAR_SHINY;
          if (game_machine_available(p, v) > 0) list[count++] = (uint16_t)(p | v << 8);
        }
      }
    }
  }
  if (sel >= count) sel = count ? count - 1 : 0;
}

void machine_open(int machine) {
  open_machine = machine;
  sel = scroll = slot_sel = 0;
  fuse_n = 0;
  has_result = false;
  on_slots = machine == MACHINE_DAYCARE;
  build();
}

bool machine_is_open(void) { return open_machine != NO_MACHINE; }

void machine_close(void) { open_machine = NO_MACHINE; }

static void say(const char *a, const char *b, uint32_t now) {
  char *q = text;
  while (*a && q < text + sizeof(text) - 1) *q++ = *a++;
  while (*b && q < text + sizeof(text) - 1) *q++ = *b++;
  *q = 0;
  ui_message(text, now);
}

/* --- Actions (OK) --- */

static int press_variant(uint32_t now) {
  int p = entry_pet(sel), v = entry_variant(sel);
  if (!game_machine_make(p, v)) {
    ui_message("Il faut 10 exemplaires (non équipés)", now);
    return 0;
  }
  char name[40];
  say(ui_pet_name(p, v + 1, name), " fabriqué !", now);
  build();
  return 0;
}

static int press_item(uint32_t now) {
  int i = list[sel];
  bool ok = open_machine == MACHINE_POTIONS ? game_upgrade_potion(i) : game_upgrade_enchant(i);
  if (!ok) {
    ui_message(open_machine == MACHINE_POTIONS ? "Il faut 3 potions identiques"
                                               : "Il faut 3 enchants identiques (non équipés)", now);
    return 0;
  }
  char name[40];
  say(ui_item_name(item_base() + i + 1, name), " fabriqué !", now);
  return 0;
}

static int press_fuse_add(uint32_t now) {
  if (has_result) has_result = false;
  if (fuse_n >= FUSE_MAX) {
    ui_message("12 pets maximum", now);
  } else if (pet_left(sel) <= 0) {
    ui_message("Plus d'exemplaire libre de ce pet", now);
  } else {
    fuse_in[fuse_n++] = (team_slot_t){(uint8_t)entry_pet(sel), (uint8_t)entry_variant(sel)};
  }
  return 0;
}

static int fuse(uint32_t now) {
  if (fuse_n < FUSE_MIN) {
    ui_message("Mets au moins 3 pets (OK)", now);
    return 0;
  }
  if (!game_fuse(fuse_in, fuse_n, &result)) return 0;
  has_result = true;
  fuse_n = 0;
  char name[40];
  say("Fusion : ", ui_pet_name(result.pet, result.variant, name), now);
  build();
  return SCREEN_REDRAW_ALL;
}

static int press_daycare(uint32_t now) {
  if (on_slots) {
    daycare_reward_t r;
    const team_slot_t *t = &game.daycare[slot_sel];
    if (t->pet == NO_PET) {
      ui_message("Choisis un pet en bas pour l'inscrire", now);
      return 0;
    }
    if (!game_daycare_collect(slot_sel, &r)) {
      ui_message("Pas encore fini !", now);
      return 0;
    }
    char amount[24], *q = text;
    for (const char *s = "+"; *s;) *q++ = *s++;
    for (const char *s = numfmt(r.coins, amount); *s;) *q++ = *s++;
    for (const char *s = " coins, +"; *s;) *q++ = *s++;
    for (const char *s = numfmt(r.diamonds, amount); *s;) *q++ = *s++;
    for (const char *s = r.item >= 0 ? " diamants + objet !" : " diamants"; *s;) *q++ = *s++;
    *q = 0;
    ui_message(text, now);
    build();
    return SCREEN_REDRAW_ALL;
  }
  if (!count) return 0;
  int p = entry_pet(sel), v = entry_variant(sel);
  if (!game_daycare_enroll(p, v)) {
    ui_message("Daycare plein (5 pets)", now);
    return 0;
  }
  char name[40];
  say(ui_pet_name(p, v, name), " inscrit !", now);
  build();
  return 0;
}

/* --- Update --- */

int machine_update(const input_t *in, uint32_t now) {
  clock_ms = now;
  int r = (now / 120 != (now - 33) / 120) ? SCREEN_REDRAW_CONTENT : 0; /* sparkles, timers */
  if (input_pressed(in, KEY_BACK)) {
    open_machine = NO_MACHINE;
    return SCREEN_REDRAW_CONTENT | SCREEN_USED_BACK;
  }
  bool ok = input_pressed(in, KEY_OK), exe = input_pressed(in, KEY_EXE);
  if (on_slots) {
    if (input_pressed(in, KEY_LEFT) && slot_sel > 0) slot_sel--;
    if (input_pressed(in, KEY_RIGHT) && slot_sel < DAYCARE_SLOTS - 1) slot_sel++;
    if (input_pressed(in, KEY_DOWN) && count) on_slots = false, sel = slot_sel < count ? slot_sel : count - 1;
  } else {
    if (input_pressed(in, KEY_LEFT) && sel > 0) sel--;
    if (input_pressed(in, KEY_RIGHT) && sel < count - 1) sel++;
    if (input_pressed(in, KEY_UP)) {
      if (sel >= COLS) sel -= COLS;
      else if (open_machine == MACHINE_DAYCARE) on_slots = true, slot_sel = sel < DAYCARE_SLOTS ? sel : DAYCARE_SLOTS - 1;
    }
    if (input_pressed(in, KEY_DOWN) && sel + COLS < count) sel += COLS;
  }
  if (in->pressed) r |= SCREEN_REDRAW_CONTENT;
  if (open_machine == MACHINE_FUSE) {
    if (input_pressed(in, KEY_BACKSPACE) && fuse_n > 0) fuse_n--;
    if (exe) r |= fuse(now);
    else if (ok && count) r |= press_fuse_add(now);
  } else if ((ok || exe) && (count || on_slots)) {
    if (open_machine == MACHINE_DAYCARE) r |= press_daycare(now);
    else if (open_machine == MACHINE_POTIONS || open_machine == MACHINE_ENCHANTS) r |= press_item(now);
    else r |= press_variant(now);
  }
  int row = sel / COLS;
  if (row < scroll) scroll = row;
  if (row >= scroll + grid_rows()) scroll = row - grid_rows() + 1;
  return r;
}

/* --- Drawing --- */

void machine_draw_card(int machine, int x, int y, int w, int h, bool selected) {
  const machine_info_t *m = &info[machine];
  ui_card(x, y, w, h, selected, m->top, m->bottom);
  if (selected) y -= 2;
  gfx_sprite(m->sprite, x + (w - m->sprite->width) / 2, y + 4);
  const char *label = m->card;
  color_t c = COL_TEXT;
  if (machine == MACHINE_DAYCARE) {
    for (int i = 0; i < DAYCARE_SLOTS; i++) {
      if (game.daycare[i].pet != NO_PET && !game.daycare_ms[i]) label = "Prêt !", c = COL_GREEN_DARK;
    }
  }
  gfx_text_aligned(&font_small, x, y + 45, w, ALIGN_CENTER, label, c);
}

/* Small card of a grid; returns the y offset of its content (lifted when selected). */
static int cell(int x, int y, int w, int h, bool selected, bool ready) {
  ui_card(x, y, w, h, selected, ready ? RGB(220, 255, 220) : COL_PANEL_TOP, ready ? RGB(150, 220, 150) : COL_PANEL_BOT);
  return selected ? -2 : 0;
}

static void count_text(int x, int y, int w, int have, int need, bool ready) {
  char n[16], *q = n;
  numfmt(have, q);
  if (need) {
    while (*q) q++;
    *q++ = '/';
    numfmt(need, q);
  }
  gfx_text_aligned(&font_small, x, y, w, ALIGN_CENTER, n, ready ? COL_GREEN_DARK : COL_TEXT_DIM);
}

static void scrollbar(int y, int rows, int step) {
  int total_rows = (count + COLS - 1) / COLS;
  if (total_rows <= rows) return;
  int bar_h = rows * step - 4, th = bar_h * rows / total_rows;
  gfx_round_rect(PANEL_X + PANEL_W - 9, y, 4, bar_h, 2, COL_PANEL_BOT);
  gfx_round_rect(PANEL_X + PANEL_W - 9, y + (bar_h - th) * scroll / (total_rows - rows), 4, th, 2, COL_BAR_LIGHT);
}

/* Grid of pets: count shown is the free copies (and /need if given). */
static void pet_grid(int y, int need, bool sel_visible) {
  int rows = grid_rows();
  for (int i = scroll * COLS; i < count && i < (scroll + rows) * COLS; i++) {
    int p = entry_pet(i), v = entry_variant(i), left = pet_left(i);
    int cx = PANEL_X + 8 + (i % COLS) * 48, cy = y + (i / COLS - scroll) * 40;
    bool ready = need ? left >= need : false;
    int dy = cell(cx, cy, 44, 36, sel_visible && i == sel, ready);
    ui_pet(p, v, cx + 10, cy + 2 + dy, PET_HALF, clock_ms);
    count_text(cx, cy + 24 + dy, 44, left, need, ready);
  }
  scrollbar(y, rows, 40);
}

static void footer(const char *a, const char *b) {
  char line[72], *q = line;
  while (*a && q < line + 40) *q++ = *a++;
  while (*b && q < line + sizeof(line) - 1) *q++ = *b++;
  *q = 0;
  gfx_text_aligned(&font_small, PANEL_X, FOOTER_Y, PANEL_W, ALIGN_CENTER, line, COL_BLUE_DARK);
}

static void empty(int y, const char *s) {
  gfx_text_aligned(&font_medium, PANEL_X, y, PANEL_W, ALIGN_CENTER, s, COL_TEXT_DIM);
}

static void draw_variant(void) {
  bool gold = open_machine == MACHINE_GOLDEN;
  if (!count) empty(PANEL_Y + 90, gold ? "Aucun pet à transformer" : "Aucun pet golden à transformer");
  pet_grid(PANEL_Y + 52, MACHINE_INPUT, true);
  if (count) {
    char name[48];
    footer("OK : fabriquer ", ui_pet_name(entry_pet(sel), entry_variant(sel) + 1, name));
  }
}

static void draw_items(void) {
  bool enchant = open_machine == MACHINE_ENCHANTS;
  for (int i = 0; i < count; i++) {
    int item = list[i];
    int have = enchant ? game_enchant_free(item) : game.potions_owned[item];
    int cx = PANEL_X + 6 + (i % COLS) * 49, cy = PANEL_Y + 52 + (i / COLS) * 54;
    bool ready = have >= ITEM_UPGRADE_INPUT;
    int dy = cell(cx, cy, 46, 50, i == sel, ready);
    gfx_sprite(ui_item_sprite(item_base() + item), cx + 7, cy + 3 + dy);
    count_text(cx, cy + 36 + dy, 46, have, ITEM_UPGRADE_INPUT, ready);
  }
  char from[40], to[40], line[72], *q = line;
  ui_item_name(item_base() + list[sel], from);
  ui_item_name(item_base() + list[sel] + 1, to);
  for (const char *s = "3 "; *s;) *q++ = *s++;
  for (const char *s = from; *s;) *q++ = *s++;
  for (const char *s = " → "; *s;) *q++ = *s++;
  for (const char *s = to; *s;) *q++ = *s++;
  *q = 0;
  gfx_text_aligned(&font_small, PANEL_X, FOOTER_Y - 16, PANEL_W, ALIGN_CENTER, line, COL_TEXT);
  footer("OK : améliorer", "");
}

static void draw_fuse(void) {
  if (!count && !fuse_n) empty(PANEL_Y + 70, "Aucun pet libre à fusionner");
  pet_grid(PANEL_Y + 50, 0, true);
  /* the pets put in, or the last result */
  int y = PANEL_Y + 132;
  gfx_round_rect(PANEL_X + 6, y, PANEL_W - 12, 32, 8, RGB(215, 235, 250));
  if (has_result) {
    char name[48];
    ui_pet(result.pet, result.variant, PANEL_X + 10, y + 4, PET_HALF, clock_ms);
    int x = PANEL_X + 40 + gfx_text(&font_medium, PANEL_X + 40, y + 9, ui_pet_name(result.pet, result.variant, name),
                                    COL_TEXT);
    if (result.is_new) ui_pill(x + 6, y + 9, "Nouveau !", COL_GOLD_DARK, COL_WHITE);
  } else if (!fuse_n) {
    gfx_text_aligned(&font_small, PANEL_X, y + 10, PANEL_W, ALIGN_CENTER, "OK : ajoute le pet choisi à la fusion",
                     COL_TEXT_DIM);
  } else {
    for (int i = 0; i < fuse_n; i++) ui_pet(fuse_in[i].pet, fuse_in[i].variant, PANEL_X + 8 + i * 20, y + 4, PET_HALF, clock_ms);
    char n[8], *q = n;
    numfmt(fuse_n, q);
    while (*q) q++;
    for (const char *s = "/12"; (*q++ = *s++);) {}
    gfx_text_aligned(&font_small, PANEL_X, y + 10, PANEL_W - 14, ALIGN_RIGHT, n,
                     fuse_n >= FUSE_MIN ? COL_GREEN_DARK : COL_TEXT_DIM);
  }
  footer(fuse_n >= FUSE_MIN ? "EXE : fusionner  ·  " : "OK : +1  ·  ", "Effacer : -1");
}

static void draw_daycare(void) {
  /* the 5 places */
  int y = PANEL_Y + 48;
  for (int i = 0; i < DAYCARE_SLOTS; i++) {
    const team_slot_t *t = &game.daycare[i];
    int cx = PANEL_X + 8 + i * 58;
    bool ready = t->pet != NO_PET && !game.daycare_ms[i];
    int dy = cell(cx, y, 54, 38, on_slots && i == slot_sel, ready);
    if (t->pet == NO_PET) {
      gfx_text_aligned(&font_small, cx, y + 13 + dy, 54, ALIGN_CENTER, "Libre", COL_TEXT_DIM);
      continue;
    }
    ui_pet(t->pet, t->variant, cx + 2, y + 6 + dy, PET_HALF, clock_ms);
    char time[8];
    uint32_t s = (game.daycare_ms[i] + 999) / 1000;
    if (ready) {
      gfx_text(&font_small, cx + 27, y + 13 + dy, "Prêt", COL_GREEN_DARK);
    } else {
      /* "10m" then "45s" */
      numfmt(s >= 60 ? (s + 59) / 60 : s, time);
      int n = 0;
      while (time[n]) n++;
      time[n++] = s >= 60 ? 'm' : 's', time[n] = 0;
      gfx_text(&font_small, cx + 28, y + 13 + dy, time, COL_TEXT_DIM);
    }
  }
  if (!count) empty(PANEL_Y + 120, "Aucun pet libre");
  pet_grid(PANEL_Y + 92, 0, !on_slots);
  if (on_slots) {
    const team_slot_t *t = &game.daycare[slot_sel];
    footer(t->pet == NO_PET ? "Bas : choisir un pet à inscrire" : game.daycare_ms[slot_sel] ? "Il revient bientôt..."
                                                                                             : "OK : récupérer",
           "");
  } else if (count) {
    char coins[24];
    numfmt(game_daycare_coins(entry_pet(sel), entry_variant(sel)) * game_coin_bonus(), coins);
    footer("OK : inscrire (10 min) · ~", coins);
  }
}

void machine_draw(uint32_t now) {
  clock_ms = now;
  const machine_info_t *m = &info[open_machine];
  ui_panel(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  gfx_sprite(m->sprite, PANEL_X + 8, PANEL_Y + 5);
  gfx_text(&font_big, PANEL_X + 58, PANEL_Y + 6, m->title, COL_TEXT);
  gfx_text(&font_small, PANEL_X + 58, PANEL_Y + 28, m->help, COL_TEXT_DIM);
  switch (open_machine) {
    case MACHINE_POTIONS:
    case MACHINE_ENCHANTS: draw_items(); break;
    case MACHINE_FUSE: draw_fuse(); break;
    case MACHINE_DAYCARE: draw_daycare(); break;
    default: draw_variant(); break;
  }
}
