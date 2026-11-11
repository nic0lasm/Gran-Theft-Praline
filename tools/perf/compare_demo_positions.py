#!/usr/bin/env python3
"""Compares the "DemoPos" lines of two deterministic demo runs.

  python3 tools/perf/compare_demo_positions.py run1.log run2.log [--max-cm 150] [--max-deg 10]

Lines look like (written by GTPDemoRoute with -DemoDeterministic=1):
  LogGTPDemo: DemoPos t=35.0 x=1234 y=-560 yaw=87 kmh=38 phase=drive

Two checks. The ROUTE (what matters for an A/B measurement): the sequence of lanes
("DemoLane t=.. lane=N" lines) must be the same in both runs; Chaos vehicle physics is not
bit-identical, so the exact moment a lane is entered may differ by a fraction of a second.
The POSITIONS every 5 s are a looser sanity check (default 600 cm, 45 degrees: a turn
taken 0.3 s earlier already gives tens of degrees).

Prints one row per instant with the distance (cm) and heading difference (degrees) between
the two runs, then PASS when every common instant is within the limits and both runs reach
the same last instant (within one 5 s step), else FAIL with the first instant that diverges.
Exit code 0 on PASS.
"""

import argparse
import math
import re
import sys

LANE = re.compile(r"DemoLane t=([\d.]+) lane=(-?\d+)")
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


def lanes(path):
    seq = []
    with open(path, errors="replace") as handle:
        for line in handle:
            match = LANE.search(line)
            if match:
                seq.append((float(match.group(1)), int(match.group(2))))
    return seq


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("run1")
    ap.add_argument("run2")
    ap.add_argument("--max-cm", type=float, default=600.0)
    ap.add_argument("--max-deg", type=float, default=45.0)
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
    la, lb = lanes(args.run1), lanes(args.run2)
    route_ok = True
    if la or lb:
        n = min(len(la), len(lb))
        for i in range(n):
            if la[i][1] != lb[i][1]:
                print(f"FAIL: the routes differ at lane change {i + 1}: lane {la[i][1]} (t={la[i][0]}) vs lane {lb[i][1]} (t={lb[i][0]})")
                route_ok = False
                break
        if route_ok and abs(len(la) - len(lb)) > 1:
            print(f"FAIL: run 1 took {len(la)} lanes, run 2 {len(lb)}")
            route_ok = False
        if route_ok:
            print(f"route: same lane sequence ({n} lane changes in common)")
    else:
        print("note: no DemoLane line (the car never joined a lane); only positions are compared")
    if not route_ok:
        return 1
    if first_bad is not None:
        print(f"FAIL: the runs diverge from t={first_bad} s (limits {args.max_cm:.0f} cm, {args.max_deg:.0f} degrees)")
        return 1
    print(f"PASS: {len(common)} instants within {args.max_cm:.0f} cm and {args.max_deg:.0f} degrees")
    return 0


if __name__ == "__main__":
    sys.exit(main())
