#include "gfx.h"
#include "../platform/platform.h"

#define BAND_H 20

static color_t band[SCREEN_W * BAND_H];
static int band_y0, band_y1; /* current band, screen coordinates */

void gfx_render(void (*draw)(void), int y0, int y1) {
  if (y0 < 0) y0 = 0;
  if (y1 > SCREEN_H) y1 = SCREEN_H;
  for (int y = y0; y < y1; y += BAND_H) {
    band_y0 = y;
    band_y1 = y + BAND_H < y1 ? y + BAND_H : y1;
    draw();
    platform_push_rect(0, band_y0, SCREEN_W, band_y1 - band_y0, band);
  }
}

/* Intersects a rectangle with the current band. Returns false if empty. */
static bool clip(int *x, int *y, int *w, int *h) {
  int x0 = *x < 0 ? 0 : *x;
  int x1 = *x + *w > SCREEN_W ? SCREEN_W : *x + *w;
  int y0 = *y < band_y0 ? band_y0 : *y;
  int y1 = *y + *h > band_y1 ? band_y1 : *y + *h;
  if (x0 >= x1 || y0 >= y1) return false;
  *x = x0;
  *y = y0;
  *w = x1 - x0;
  *h = y1 - y0;
  return true;
}

static inline color_t *pixel(int x, int y) { return &band[(y - band_y0) * SCREEN_W + x]; }

color_t gfx_mix(color_t a, color_t b, int alpha) {
  if (alpha <= 0) return a;
  if (alpha >= 255) return b;
  int ra = a >> 11, ga = (a >> 5) & 63, ba = a & 31;
  int rb = b >> 11, gb = (b >> 5) & 63, bb = b & 31;
  int r = ra + (((rb - ra) * alpha) >> 8);
  int g = ga + (((gb - ga) * alpha) >> 8);
  int bl = ba + (((bb - ba) * alpha) >> 8);
  return (color_t)((r << 11) | (g << 5) | bl);
}

void gfx_fill_rect(int x, int y, int w, int h, color_t c) {
  if (!clip(&x, &y, &w, &h)) return;
  for (int j = 0; j < h; j++) {
    color_t *p = pixel(x, y + j);
    for (int i = 0; i < w; i++) p[i] = c;
  }
}

void gfx_blend_rect(int x, int y, int w, int h, color_t c, int alpha) {
  if (!clip(&x, &y, &w, &h)) return;
  for (int j = 0; j < h; j++) {
    color_t *p = pixel(x, y + j);
    for (int i = 0; i < w; i++) p[i] = gfx_mix(p[i], c, alpha);
  }
}

void gfx_vgradient(int x, int y, int w, int h, color_t top, color_t bottom) {
  int full_y = y, full_h = h;
  if (h <= 0 || !clip(&x, &y, &w, &h)) return;
  for (int j = 0; j < h; j++) {
    int t = full_h > 1 ? (y + j - full_y) * 255 / (full_h - 1) : 0;
    color_t c = gfx_mix(top, bottom, t);
    color_t *p = pixel(x, y + j);
    for (int i = 0; i < w; i++) p[i] = c;
  }
}

/* Rounded rectangle scanline: on line dy of a rect of height h and corner radius r,
 * the first `inset` pixels from each side are outside, the next one is covered at
 * *edge (0..255, 255 = fully inside). */
static int span_inset(int dy, int h, int r, int *edge) {
  *edge = 255;
  if (r <= 0) return 0;
  if (dy >= h - r) dy = h - 1 - dy;
  if (dy >= r) return 0;
  int cy = r * 16 - (dy * 16 + 8); /* 1/16 px, from the corner center */
  int dx2 = r * r * 256 - cy * cy;
  int dx = 0;
  while ((dx + 1) * (dx + 1) <= dx2) dx++;
  int inset16 = r * 16 - dx;
  int frac = inset16 & 15;
  if (frac) *edge = (16 - frac) * 16;
  return inset16 >> 4;
}

static inline void put(color_t *row, int x, color_t c, int alpha) {
  if (x < 0 || x >= SCREEN_W || alpha <= 0) return;
  row[x] = alpha >= 255 ? c : gfx_mix(row[x], c, alpha);
}

static void round_rect_impl(int x, int y, int w, int h, int r, color_t top, color_t bottom, bool gradient) {
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  int ly0 = y < band_y0 ? band_y0 : y;
  int ly1 = y + h > band_y1 ? band_y1 : y + h;
  for (int ly = ly0; ly < ly1; ly++) {
    int dy = ly - y, edge;
    int inset = span_inset(dy, h, r, &edge);
    color_t c = gradient ? gfx_mix(top, bottom, h > 1 ? dy * 255 / (h - 1) : 0) : top;
    color_t *p = pixel(0, ly);
    int x0 = x + inset, x1 = x + w - inset; /* [x0, x1) not outside */
    if (edge < 255) {
      put(p, x0, c, edge);
      put(p, x1 - 1, c, edge);
      x0++;
      x1--;
    }
    if (x0 < 0) x0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    for (int i = x0; i < x1; i++) p[i] = c;
  }
}

void gfx_round_rect(int x, int y, int w, int h, int r, color_t c) {
  round_rect_impl(x, y, w, h, r, c, c, false);
}

void gfx_round_gradient(int x, int y, int w, int h, int r, color_t top, color_t bottom) {
  round_rect_impl(x, y, w, h, r, top, bottom, true);
}

void gfx_round_frame(int x, int y, int w, int h, int r, int t, color_t c) {
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  int ir = r - t > 0 ? r - t : 0;
  int ly0 = y < band_y0 ? band_y0 : y;
  int ly1 = y + h > band_y1 ? band_y1 : y + h;
  for (int ly = ly0; ly < ly1; ly++) {
    int dy = ly - y, edge;
    int inset = span_inset(dy, h, r, &edge);
    color_t *p = pixel(0, ly);
    int ox0 = x + inset, ox1 = x + w - inset;
    if (dy < t || dy >= h - t) { /* top/bottom border: full line */
      if (edge < 255) {
        put(p, ox0, c, edge);
        put(p, ox1 - 1, c, edge);
        ox0++;
        ox1--;
      }
      for (int i = ox0; i < ox1; i++) put(p, i, c, 255);
      continue;
    }
    int iedge;
    int iinset = span_inset(dy - t, h - 2 * t, ir, &iedge);
    int ix0 = x + t + iinset, ix1 = x + w - t - iinset; /* inner area */
    if (edge < 255) {
      put(p, ox0, c, edge);
      put(p, ox1 - 1, c, edge);
      ox0++;
      ox1--;
    }
    for (int i = ox0; i < ix0; i++) put(p, i, c, 255);
    for (int i = ix1; i < ox1; i++) put(p, i, c, 255);
    if (iedge < 255) {
      put(p, ix0, c, 255 - iedge);
      put(p, ix1 - 1, c, 255 - iedge);
    }
  }
}

void gfx_progress_bar(int x, int y, int w, int h, int permille, color_t fill, color_t back) {
  if (permille < 0) permille = 0;
  if (permille > 1000) permille = 1000;
  int r = h / 2;
  gfx_round_rect(x, y, w, h, r, back);
  int fw = w * permille / 1000;
  if (fw > 0) {
    if (fw < h) fw = h;
    gfx_round_gradient(x, y, fw, h, r, gfx_mix(fill, RGB(255, 255, 255), 90), fill);
  }
}

/* --- Text --- */

uint32_t utf8_next(const char **text) {
  const unsigned char *s = (const unsigned char *)*text;
  uint32_t c = *s;
  if (c == 0) return 0;
  int extra = 0;
  if (c >= 0xF0) { c &= 0x07; extra = 3; }
  else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
  else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
  s++;
  while (extra-- > 0 && (*s & 0xC0) == 0x80) {
    c = (c << 6) | (*s & 0x3F);
    s++;
  }
  *text = (const char *)s;
  return c;
}

const glyph_t *font_find_glyph(const font_t *font, uint32_t cp) {
  int lo = 0, hi = font->glyph_count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint32_t v = font->glyphs[mid].codepoint;
    if (v == cp) return &font->glyphs[mid];
    if (v < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return cp == '?' ? 0 : font_find_glyph(font, '?');
}

int gfx_text_width(const font_t *f, const char *s) {
  int w = 0;
  uint32_t cp;
  while ((cp = utf8_next(&s)) != 0) {
    const glyph_t *g = font_find_glyph(f, cp);
    if (g) w += g->advance;
  }
  return w;
}

static void draw_glyph(const font_t *f, const glyph_t *g, int x, int y, color_t c) {
  int gx = x + g->x_offset, gy = y + g->y_offset;
  int stride = (g->width + 1) / 2;
  int y0 = gy < band_y0 ? band_y0 : gy;
  int y1 = gy + g->height > band_y1 ? band_y1 : gy + g->height;
  const uint8_t *bits = f->bitmap + g->offset;
  for (int py = y0; py < y1; py++) {
    const uint8_t *row = bits + (py - gy) * stride;
    color_t *p = pixel(0, py);
    for (int i = 0; i < g->width; i++) {
      int px = gx + i;
      if (px < 0 || px >= SCREEN_W) continue;
      int a = (i & 1) ? (row[i >> 1] & 15) : (row[i >> 1] >> 4);
      if (a == 0) continue;
      p[px] = a == 15 ? c : gfx_mix(p[px], c, a * 17);
    }
  }
}

int gfx_text(const font_t *f, int x, int y, const char *s, color_t c) {
  int start = x;
  if (y >= band_y1 || y + f->line_height <= band_y0) return gfx_text_width(f, s);
  uint32_t cp;
  while ((cp = utf8_next(&s)) != 0) {
    const glyph_t *g = font_find_glyph(f, cp);
    if (!g) continue;
    if (g->width) draw_glyph(f, g, x, y, c);
    x += g->advance;
  }
  return x - start;
}

int gfx_text_aligned(const font_t *f, int x, int y, int w, align_t a, const char *s, color_t c) {
  int tw = gfx_text_width(f, s);
  if (a == ALIGN_CENTER) x += (w - tw) / 2;
  else if (a == ALIGN_RIGHT) x += w - tw;
  gfx_text(f, x, y, s, c);
  return tw;
}

int gfx_text_shadow(const font_t *f, int x, int y, const char *s, color_t c, color_t shadow) {
  gfx_text(f, x + 1, y + 1, s, shadow);
  return gfx_text(f, x, y, s, c);
}

/* --- Sprites --- */

/* --- Effects (golden / rainbow pets): recolors the sprite from its luminance --- */

static int effect;
static int effect_phase;

void gfx_set_effect(int e, int phase) {
  effect = e;
  effect_phase = phase;
}

static color_t hue_color(int hue) { /* 0..359, full saturation */
  int seg = hue / 60, f = (hue % 60) * 255 / 60;
  switch (seg) {
    case 0: return RGB(255, f, 0);
    case 1: return RGB(255 - f, 255, 0);
    case 2: return RGB(0, 255, f);
    case 3: return RGB(0, 255 - f, 255);
    case 4: return RGB(f, 0, 255);
    default: return RGB(255, 0, 255 - f);
  }
}

static color_t apply_effect(color_t c, int px, int py) {
  int lum = (((c >> 11) * 8) * 77 + (((c >> 5) & 63) * 4) * 150 + ((c & 31) * 8) * 29) >> 8; /* 0..255 */
  if (effect == GFX_EFFECT_GOLDEN) {
    if (lum < 150) return gfx_mix(RGB(90, 55, 0), RGB(240, 180, 30), lum * 255 / 150);
    return gfx_mix(RGB(240, 180, 30), RGB(255, 250, 200), (lum - 150) * 255 / 105);
  }
  color_t h = hue_color(((px + py) * 8 + effect_phase) % 360);
  if (lum < 128) return gfx_mix(gfx_mix(h, RGB(0, 0, 0), 170), h, lum * 2);
  return gfx_mix(h, RGB(255, 255, 255), (lum - 128) * 160 / 127);
}

static void sprite_impl(const sprite_t *s, int x, int y, bool tint, color_t tc) {
  int y0 = y < band_y0 ? band_y0 : y;
  int y1 = y + s->height > band_y1 ? band_y1 : y + s->height;
  for (int py = y0; py < y1; py++) {
    const uint8_t *row = s->pixels + (py - y) * s->width;
    color_t *p = pixel(0, py);
    for (int i = 0; i < s->width; i++) {
      int px = x + i;
      if (px < 0 || px >= SCREEN_W) continue;
      uint8_t idx = row[i];
      int a = s->palette_alpha[idx];
      if (a == 0) continue;
      color_t c = tint ? tc : s->palette[idx];
      if (effect && !tint) c = apply_effect(c, i, py - y);
      p[px] = a == 15 ? c : gfx_mix(p[px], c, a * 17);
    }
  }
}

void gfx_sprite(const sprite_t *s, int x, int y) { sprite_impl(s, x, y, false, 0); }

void gfx_sprite_tint(const sprite_t *s, int x, int y, color_t c) { sprite_impl(s, x, y, true, c); }

/* Half size: each destination pixel averages a 2x2 block (colors weighted by alpha). */
static void sprite_half_impl(const sprite_t *s, int x, int y, bool tint, color_t tc) {
  int w = s->width / 2, h = s->height / 2;
  int y0 = y < band_y0 ? band_y0 : y;
  int y1 = y + h > band_y1 ? band_y1 : y + h;
  for (int py = y0; py < y1; py++) {
    const uint8_t *r0 = s->pixels + (py - y) * 2 * s->width;
    const uint8_t *r1 = r0 + s->width;
    color_t *p = pixel(0, py);
    for (int i = 0; i < w; i++) {
      int px = x + i;
      if (px < 0 || px >= SCREEN_W) continue;
      uint8_t idx[4] = {r0[2 * i], r0[2 * i + 1], r1[2 * i], r1[2 * i + 1]};
      int asum = 0, rs = 0, gs = 0, bs = 0;
      for (int k = 0; k < 4; k++) {
        int a = s->palette_alpha[idx[k]];
        if (!a) continue;
        color_t c = s->palette[idx[k]];
        asum += a;
        rs += (c >> 11) * a;
        gs += ((c >> 5) & 63) * a;
        bs += (c & 31) * a;
      }
      if (!asum) continue;
      color_t c = tint ? tc : (color_t)(((rs / asum) << 11) | ((gs / asum) << 5) | (bs / asum));
      if (effect && !tint) c = apply_effect(c, i * 2, (py - y) * 2);
      int a = asum * 17 / 4; /* 0..255 */
      p[px] = a >= 255 ? c : gfx_mix(p[px], c, a);
    }
  }
}

void gfx_sprite_half(const sprite_t *s, int x, int y) { sprite_half_impl(s, x, y, false, 0); }

void gfx_sprite_half_tint(const sprite_t *s, int x, int y, color_t c) { sprite_half_impl(s, x, y, true, c); }

void gfx_sprite_scaled(const sprite_t *s, int x, int y, int scale) {
  int h = s->height * scale;
  int y0 = y < band_y0 ? band_y0 : y;
  int y1 = y + h > band_y1 ? band_y1 : y + h;
  for (int py = y0; py < y1; py++) {
    const uint8_t *row = s->pixels + (py - y) / scale * s->width;
    color_t *p = pixel(0, py);
    for (int i = 0; i < s->width * scale; i++) {
      int px = x + i;
      if (px < 0 || px >= SCREEN_W) continue;
      uint8_t idx = row[i / scale];
      int a = s->palette_alpha[idx];
      if (a == 0) continue;
      color_t c = s->palette[idx];
      if (effect) c = apply_effect(c, i / scale, (py - y) / scale);
      p[px] = a == 15 ? c : gfx_mix(p[px], c, a * 17);
    }
  }
}
