#!/usr/bin/env python3
"""Downloads the Pet Simulator 99 items and ranks used by the game from the fandom
wiki (PC-only tool, needs network access to pet-simulator.fandom.com).

    python3 tools/fetch_wiki_items.py [--ranks 12]

Writes:
    data/potions.csv       potions of the game, tiers I-III (effect %, duration)
    data/enchants.csv      enchants of the game, tiers I-III (effect %)
    data/ranks.csv         ranks (name, stars of the reward path)
    data/rank_rewards.csv  reward path of each rank, mapped to the game's items
    data/rank_goals.csv    quest goals of each rank, mapped to the game's quests
    assets/items/*.png     potion and enchant icons (64 px)
    assets/art/gold_machine.png, rainbow_machine.png

Only tiers I to III exist in the game for now: a reward of a higher tier on the wiki
becomes tier III. Rewards and goals the game does not have (fruits, flags, fishing...)
are kept in the CSV with kind "skip" (see tools/gen_data.py for what replaces them).
"""
import argparse
import csv
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fetch_wiki import download, image_urls, slug, wikitext  # noqa: E402

# game name -> wiki row name, icon file pattern (tier 1..3)
POTIONS = {
    "coins": ("Coins", "PS99-Coins-Potion-{r}.png"),
    "damage": ("Damage", "PS99-Damage-Potion-{r}.png"),
    "diamonds": ("Diamonds", "PS99-Diamonds-Potion-{r}.png"),
    "lucky": ("Lucky Eggs", "PS99-Luck-Potion-{r}.png"),
    "treasure": ("Treasure Hunter", "Treasure Hunter {n}.png"),
}
ENCHANTS = {
    "coins": ("Coins", "Coins Enchant {n}.png"),
    "diamonds": ("Diamonds", "Diamonds Enchant {n}.png"),
    "lucky": ("Lucky Eggs", "Lucky Enchant {n}.png"),
    "strong": ("Strong Pets", "Strong Pets Enchant {n}.png"),
    "tap": ("Tap Power", "Tap Power Enchant {n}.png"),
    "treasure": ("Treasure Hunter", "Treasure Enchant {n}.png"),
}
ROMAN = {"I": 1, "II": 2, "III": 3, "IV": 4, "V": 5, "VI": 6, "VII": 7, "VIII": 8, "IX": 9, "X": 10, "XI": 11}

# quest goals: regex on the wiki text -> game quest kind (N = amount)
GOALS = [
    (r"^Break (\S+) breakables in best area$", "break_best"),
    (r"^Break (\S+) breakables$", "break_any"),
    (r"^Break (\S+) diamond breakables$", "break_diamond"),
    (r"^Break (\S+) Coins$", "break_coin"),
    (r"^Break (\S+) Safes$", "break_chest"),
    (r"^Break (\S+) Presents$", "break_gift"),
    (r"^Break (\S+) mini-chests(?: in best area)?$", "break_chest"),
    (r"^Break (a) (?:superior )?mini-chest(?: in best area)?$", "break_chest"),
    # coin jars and comets are special PS99 breakables: crates and gifts here
    (r"^Break (\S+) coin jars(?: in best area)?$", "break_crate"),
    (r"^Break (a) coin jar(?: in best area)?$", "break_crate"),
    (r"^Break (\S+) comets(?: in best area)?$", "break_gift"),
    (r"^Break (a) comet(?: in best area)?$", "break_gift"),
    (r"^Earn (\S+) Diamonds$", "earn_diamonds"),
    (r"^Hatch (\S+) of your best eggs$", "hatch_best"),
    (r"^Hatch (\S+) eggs$", "hatch_any"),
    (r"^Hatch (\S+) Legendary \(or above\) Pets$", "hatch_legendary"),
    (r"^Hatch (a) Legendary \(or above\) Pet$", "hatch_legendary"),
    (r"^Make (\S+) golden pets from best egg$", "make_golden"),
    (r"^Make (\S+) rainbow pets from best egg$", "make_rainbow"),
    (r"^Unlock (an) area$", "unlock_area"),
    (r"^Collect (\S+) potions$", "collect_potion"),
    (r"^Collect (a) potion$", "collect_potion"),
    (r"^Collect (\S+) enchants$", "collect_enchant"),
    (r"^Collect (an) enchant$", "collect_enchant"),
    (r"^Use (\S+) Tier (\w+) Potions$", "use_potion"),
    (r"^Use (a) Tier (\w+) Potion$", "use_potion"),
]


def amount(text):
    if text in ("a", "an"):
        return 1
    mult = 1000 if text.lower().endswith("k") else 1
    return int(round(float(text.lower().rstrip("k").replace(",", "")) * mult))


def parse_items(page, table, kind):
    """Rows of the potions/enchants tables: name -> [(effect %, minutes)] for tiers I-III."""
    text = wikitext(page)
    rows = []
    for game, (wiki_name, icon) in table.items():
        m = re.search(r"\| rowspan=\"\d+\" \|'''" + re.escape(wiki_name) + r"'''(.*?)(?=\| rowspan=\"\d+\" \|'''|\|\})",
                      text, re.S)
        if not m:
            sys.exit(f"{kind} {wiki_name!r} not found on the wiki")
        descs = re.findall(r"(?:increases|Increases)[^\n%]*?by (\d+)%!(?:<br>\s*|\s*\n)?(?:Lasts for ([^!\n]+)!)?",
                           m.group(1))[:3]
        if len(descs) < 3:
            sys.exit(f"{kind} {wiki_name!r}: tiers I-III not found")
        for tier, (pct, lasts) in enumerate(descs, 1):
            minutes = 0
            if lasts:
                h = re.search(r"(\d+) hours?", lasts)
                mn = re.search(r"(\d+) minutes?", lasts)
                minutes = (int(h.group(1)) * 60 if h else 0) + (int(mn.group(1)) if mn else 0)
            roman = ["I", "II", "III"][tier - 1]
            rows.append({"type": game, "tier": tier, "effect_percent": int(pct), "minutes": minutes,
                         "wiki_name": wiki_name, "icon_wiki": icon.format(r=roman, n=tier),
                         "image": f"items/{kind}_{game}_{tier}.png"})
    return rows


def map_reward(name):
    """'Damage Potion II' -> ('potion', 'damage', 2); 'Bag of Diamonds' -> ('diamonds', '', 0)."""
    if name == "Bag of Diamonds":
        return "diamonds", "", 0
    m = re.match(r"^(.+?) Potion (\w+)$", name)
    if m:
        wiki = {"Coin": "coins", "Coins": "coins", "Damage": "damage", "Diamonds": "diamonds",
                "Lucky Eggs": "lucky", "Treasure Hunter": "treasure"}.get(m.group(1))
        if wiki and m.group(2) in ROMAN:
            return "potion", wiki, min(3, ROMAN[m.group(2)])
    m = re.match(r"^(.+?) (\w+)$", name)
    if m and m.group(2) in ROMAN:
        wiki = {"Coins": "coins", "Diamonds": "diamonds", "Lucky Eggs": "lucky", "Strong Pets": "strong",
                "Tap Power": "tap", "Treasure Hunter": "treasure"}.get(m.group(1))
        if wiki:
            return "enchant", wiki, min(3, ROMAN[m.group(2)])
    return "skip", "", 0


def map_goal(text):
    for pattern, kind in GOALS:
        m = re.match(pattern, text)
        if m:
            tier = 0
            if kind == "use_potion":
                tier = min(3, ROMAN.get(m.group(2), 1))
            return kind, amount(m.group(1)), tier
    return "skip", 0, 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ranks", type=int, default=12)
    ap.add_argument("--no-images", action="store_true")
    args = ap.parse_args()

    potions = parse_items("Potions (Pet Simulator 99)", POTIONS, "potion")
    enchants = parse_items("Enchants (Pet Simulator 99)", ENCHANTS, "enchant")
    os.makedirs("data", exist_ok=True)
    for name, rows in (("potions", potions), ("enchants", enchants)):
        with open(f"data/{name}.csv", "w", newline="") as f:
            fields = ["type", "tier", "effect_percent"] + (["minutes"] if name == "potions" else []) + \
                     ["wiki_name", "image"]
            w = csv.DictWriter(f, fields, extrasaction="ignore")
            w.writeheader()
            w.writerows(rows)

    ranks, rewards, goals = [], [], []
    for r in range(1, args.ranks + 1):
        text = wikitext(f"Template:Rank{r}info")
        m = re.search(r"===\s*Rank \d+:\s*([^=]+?)\s*===", text)
        total = re.search(r"'''(\d+) [Ss]tars'''", text)
        if not m or not total:
            sys.exit(f"rank {r}: name or stars not found")
        ranks.append({"rank": r, "name": m.group(1).strip(), "stars_total": int(total.group(1))})
        for num, name, stars, qty in re.findall(r"^\|(\d+)\|\|([^|]+?)\s*\|\|(\d+)\|\|[^|]*\|\|([\d,]+)", text, re.M):
            kind, typ, tier = map_reward(name.strip())
            rewards.append({"rank": r, "index": int(num), "stars": int(stars), "reward_wiki": name.strip(),
                            "quantity": int(qty.replace(",", "")), "kind": kind, "type": typ, "tier": tier})
        pool = text[text.find("Goal Pool"):]
        header = pool[:pool.find("|-")]
        tiers = len(re.findall(r"Tier \d\.png", header))
        cells = pool[pool.find("|-") + 2:].split("\n|\n")
        cells = [c for c in cells if "*" in c][:tiers]
        for star, cell in enumerate(cells, 1):
            for g in re.findall(r"^\*\s*(.+?)\s*$", cell, re.M):
                kind, n, tier = map_goal(g)
                goals.append({"rank": r, "stars": star, "goal_wiki": g, "kind": kind, "amount": n,
                              "potion_tier": tier})
    for name, rows in (("ranks", ranks), ("rank_rewards", rewards), ("rank_goals", goals)):
        with open(f"data/{name}.csv", "w", newline="") as f:
            w = csv.DictWriter(f, list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
    skipped = sum(1 for g in goals if g["kind"] == "skip")
    print(f"{len(potions)} potions, {len(enchants)} enchants, {len(ranks)} ranks, "
          f"{len(rewards)} rewards ({sum(1 for x in rewards if x['kind'] == 'skip')} not in the game), "
          f"{len(goals)} goals ({skipped} not in the game)")

    if args.no_images:
        return
    jobs = [(p["icon_wiki"], "assets/" + p["image"], (64, 64)) for p in potions + enchants]
    jobs += [("PS99 Icon Gold Machine.png", "assets/art/gold_machine.png", (96, 96)),
             ("PS99 Icon Rainbow Machine.png", "assets/art/rainbow_machine.png", (96, 96))]
    urls = image_urls([j[0] for j in jobs])
    for file, dest, size in jobs:
        download(urls[file], dest, size)
    print(f"{len(jobs)} images")


if __name__ == "__main__":
    main()
