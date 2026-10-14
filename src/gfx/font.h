#ifndef GFX_FONT_H
#define GFX_FONT_H

#include <stdint.h>

/* Anti-aliased bitmap font: 4 bits of coverage per pixel, rows padded to a byte. */
typedef struct {
  uint16_t codepoint;
  uint8_t width, height;
  int8_t x_offset, y_offset; /* relative to the pen position (top of the line) */
  uint8_t advance;
  uint32_t offset; /* into the font bitmap */
} glyph_t;

typedef struct {
  uint8_t line_height;
  uint8_t ascent;
  uint16_t glyph_count;
  const glyph_t *glyphs; /* sorted by codepoint */
  const uint8_t *bitmap;
} font_t;

extern const font_t font_small;
extern const font_t font_medium;
extern const font_t font_big;

const glyph_t *font_find_glyph(const font_t *font, uint32_t codepoint);

/* Decodes one UTF-8 character and advances *text. Returns 0 at the end. */
uint32_t utf8_next(const char **text);

#endif
