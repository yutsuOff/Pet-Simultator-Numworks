#ifndef GFX_H
#define GFX_H

/* Band renderer: the screen is drawn in horizontal bands of BAND_H lines.
 * Each band is composed in RAM (so layers, transparency and text over gradients
 * are free of flicker) and then sent to the display in a single transfer.
 * All drawing primitives are clipped to the current band. */

#include <stdbool.h>
#include <stdint.h>
#include "font.h"

typedef uint16_t color_t;

#define RGB(r, g, b) ((color_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

typedef struct {
  uint8_t width, height;
  uint8_t palette_size;
  const uint16_t *palette;      /* RGB565 */
  const uint8_t *palette_alpha; /* 0..15 */
  const uint8_t *pixels;        /* palette indices, row major */
} sprite_t;

typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } align_t;

/* Renders the screen lines [y0, y1) by calling draw() once per band. */
void gfx_render(void (*draw)(void), int y0, int y1);

void gfx_fill_rect(int x, int y, int w, int h, color_t c);
void gfx_blend_rect(int x, int y, int w, int h, color_t c, int alpha /* 0..255 */);
void gfx_vgradient(int x, int y, int w, int h, color_t top, color_t bottom);
void gfx_round_rect(int x, int y, int w, int h, int r, color_t c);
void gfx_round_gradient(int x, int y, int w, int h, int r, color_t top, color_t bottom);
void gfx_round_frame(int x, int y, int w, int h, int r, int thickness, color_t c);
void gfx_progress_bar(int x, int y, int w, int h, int permille, color_t fill, color_t back);

int gfx_text(const font_t *f, int x, int y, const char *s, color_t c);
int gfx_text_aligned(const font_t *f, int x, int y, int w, align_t a, const char *s, color_t c);
/* Text with a 1-pixel dark shadow, for titles drawn over images and gradients. */
int gfx_text_shadow(const font_t *f, int x, int y, const char *s, color_t c, color_t shadow);
int gfx_text_width(const font_t *f, const char *s);

void gfx_sprite(const sprite_t *s, int x, int y);
/* Draws the sprite in a single color (silhouette of undiscovered pets, shadows). */
void gfx_sprite_tint(const sprite_t *s, int x, int y, color_t c);

color_t gfx_mix(color_t a, color_t b, int alpha /* 0..255, weight of b */);

#endif
