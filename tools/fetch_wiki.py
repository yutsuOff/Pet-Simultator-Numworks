#!/usr/bin/env python3
"""Downloads the Pet Simulator 99 content used by the game from the fandom wiki
(PC-only tool, needs network access to pet-simulator.fandom.com).

    python3 tools/fetch_wiki.py [--eggs 50]

Reads the official wiki tables (eggs of World 1, areas of the Spawn World) and the
infobox of every pet, then writes:
    data/zones.csv      the areas that hold the first N eggs
    data/eggs.csv       the first N eggs (number, zone, price)
    data/egg_pets.csv   the pets of each egg with the chance shown on the wiki
    data/pets.csv       every pet found in those eggs, with its rarity
    assets/pets/*.png   pet images   (downscaled to 96 px, transparent)
    assets/eggs/*.png   egg images   (downscaled to 96 px, transparent)
    assets/zones/*.png  area screenshots (downscaled to 128 px wide)
Nothing is invented: a value missing on the wiki stops the script.
"""
import argparse
import csv
import io
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request

from PIL import Image

API = "https://pet-simulator.fandom.com/api.php"
EGG_LIST = "Template:PS99World1EggsList"
AREA_LIST = "Template:PS99SpawnWorldAreaList"
UA = "PetSimNumWorks-fan-project/1.0 (data import script)"

# Coin tiers of World 1 (wiki page "Currencies (Pet Simulator 99)").
COIN_VALUE = {"": 1.0, "GoldBar": 1e6, "Platinum": 1e12, "PlatinumBar": 1e18, "Emerald": 1e24,
              "EmeraldBar": 1e30, "Sapphire": 1e36}
RARITIES = ["Basic", "Rare", "Epic", "Legendary", "Mythical", "Secret", "Exclusive", "Celestial"]


def api(**params):
    params.update(format="json", formatversion=2)
    url = API + "?" + urllib.parse.urlencode(params)
    for attempt in range(4):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            return json.load(urllib.request.urlopen(req, timeout=30))
        except OSError:
            time.sleep(2 ** attempt)
    sys.exit(f"network error: {url}")


def wikitext(page):
    return api(action="parse", page=page, prop="wikitext", redirects=1)["parse"]["wikitext"]


def parse_cost(text):
    """'1 {{Coin 99|GoldBar}} 500,000 {{Coin 99}}' -> (1500000.0, [(1, 'GoldBar'), (500000, '')])."""
    total, parts = 0.0, []
    for amount, unit in re.findall(r"([\d.,]+[kKmM]?)\s*\{\{Coin 99(?:\|(\w+))?\}\}", text):
        mult = 1.0
        if amount[-1] in "kK":
            mult, amount = 1e3, amount[:-1]
        elif amount[-1] in "mM":
            mult, amount = 1e6, amount[:-1]
        value = float(amount.replace(",", "")) * mult
        if unit not in COIN_VALUE:
            sys.exit(f"unknown currency {unit!r} in {text!r}")
        total += value * COIN_VALUE[unit]
        parts.append((value, unit))
    if not parts:
        sys.exit(f"cannot read price {text!r}")
    return total, parts


def parse_eggs(text, count):
    eggs = []
    for row in re.split(r"\n\|-", text):
        m = re.search(r"\[\[File:([^|\]]+)\|50px\|link=[^\]]+\]\]\s*\n\|\s*'''\[\[[^|]+\|([^\]]+)\]\]'''<br>Egg (\d+)\s*\n"
                      r"\|\s*([^\n]+)\n\|\s*'''([^']+)'''<br>Area (\d+)", row)
        if not m:
            continue
        image, name, number, cost, area_name, area = m.groups()
        pets = []
        for line in re.findall(r"^\|(PS99 [^\n]+)$", row, re.M):
            fields = line.split(";")
            if len(fields) != 5:
                sys.exit(f"bad pet line {line!r}")
            pets.append({"image": fields[0].strip(), "name": fields[3].replace(" (Pet Simulator 99)", "").strip(),
                         "chance": fields[4].strip()})
        total, parts = parse_cost(cost)
        eggs.append({"number": int(number), "name": name.strip(), "image": image.strip(),
                     "cost": total, "cost_parts": parts, "area": int(area), "area_name": area_name.strip(),
                     "pets": pets})
    eggs.sort(key=lambda e: e["number"])
    eggs = [e for e in eggs if e["number"] <= count]
    if [e["number"] for e in eggs] != list(range(1, count + 1)):
        sys.exit("missing eggs in the wiki table: " + str([e["number"] for e in eggs]))
    for e in eggs:
        for p in e["pets"]:
            # "??" = chance hidden on the wiki (rare teaser pet of the next egg): kept as is.
            p["chance"] = p["chance"].rstrip("%")
            if p["chance"] != "??":
                try:
                    float(p["chance"])
                except ValueError:
                    sys.exit(f"unknown chance for {p['name']} in {e['name']}: {p['chance']!r}")
    return eggs


def parse_areas(text):
    areas = {}
    for row in re.split(r"\n\|-", text):
        m = re.search(r"'''Area (\d+)'''<br>\(([^)]+)\)\s*\n\|([^\n]*)\n\|\s*\[\[File:([^|\]]+)", row)
        if not m:
            continue
        number, name, cost, image = m.groups()
        number = int(number)
        if "Unlocked by default" in cost:
            total, parts = 0.0, []
        else:
            total, parts = parse_cost(cost)
        areas[number] = {"number": number, "name": name.strip(), "cost": total, "cost_parts": parts,
                         "image": image.strip()}
    return areas


def pet_rarities(names):
    rarities = {}
    names = sorted(names)
    for i in range(0, len(names), 40):
        batch = names[i:i + 40]
        titles = "|".join(f"{n} (Pet Simulator 99)" for n in batch)
        data = api(action="query", prop="revisions", rvprop="content", rvslots="main", titles=titles, redirects=1)
        redirects = {r["to"]: r["from"] for r in data["query"].get("redirects", [])}
        for page in data["query"]["pages"]:
            title = redirects.get(page["title"], page["title"])
            name = title.replace(" (Pet Simulator 99)", "")
            if "revisions" not in page:
                sys.exit(f"no wiki page for pet {name!r}")
            content = page["revisions"][0]["slots"]["main"]["content"]
            m = re.search(r"\|rarity\s*=\s*\{\{(\w+?)99\}\}", content)
            if not m or m.group(1) not in RARITIES:
                sys.exit(f"no rarity for pet {name!r}")
            rarities[name] = m.group(1)
    missing = set(names) - set(rarities)
    if missing:
        sys.exit(f"pets without page: {sorted(missing)}")
    return rarities


def image_urls(files):
    urls = {}
    files = sorted(set(files))
    for i in range(0, len(files), 40):
        titles = "|".join("File:" + f for f in files[i:i + 40])
        data = api(action="query", prop="imageinfo", iiprop="url", titles=titles)
        norm = {n["to"]: n["from"] for n in data["query"].get("normalized", [])}
        for page in data["query"]["pages"]:
            name = norm.get(page["title"], page["title"])[5:]
            if "imageinfo" not in page:
                sys.exit(f"image not found on the wiki: {name}")
            urls[name] = page["imageinfo"][0]["url"]
    return urls


def download(url, dest, max_size):
    if os.path.exists(dest):
        return
    for attempt in range(4):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "image/png,image/*"})
            raw = urllib.request.urlopen(req, timeout=60).read()
            break
        except OSError:
            time.sleep(2 ** attempt)
    else:
        sys.exit(f"cannot download {url}")
    img = Image.open(io.BytesIO(raw)).convert("RGBA")
    bbox = img.getchannel("A").getbbox()
    if bbox and max_size[1] == max_size[0]:  # pets and eggs: crop the transparent margins
        img = img.crop(bbox)
    img.thumbnail(max_size, Image.LANCZOS)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    img.save(dest, optimize=True)


def slug(name):
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


def cost_text(parts):
    names = {"": "Coins", "GoldBar": "Gold Bars", "Platinum": "Platinum Coins", "PlatinumBar": "Platinum Bars",
             "Emerald": "Emerald Coins", "EmeraldBar": "Emerald Bars", "Sapphire": "Sapphire Coins"}
    return " + ".join(f"{v:g} {names[u]}" for v, u in parts) if parts else "free"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--eggs", type=int, default=50, help="number of eggs to import (from egg 1)")
    ap.add_argument("--no-images", action="store_true")
    args = ap.parse_args()

    eggs = parse_eggs(wikitext(EGG_LIST), args.eggs)
    areas = parse_areas(wikitext(AREA_LIST))
    zone_numbers = sorted({e["area"] for e in eggs})
    zone_numbers = list(range(1, zone_numbers[-1] + 1))  # every area up to the last egg
    for n in zone_numbers:
        if n not in areas:
            sys.exit(f"area {n} missing from the wiki table")
    for e in eggs:
        if areas[e["area"]]["name"].lower().replace("chrystal", "crystal") != e["area_name"].lower():
            print(f"note: egg {e['number']} says area {e['area']} is {e['area_name']!r}, "
                  f"area list says {areas[e['area']]['name']!r}")

    pets = {}
    for e in eggs:
        for p in e["pets"]:
            pets.setdefault(p["name"], p["image"])
    rarities = pet_rarities(list(pets))

    os.makedirs("data", exist_ok=True)
    with open("data/zones.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["area", "name", "cost_coins", "cost_wiki", "image"])
        for n in zone_numbers:
            a = areas[n]
            name = a["name"].replace("Chrystal", "Crystal")  # typo on the wiki area list
            w.writerow([n, name, f"{a['cost']:.0f}", cost_text(a["cost_parts"]), f"zones/{slug(name)}.png"])
    with open("data/eggs.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["egg", "name", "area", "cost_coins", "cost_wiki", "image"])
        for e in eggs:
            w.writerow([e["number"], e["name"], e["area"], f"{e['cost']:.0f}", cost_text(e["cost_parts"]),
                        f"eggs/{slug(e['name'])}.png"])
    with open("data/egg_pets.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["egg", "pet", "chance_percent"])
        for e in eggs:
            for p in e["pets"]:
                w.writerow([e["number"], p["name"], p["chance"]])
    with open("data/pets.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["pet", "rarity", "image"])
        for name in pets:  # order of first appearance
            w.writerow([name, rarities[name], f"pets/{slug(name)}.png"])
    print(f"{len(zone_numbers)} zones, {len(eggs)} eggs, {len(pets)} pets")

    if args.no_images:
        return
    jobs = [(img, f"assets/pets/{slug(n)}.png", (96, 96)) for n, img in pets.items()]
    jobs += [(e["image"], f"assets/eggs/{slug(e['name'])}.png", (96, 96)) for e in eggs]
    jobs += [(areas[n]["image"], f"assets/zones/{slug(areas[n]['name'].replace('Chrystal', 'Crystal'))}.png",
              (128, 72)) for n in zone_numbers]
    urls = image_urls([j[0] for j in jobs])
    for i, (file, dest, size) in enumerate(jobs):
        download(urls[file], dest, size)
        print(f"\r{i + 1}/{len(jobs)} images", end="", flush=True)
    print()


if __name__ == "__main__":
    main()
