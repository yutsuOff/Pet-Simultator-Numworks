#ifndef SCREENS_H
#define SCREENS_H

#include <stdbool.h>
#include <stdint.h>
#include "../input/input.h"
#include "../gfx/gfx.h"

/* Bits returned by screen_t.update */
enum {
  SCREEN_REDRAW_CONTENT = 1, /* only the content area changed */
  SCREEN_REDRAW_ALL = 2,     /* bars changed too (currencies, zone...) */
  SCREEN_FOCUS_TABS = 4,     /* the player moved down past the last row */
  SCREEN_USED_BACK = 8,      /* the screen handled the Back key (closing a popup) */
};

typedef struct {
  const char *label;
  const sprite_t *icon;
  void (*enter)(void);
  int (*update)(const input_t *in, uint32_t now);
  void (*draw)(void);
} screen_t;

extern const screen_t screen_farm;
extern const screen_t screen_pets;
extern const screen_t screen_eggs;
extern const screen_t screen_zones;
extern const screen_t screen_upgrades;
extern const screen_t screen_index;
extern const screen_t screen_quests;
extern const screen_t screen_items;

/* Machines (machines.c), opened from the Farm screen over its content. */
void machine_open(int machine);
bool machine_is_open(void);
void machine_close(void);
int machine_update(const input_t *in, uint32_t now);
void machine_draw(uint32_t now);
void machine_draw_card(int machine, int x, int y, int w, int h, bool selected);

#endif
