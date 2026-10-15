#!/usr/bin/env python3
"""Makes the app icon shown in the calculator's home menu (55x56, NumWorks' size)
from the artwork assets/art/app_icon.png (PC-only tool).

Usage: python3 tools/gen_icon.py [assets/icon.png]
"""
import sys
from PIL import Image

SOURCE = "assets/art/app_icon.png"
W, H = 55, 56

img = Image.open(SOURCE).convert("RGB")
img.resize((W, H), Image.LANCZOS).save(sys.argv[1] if len(sys.argv) > 1 else "assets/icon.png")
