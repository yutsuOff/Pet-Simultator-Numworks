#!/usr/bin/env python3
"""Draws the provisional tab icons (PC-only tool).

Temporary assets, to be replaced by real images when provided (the coin, diamond and
breakables now use the art of assets/art/).
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


def main():
    for name, fn in [("tab_farm", coins_stack), ("tab_pets", paw), ("tab_eggs", egg), ("tab_zones", pin),
                     ("tab_upgrades", arrow_up), ("tab_index", book), ("tab_quests", star)]:
        save(fn(), name, 16, 16)
    print("placeholder art written to", OUT)


if __name__ == "__main__":
    main()
