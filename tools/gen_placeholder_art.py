#!/usr/bin/env python3
"""Draws the provisional UI icons and placeholder art (PC-only tool).

These are temporary assets, to be replaced by the real images when provided.
Everything is drawn 4x larger then downscaled, for smooth anti-aliased edges.

Usage: python3 tools/gen_placeholder_art.py
"""
import math
import os
from PIL import Image, ImageDraw

SS = 4  # supersampling
OUT = "assets/ui"


def canvas(w, h):
    img = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def save(img, name, w, h):
    img = img.resize((w, h), Image.LANCZOS)
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, name + ".png"))


def s(*v):
    return [int(round(x * SS)) for x in v]


def coin(size=14):
    img, d = canvas(size, size)
    d.ellipse(s(0.5, 0.5, size - 0.5, size - 0.5), fill=(196, 120, 10), outline=(120, 70, 0), width=SS)
    d.ellipse(s(2, 2, size - 2, size - 2), fill=(255, 200, 40))
    d.ellipse(s(3.5, 3.5, size - 3.5, size - 3.5), outline=(220, 150, 20), width=SS)
    d.ellipse(s(4, 3, 7, 6), fill=(255, 245, 190))
    return img


def gem(size=14):
    img, d = canvas(size, size)
    w = size
    top, mid, bot = 1.5, w * 0.38, w - 1
    outline = [(w * 0.25, top), (w * 0.75, top), (w - 1, mid), (w / 2, bot), (1, mid)]
    d.polygon([tuple(s(*p)) for p in outline], fill=(40, 170, 255), outline=(10, 70, 160))
    d.polygon([tuple(s(*p)) for p in [(w * 0.25, top), (w * 0.75, top), (w * 0.62, mid), (w * 0.38, mid)]],
              fill=(150, 225, 255))
    d.polygon([tuple(s(*p)) for p in [(w * 0.38, mid), (w * 0.62, mid), (w / 2, bot)]], fill=(90, 200, 255))
    d.line([tuple(s(*p)) for p in [(1, mid), (w - 1, mid)]], fill=(10, 90, 190), width=SS)
    return img


def paw(size=16, color=(255, 255, 255)):
    img, d = canvas(size, size)
    d.ellipse(s(4, 7, 12, 14.5), fill=color)
    for cx, cy in [(3, 6), (6.2, 3), (9.8, 3), (13, 6)]:
        d.ellipse(s(cx - 1.8, cy - 2.2, cx + 1.8, cy + 2.2), fill=color)
    return img


def egg(size=16, color=(255, 255, 255)):
    img, d = canvas(size, size)
    d.ellipse(s(3, 1, 13, 15), fill=color)
    return img


def coins_stack(size=16, color=(255, 255, 255)):
    img, d = canvas(size, size)
    for i, y in enumerate([11, 8, 5]):
        d.ellipse(s(2, y, 12, y + 4), fill=color, outline=(0, 0, 0, 0))
        d.rectangle(s(2, y + 2, 12, y + 4.5), fill=color)
        d.line(s(2, y + 4.5, 12, y + 4.5), fill=(0, 0, 0, 0), width=SS)
    d.ellipse(s(9, 1, 15, 7), fill=color)
    return img


def pin(size=16, color=(255, 255, 255)):
    img, d = canvas(size, size)
    d.ellipse(s(3, 1, 13, 11), fill=color)
    d.polygon([tuple(s(*p)) for p in [(4, 8), (12, 8), (8, 15.5)]], fill=color)
    d.ellipse(s(6, 4, 10, 8), fill=(0, 0, 0, 0))
    return img


def arrow_up(size=16, color=(255, 255, 255)):
    img, d = canvas(size, size)
    d.polygon([tuple(s(*p)) for p in [(8, 1), (15, 8), (11, 8), (11, 15), (5, 15), (5, 8), (1, 8)]], fill=color)
    return img


def book(size=16, color=(255, 255, 255)):
    img, d = canvas(size, size)
    d.rounded_rectangle(s(2, 1.5, 14, 14.5), radius=1.5 * SS, fill=color)
    d.rectangle(s(4, 1.5, 5, 14.5), fill=(0, 0, 0, 0))
    d.rectangle(s(7, 4, 12, 5.5), fill=(0, 0, 0, 0))
    return img


def star(size=16, color=(255, 255, 255)):
    img, d = canvas(size, size)
    pts = []
    for i in range(10):
        r = 7.5 if i % 2 == 0 else 3.2
        a = -math.pi / 2 + i * math.pi / 5
        pts.append(tuple(s(8 + r * math.cos(a), 8.5 + r * math.sin(a))))
    d.polygon(pts, fill=color)
    return img


def coin_pile(w=34, h=28):
    img, d = canvas(w, h)
    rows = [(4, 22, 7), (8, 18, 6), (12, 14, 5)]
    for x0, y0, n in rows:
        for i in range(n):
            cx = x0 + i * 4.2
            d.ellipse(s(cx, y0, cx + 6, y0 + 5), fill=(240, 175, 25), outline=(150, 90, 0), width=SS)
    d.ellipse(s(13, 6, 20, 11), fill=(255, 205, 50), outline=(150, 90, 0), width=SS)
    d.ellipse(s(15, 7, 17, 8.5), fill=(255, 250, 200))
    return img


def chest(w=34, h=28):
    img, d = canvas(w, h)
    d.rounded_rectangle(s(3, 11, 31, 26), radius=2 * SS, fill=(150, 85, 35), outline=(80, 40, 10), width=SS)
    d.rounded_rectangle(s(3, 4, 31, 13), radius=5 * SS, fill=(175, 105, 45), outline=(80, 40, 10), width=SS)
    d.rectangle(s(3, 12, 31, 14), fill=(230, 180, 40))
    d.rectangle(s(15, 10, 19, 17), fill=(250, 210, 60), outline=(120, 80, 0), width=SS)
    return img


def present(w=34, h=28):
    img, d = canvas(w, h)
    d.rounded_rectangle(s(6, 10, 28, 27), radius=2 * SS, fill=(220, 60, 90), outline=(120, 20, 40), width=SS)
    d.rounded_rectangle(s(4, 7, 30, 12), radius=1.5 * SS, fill=(240, 80, 110), outline=(120, 20, 40), width=SS)
    d.rectangle(s(15, 7, 19, 27), fill=(255, 220, 80))
    d.ellipse(s(10, 1, 17, 8), outline=(255, 220, 80), width=2 * SS)
    d.ellipse(s(17, 1, 24, 8), outline=(255, 220, 80), width=2 * SS)
    return img


def gem_crate(w=34, h=28):
    img, d = canvas(w, h)
    d.rounded_rectangle(s(4, 9, 30, 27), radius=2 * SS, fill=(90, 110, 140), outline=(40, 50, 70), width=SS)
    d.line(s(4, 18, 30, 18), fill=(40, 50, 70), width=SS)
    g = gem(16)
    img.alpha_composite(g, tuple(s(9, 0)))
    return img


def main():
    save(coin(), "coin", 14, 14)
    save(gem(), "gem", 14, 14)
    for name, fn in [("tab_farm", coins_stack), ("tab_pets", paw), ("tab_eggs", egg), ("tab_zones", pin),
                     ("tab_upgrades", arrow_up), ("tab_index", book), ("tab_quests", star)]:
        save(fn(), name, 16, 16)
    save(coin_pile(), "brk_coins", 34, 28)
    save(chest(), "brk_chest", 34, 28)
    save(present(), "brk_present", 34, 28)
    save(gem_crate(), "brk_gems", 34, 28)
    print("placeholder art written to", OUT)


if __name__ == "__main__":
    main()
