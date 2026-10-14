#!/usr/bin/env python3
"""Converts the game content (data/*.csv + assets/pets, eggs, zones) into C tables
(PC-only tool, needs Python 3 + Pillow). Output: src/generated/content.c and content.h.

    python3 tools/gen_data.py

Wiki data (names, prices, chances) is used as is. What the wiki does not give is
computed here by the game rules below, so they are easy to tune in one place:
  - hatch chances: the wiki percentages are normalized into integer weights summing to
    exactly 1,000,000,000; a chance shown "??" on the wiki gets SECRET_PERCENT;
  - rarity tier of a pet (all egg pets are "Basic" on the PS99 wiki): from its chance in
    its home egg (the last egg that contains it);
  - pet power, breakable HP and rewards: provisional balancing (docs/CONCEPTION.md §6).
"""
import csv
import os
import sys

from PIL import Image, ImageStat

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from convert_images import sprite_c  # noqa: E402

OUT = "src/generated/content"
WEIGHT_TOTAL = 1_000_000_000
SECRET_PERCENT = 0.05          # chance used for pets shown "??" on the wiki

# Game rarity tiers: (C name, minimum chance in % in the home egg, power multiplier)
TIERS = [("TIER_BASIC", 20.0, 1.0), ("TIER_RARE", 5.0, 1.4), ("TIER_EPIC", 1.0, 2.0),
         ("TIER_LEGENDARY", 0.1, 3.0), ("TIER_MYTHICAL", 0.0, 5.0)]
PET_BASE_POWER = 10.0           # power of a basic pet of egg 1
PET_POWER_GROWTH = 1.35         # x per egg number
PILE_HP_PETS = 4.0              # a coin pile of zone z has the HP of 4 basic pets of its first egg
SECONDS_PER_ZONE = 250.0        # a coin pile gives cost(next zone) / this

PET_SIZE = (48, 48)
EGG_SIZE = (48, 48)
THUMB_SIZE = (64, 36)


def read(name):
    with open(f"data/{name}.csv", newline="") as f:
        return list(csv.DictReader(f))


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def rgb565(c):
    r, g, b = (max(0, min(255, int(v))) for v in c[:3])
    return f"RGB({r}, {g}, {b})"


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def fit(path, size):
    img = Image.open(path).convert("RGBA")
    img.thumbnail(size, Image.LANCZOS)
    framed = Image.new("RGBA", size, (0, 0, 0, 0))
    framed.alpha_composite(img, ((size[0] - img.width) // 2, (size[1] - img.height) // 2))
    return framed


def fill(path, size):
    """Center crop to the aspect ratio of size, then resize (zone thumbnails)."""
    img = Image.open(path).convert("RGBA")
    w, h = img.size
    tw, th = size
    if w * th > h * tw:
        nw = h * tw // th
        img = img.crop(((w - nw) // 2, 0, (w - nw) // 2 + nw, h))
    else:
        nh = w * th // tw
        img = img.crop((0, (h - nh) // 2, w, (h - nh) // 2 + nh))
    return img.resize(size, Image.LANCZOS)


def zone_colors(path):
    """Background gradient and badge color taken from the area screenshot."""
    img = Image.open(path).convert("RGB")
    w, h = img.size
    top = ImageStat.Stat(img.crop((0, 0, w, h // 3))).mean
    bottom = ImageStat.Stat(img.crop((0, h * 2 // 3, w, h))).mean
    whole = ImageStat.Stat(img).mean
    sky_top = mix(top, (255, 255, 255), 0.35)
    sky_bottom = mix(bottom, (255, 255, 255), 0.15)
    # badge: saturated version of the average, dark enough for white text
    m = sum(whole) / 3
    accent = tuple(m + (v - m) * 2.2 for v in whole)
    lum = 0.3 * accent[0] + 0.59 * accent[1] + 0.11 * accent[2]
    if lum > 130:
        accent = tuple(v * 130 / lum for v in accent)
    return sky_top, sky_bottom, accent


def normalize(slots):
    """[(pet, '12.5' or '??')] -> [(pet, secret, weight)] with weights summing to WEIGHT_TOTAL."""
    secret = [p for p, c in slots if c == "??"]
    known = [(p, float(c)) for p, c in slots if c != "??"]
    secret_w = round(SECRET_PERCENT / 100 * WEIGHT_TOTAL)
    budget = WEIGHT_TOTAL - secret_w * len(secret)
    total = sum(c for _, c in known)
    raw = [(p, c / total * budget) for p, c in known]
    weights = {p: int(w) for p, w in raw}
    left = budget - sum(weights.values())
    for p, w in sorted(raw, key=lambda pw: pw[1] - int(pw[1]), reverse=True)[:left]:
        weights[p] += 1  # largest remainder
    out = []
    for p, c in slots:
        out.append((p, c == "??", secret_w if c == "??" else weights[p]))
    assert sum(w for _, _, w in out) == WEIGHT_TOTAL
    return out


def main():
    zones, eggs, egg_pets, pets = read("zones"), read("eggs"), read("egg_pets"), read("pets")
    pet_index = {p["pet"]: i for i, p in enumerate(pets)}
    zone_index = {int(z["area"]): i for i, z in enumerate(zones)}
    if len(pets) > 255 or len(eggs) > 255 or len(zones) > 255:
        sys.exit("too many entries for uint8_t indices")

    # hatch slots
    slots, egg_slots = [], []
    for e in eggs:
        rows = [(pet_index[r["pet"]], r["chance_percent"]) for r in egg_pets if r["egg"] == e["egg"]]
        egg_slots.append((len(slots), len(rows)))
        slots += normalize(rows)

    # home egg and tier of each pet: the last egg where its chance is known
    home = {}
    for ei, (first, count) in enumerate(egg_slots):
        for pet, secret, w in slots[first:first + count]:
            if not secret or pet not in home or home[pet][2]:
                home[pet] = (ei, w * 100.0 / WEIGHT_TOTAL, secret)
    pet_rows = []
    for i, p in enumerate(pets):
        egg, percent, _ = home[i]
        tier = next(t for t in TIERS if percent >= t[1])
        power = PET_BASE_POWER * PET_POWER_GROWTH ** egg * tier[2]
        pet_rows.append((p["pet"], egg, tier[0], float(f"{power:.3g}")))

    # zones: eggs, colors, breakables
    zone_eggs = {i: [] for i in range(len(zones))}
    for ei, e in enumerate(eggs):
        zone_eggs[zone_index[int(e["area"])]].append(ei)
    zone_rows = []
    for zi, z in enumerate(zones):
        if not zone_eggs[zi]:
            sys.exit(f"zone {z['name']} has no egg")
        first = zone_eggs[zi][0]
        hp = PILE_HP_PETS * PET_BASE_POWER * PET_POWER_GROWTH ** first
        cost = float(z["cost_coins"])
        nxt = float(zones[zi + 1]["cost_coins"]) if zi + 1 < len(zones) else \
            cost * cost / float(zones[zi - 1]["cost_coins"])
        reward = nxt / SECONDS_PER_ZONE
        zone_rows.append((z, first, len(zone_eggs[zi]), cost, float(f"{hp:.3g}"), float(f"{reward:.3g}"),
                          zone_colors("assets/" + z["image"])))

    c = ["/* Generated by tools/gen_data.py from data/ and assets/ - do not edit. */",
         '#include "content.h"', ""]
    flash = 0
    for i, p in enumerate(pets):
        lines, size = sprite_c(f"pet_spr_{i}", fit("assets/" + p["image"], PET_SIZE), "static ")
        c += lines
        flash += size
    for i, e in enumerate(eggs):
        lines, size = sprite_c(f"egg_spr_{i}", fit("assets/" + e["image"], EGG_SIZE), "static ")
        c += lines
        flash += size
    for i, z in enumerate(zones):
        lines, size = sprite_c(f"zone_spr_{i}", fill("assets/" + z["image"], THUMB_SIZE), "static ")
        c += lines
        flash += size

    c.append("const pet_t pets[PET_COUNT] = {")
    for i, (name, egg, tier, power) in enumerate(pet_rows):
        c.append(f"  {{{c_str(name)}, &pet_spr_{i}, {power!r}, {tier}, {egg}}},")
    c.append("};")
    c.append("")
    c.append("const egg_slot_t egg_slots[EGG_SLOT_COUNT] = {")
    for pet, secret, w in slots:
        c.append(f"  {{{pet}, {int(secret)}, {w}u}}, /* {pets[pet]['pet']} */")
    c.append("};")
    c.append("")
    c.append("const egg_t eggs[EGG_COUNT] = {")
    for i, e in enumerate(eggs):
        first, count = egg_slots[i]
        c.append(f"  {{{c_str(e['name'])}, &egg_spr_{i}, {float(e['cost_coins'])!r}, "
                 f"{zone_index[int(e['area'])]}, {e['egg']}, {first}, {count}}},")
    c.append("};")
    c.append("")
    c.append("const zone_t zones[ZONE_COUNT] = {")
    for i, (z, first, count, cost, hp, reward, (top, bottom, accent)) in enumerate(zone_rows):
        c.append(f"  {{{c_str(z['name'])}, &zone_spr_{i}, {cost!r}, {rgb565(top)}, {rgb565(bottom)}, "
                 f"{rgb565(accent)}, {first}, {count}, {hp!r}, {reward!r}}},")
    c.append("};")
    c.append("")

    h = ["/* Generated by tools/gen_data.py - do not edit. */",
         "#ifndef GENERATED_CONTENT_H", "#define GENERATED_CONTENT_H", "",
         '#include "../game/content_types.h"', "",
         f"#define PET_COUNT {len(pets)}",
         f"#define EGG_COUNT {len(eggs)}",
         f"#define EGG_SLOT_COUNT {len(slots)}",
         f"#define ZONE_COUNT {len(zones)}",
         f"#define EGG_WEIGHT_TOTAL {WEIGHT_TOTAL}u", "",
         "extern const pet_t pets[PET_COUNT];",
         "extern const egg_slot_t egg_slots[EGG_SLOT_COUNT];",
         "extern const egg_t eggs[EGG_COUNT];",
         "extern const zone_t zones[ZONE_COUNT];", "", "#endif", ""]
    with open(OUT + ".c", "w") as f:
        f.write("\n".join(c))
    with open(OUT + ".h", "w") as f:
        f.write("\n".join(h))
    tiers = {}
    for row in pet_rows:
        tiers[row[2]] = tiers.get(row[2], 0) + 1
    print(f"{OUT}.c: {len(zones)} zones, {len(eggs)} eggs, {len(pets)} pets, "
          f"{flash // 1024} KB of images; tiers {tiers}")


if __name__ == "__main__":
    main()
