#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "../gfx/gfx.h"
#include "theme.h"

typedef struct {
  const char *label;
  const sprite_t *icon;
} tab_t;

void ui_background(color_t top, color_t bottom);
void ui_top_bar(const char *zone_name, color_t zone_color, double coins, double diamonds);
void ui_tab_bar(const tab_t *tabs, int count, int current, bool focused);

/* White rounded panel with a thick dark outline. */
void ui_panel(int x, int y, int w, int h);
/* Card used in grids. A selected card gets a golden frame and is lifted by 2 px. */
void ui_card(int x, int y, int w, int h, bool selected, color_t top, color_t bottom);
void ui_button(int x, int y, int w, int h, const char *label, bool selected, color_t color);
/* Small rounded label ("pill"), returns its width. */
int ui_pill(int x, int y, const char *text, color_t bg, color_t fg);

#endif
