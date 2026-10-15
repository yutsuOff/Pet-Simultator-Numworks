#!/usr/bin/env python3
"""Tunes the coins given by the breakables of each zone so that the whole game takes
about TOTAL_HOURS for the balance bot (tests/balance_sim.c), early zones being shorter
than the last ones. Writes data/balance.csv, then regenerates src/generated/content.c.

    python3 tools/calibrate_balance.py

Each round: run the bot, compare the time spent in each zone with its target, scale the
zone's rewards accordingly. A few rounds are enough (zones only slightly interact).
"""
import csv
import subprocess
import sys

TOTAL_HOURS = 6.0
FIRST_ZONE_MINUTES = 4.0     # target time in zone 1; later zones ramp up linearly
ROUNDS = 12
ZONES = 37


def targets():
    n = ZONES - 1  # zone purchases
    total = TOTAL_HOURS * 3600
    first = FIRST_ZONE_MINUTES * 60
    last = 2 * total / n - first
    return {z: first + (last - first) * z / (n - 1) for z in range(n)}


def write(scale):
    with open("data/balance.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["zone", "reward_scale"])
        for z in sorted(scale):
            w.writerow([z, f"{scale[z]:.6g}"])


def run():
    subprocess.run([sys.executable, "tools/gen_data.py"], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["make", "-s", "output/balance_sim"], check=True)
    out = subprocess.run(["output/balance_sim", "--csv"], check=True, capture_output=True, text=True).stdout
    times = {}
    for line in out.split():
        a, b = line.split(",")
        if a != "total":
            times[int(a) - 1] = float(b)  # time spent in zone a-1 before buying zone a
    return times


def main():
    target = targets()
    scale = {z: 1.0 for z in target}
    try:
        with open("data/balance.csv") as f:
            scale.update({int(r["zone"]): float(r["reward_scale"]) for r in csv.DictReader(f)})
    except FileNotFoundError:
        pass
    for r in range(ROUNDS):
        write(scale)
        times = run()
        total = sum(times.values())
        missing = [z for z in target if z not in times]
        print(f"round {r + 1}: {total / 3600:.2f} h" + (f", zones {missing} not reached" if missing else ""))
        for z in target:
            got = times.get(z, 3600 * 4)
            factor = max(0.25, min(4.0, got / target[z]))
            scale[z] *= factor ** 0.8
    write(scale)
    subprocess.run([sys.executable, "tools/gen_data.py"], check=True)


if __name__ == "__main__":
    main()
