#!/usr/bin/env python3
"""Parked car slots along the kerbs, from the measured carriageways.

A slot is kept where the carriageway leaves room for a parked car between the
outer traffic lane (traffic.py, same offsets) and the kerb, away from the
junctions. Two-way streets park on the right of each direction; one-way
streets on both sides. About PARK_SHARE of the slots get a car, the same ones
at every run (seeded by the road id).

Inputs: the zone's roads.geojson and carriageway.geojson, the focus area of
gen/<focus>/buildings_gen.csv (same as traffic.py).
Output: gen/<focus>/traffic/parking.csv (x_cm, y_cm, yaw_deg, road_id), and with
--map <MapName> a copy at GranTheftPraline/Content/City/Traffic/<MapName>.parking.csv
(not committed), read by AGTPParkedCars (-ParkedCars=1).
"""
import argparse
import csv
import json
import math
import os
import random
import shutil
import zlib

from shapely.geometry import Point

import traffic

PARK_KERB_M = 1.05     # parked car centre from the kerb (half a car plus a hand)
PARK_NEED_M = 2.1      # free room between the outer lane edge and the kerb
SLOT_M = 5.6           # a car and the gap to the next one
JUNCTION_CLEAR_M = 15.0
PARK_SHARE = 0.7


def side_slots(centre, half, offset_of, side, road_length, facing):
    """Slots (x, y, heading radians) along one kerb. side +1 = right of the
    drawing direction of the road, -1 = left; facing +1 = cars point along the
    drawing direction, -1 = against it. offset_of(i) is the offset of the outer
    lane on that side at point i (traffic.py, its sign does not matter here)."""
    slots = []
    travelled = [0.0]
    for (ax, ay), (bx, by) in zip(centre, centre[1:]):
        travelled.append(travelled[-1] + math.hypot(bx - ax, by - ay))
    next_at = JUNCTION_CLEAR_M
    for i in range(len(centre)):
        s = travelled[i]
        if s < next_at or s > road_length - JUNCTION_CLEAR_M:
            continue
        lane_edge = abs(offset_of(i)) + traffic.LANE_W_M / 2.0
        if half[i] - lane_edge < PARK_NEED_M:
            continue
        (ax, ay), (bx, by) = centre[max(i - 1, 0)], centre[min(i + 1, len(centre) - 1)]
        dx, dy = bx - ax, by - ay
        length = math.hypot(dx, dy) or 1.0
        rx, ry = dy / length, -dx / length  # right-hand normal (x east, y north)
        off = side * (half[i] - PARK_KERB_M)
        x, y = centre[i][0] + rx * off, centre[i][1] + ry * off
        heading = math.atan2(dy, dx) if facing > 0 else math.atan2(-dy, -dx)
        slots.append((x, y, heading))
        next_at = s + SLOT_M
    return slots


def build_slots(features, carriageways, area=None, share=PARK_SHARE):
    out = []
    for feat in features:
        p = feat["properties"]
        if not traffic.is_car_road(p):
            continue
        road = p.get("cleabs") or ""
        measured = carriageways.get(road)
        if measured is None:
            continue
        centre, half = measured
        if len(centre) < 2:
            continue
        dirs = traffic.directions(p)
        two_way = len(dirs) == 2
        total = max(1, int(p.get("nombre_de_voies") or 1))
        per_dir = max(1, total // len(dirs)) if two_way else total
        length = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(centre, centre[1:]))
        found = []
        if two_way:
            # Right of each direction: the outer lane of that direction.
            outer = lambda i: traffic.measured_offset(per_dir - 1, per_dir, True, half[i])
            # Right of the drawing direction is the kerb of the +1 direction, and the other way round.
            found += side_slots(centre, half, outer, +1, length, +1)
            found += side_slots(centre, half, outer, -1, length, -1)
        else:
            # One way: both kerbs, every car facing the way the traffic goes.
            d = dirs[0]
            right = lambda i: traffic.measured_offset(per_dir - 1, per_dir, False, half[i])
            left = lambda i: traffic.measured_offset(0, per_dir, False, half[i])
            found += side_slots(centre, half, right if d == 1 else left, +1, length, d)
            found += side_slots(centre, half, left if d == 1 else right, -1, length, d)
        rng = random.Random(zlib.crc32(road.encode("utf-8")))
        for x, y, heading in found:
            if area is not None and not area.contains(Point(x, y)):
                continue
            if rng.random() < share:
                out.append((x, y, heading, road))
    return out


def write(slots, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["x_cm", "y_cm", "yaw_deg", "road_id"])
        for x, y, heading, road in slots:
            # Unreal: x east, y south (cm); yaw from +X towards +Y.
            w.writerow([f"{x * 100.0:.0f}", f"{-y * 100.0:.0f}", f"{math.degrees(-heading):.1f}", road])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--zone", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--map", help="map name: copy the slots into the game for this map")
    ap.add_argument("--share", type=float, default=PARK_SHARE, help="share of the slots that get a car")
    args = ap.parse_args()
    base = os.path.join(traffic.REPO, "assets", "geo", "processed", args.zone)
    gen = os.path.join(base, "gen", args.focus)
    with open(os.path.join(base, "roads.geojson"), encoding="utf-8") as f:
        roads = json.load(f)["features"]
    carriageways = traffic.load_carriageways(os.path.join(base, "carriageway.geojson"))
    slots = build_slots(roads, carriageways, traffic.focus_area_m(gen), args.share)
    out = os.path.join(gen, "traffic", "parking.csv")
    write(slots, out)
    meta = {"parked_cars": len(slots), "file": out}
    if args.map:
        dest = os.path.join(traffic.REPO, "GranTheftPraline", "Content", "City", "Traffic", args.map + ".parking.csv")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(out, dest)
        meta["installed"] = dest
    print(json.dumps(meta, indent=2))


if __name__ == "__main__":
    main()
