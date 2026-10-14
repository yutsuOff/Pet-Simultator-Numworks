#!/usr/bin/env python3
"""Converts the PPM screenshots written by the PC build into PNG (x2 for readability)."""
import glob
import os
import sys
from PIL import Image

folder = sys.argv[1] if len(sys.argv) > 1 else "output/screens"
for path in sorted(glob.glob(os.path.join(folder, "*.ppm"))):
    img = Image.open(path)
    img.resize((img.width * 2, img.height * 2), Image.NEAREST).save(path[:-4] + ".png")
    os.remove(path)
print("PNG screenshots in", folder)
