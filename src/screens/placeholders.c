/* Screens not built yet: they show what is planned and when (see docs/CONCEPTION.md). */
#include "screens.h"
#include "../generated/sprites.h"
#include "../platform/platform.h"
#include "../ui/ui.h"

typedef struct {
  const char *title;
  const char *stage;
  const char *lines[5];
} planned_t;

static const planned_t planned[] = {
  {"Quêtes", "Étape M4", {"Quêtes courtes renouvelées", "Succès à paliers (bronze → diamant)",
   "Récompenses en coins, diamants et bonus", 0}},
};

static int planned_index;

static int update(const input_t *in, uint32_t now) {
  (void)now;
  return input_pressed(in, KEY_DOWN) ? SCREEN_FOCUS_TABS : 0;
}

static void draw(void) {
  const planned_t *p = &planned[planned_index];
  int y = CONTENT_Y + 10;
  ui_panel(10, y, 300, 174);
  gfx_text(&font_big, 24, y + 10, p->title, COL_TEXT);
  int sw = gfx_text_width(&font_small, p->stage) + 10;
  ui_pill(296 - sw, y + 14, p->stage, COL_BLUE, COL_WHITE);
  gfx_fill_rect(24, y + 38, 272, 1, RGB(200, 210, 230));
  gfx_text(&font_medium, 24, y + 46, "Bientôt disponible :", COL_TEXT_DIM);
  for (int i = 0; i < 5 && p->lines[i]; i++) {
    int ly = y + 68 + i * 19;
    gfx_round_rect(26, ly + 4, 6, 6, 3, COL_BLUE);
    gfx_text(&font_small, 40, ly, p->lines[i], COL_TEXT);
  }
}

#define PLACEHOLDER(var, idx, label, icon)                 \
  static void enter_##var(void) { planned_index = idx; }   \
  static void draw_##var(void) { planned_index = idx; draw(); } \
  const screen_t var = {label, icon, enter_##var, update, draw_##var};

PLACEHOLDER(screen_quests, 0, "Quêtes", &spr_tab_quests)
