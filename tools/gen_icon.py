#!/usr/bin/env python3
"""Draws the provisional app icon (55x56, the size used by NumWorks' home screen)."""
import sys
from PIL import Image, ImageDraw

SS = 4
W, H = 55, 56
img = Image.new("RGB", (W * SS, H * SS), (255, 255, 255))
d = ImageDraw.Draw(img)
for y in range(H * SS):  # sky-to-grass gradient
    t = y / (H * SS)
    d.line([(0, y), (W * SS, y)], fill=(int(70 + 60 * t), int(150 + 70 * t), int(255 - 115 * t)))
# egg
d.ellipse([14 * SS, 6 * SS, 41 * SS, 44 * SS], fill=(255, 245, 225), outline=(60, 50, 90), width=2 * SS)
d.ellipse([20 * SS, 14 * SS, 26 * SS, 20 * SS], fill=(255, 190, 60))
d.ellipse([30 * SS, 24 * SS, 36 * SS, 30 * SS], fill=(120, 200, 255))
d.ellipse([19 * SS, 31 * SS, 24 * SS, 36 * SS], fill=(255, 120, 160))
# paw
px, py = 38, 40
d.ellipse([(px - 6) * SS, (py - 1) * SS, (px + 6) * SS, (py + 9) * SS], fill=(255, 255, 255), outline=(60, 50, 90), width=SS)
for dx, dy in [(-7, -3), (-3, -7), (3, -7), (7, -3)]:
    d.ellipse([(px + dx - 2.4) * SS, (py + dy - 2.8) * SS, (px + dx + 2.4) * SS, (py + dy + 2.8) * SS],
              fill=(255, 255, 255), outline=(60, 50, 90), width=SS)
img.resize((W, H), Image.LANCZOS).save(sys.argv[1] if len(sys.argv) > 1 else "assets/icon.png")
