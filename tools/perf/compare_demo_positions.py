#!/usr/bin/env python3
"""Compares the "DemoPos" lines of two deterministic demo runs.

  python3 tools/perf/compare_demo_positions.py run1.log run2.log [--max-cm 150] [--max-deg 10]

Lines look like (written by GTPDemoRoute with -DemoDeterministic=1):
  LogGTPDemo: DemoPos t=35.0 x=1234 y=-560 yaw=87 kmh=38 phase=drive

Prints one row per instant with the distance (cm) and heading difference (degrees) between
the two runs, then PASS when every common instant is within the limits and both runs reach
the same last instant (within one 5 s step), else FAIL with the first instant that diverges.
Exit code 0 on PASS.
"""

import argparse
import math
import re
import sys

LINE = re.compile(r"DemoPos t=([\d.]+) x=(-?\d+) y=(-?\d+) yaw=(-?\d+) kmh=(-?\d+) phase=([\w ]+)")


def read(path):
    rows = {}
    with open(path, errors="replace") as handle:
        for line in handle:
            match = LINE.search(line)
            if match:
                t = float(match.group(1))
                rows[t] = (int(match.group(2)), int(match.group(3)), int(match.group(4)), int(match.group(5)), match.group(6).strip())
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("run1")
    ap.add_argument("run2")
    ap.add_argument("--max-cm", type=float, default=150.0)
    ap.add_argument("--max-deg", type=float, default=10.0)
    args = ap.parse_args()
    a, b = read(args.run1), read(args.run2)
    if not a or not b:
        print("FAIL: no DemoPos line in", args.run1 if not a else args.run2)
        return 1
    common = sorted(set(a) & set(b))
    first_bad = None
    print(f"{'t':>6} {'dist cm':>9} {'dyaw':>6}  run1 phase / run2 phase")
    for t in common:
        ax, ay, ayaw, _, aphase = a[t]
        bx, by, byaw, _, bphase = b[t]
        dist = math.hypot(ax - bx, ay - by)
        dyaw = abs((ayaw - byaw + 180) % 360 - 180)
        bad = dist > args.max_cm or dyaw > args.max_deg or aphase != bphase
        if bad and first_bad is None:
            first_bad = t
        print(f"{t:6.1f} {dist:9.0f} {dyaw:6.0f}  {aphase} / {bphase}{'  <-- diverges' if bad else ''}")
    last_a, last_b = max(a), max(b)
    if abs(last_a - last_b) > 5.0:
        print(f"FAIL: run 1 ends at t={last_a}, run 2 at t={last_b}")
        return 1
    if first_bad is not None:
        print(f"FAIL: the runs diverge from t={first_bad} s (limits {args.max_cm:.0f} cm, {args.max_deg:.0f} degrees)")
        return 1
    print(f"PASS: {len(common)} instants within {args.max_cm:.0f} cm and {args.max_deg:.0f} degrees")
    return 0


if __name__ == "__main__":
    sys.exit(main())
