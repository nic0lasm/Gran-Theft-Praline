"""Why traffic cars cannot be placed: a report on a lanes.csv.

Counts the lanes the traffic network can place a car on (GTPTraffic.cpp,
FindSpawnPoint, and GTPFarTraffic.cpp, Place): at least 4 points and at least
one lane to go on to. With --player X Y (Unreal cm), also counts the usable
lanes with a point in the far-traffic ring (120 to 250 m) and the driven-car
ring (40 to 250 m) around that spot.

Usage: python tools/city/lanes_report.py <lanes.csv> [<other.csv>] [--player X Y]
"""
import argparse
import csv
import math
import sys


def load(path):
    lanes = []
    with open(path, newline="", encoding="utf-8") as handle:
        rows = csv.reader(handle)
        next(rows, None)
        for row in rows:
            if len(row) < 7:
                continue
            points = []
            for pair in row[5].split(";"):
                parts = pair.split()
                if len(parts) >= 2:
                    points.append((float(parts[0]), float(parts[1])))
            following = [int(x) for x in row[6].split()]
            lanes.append((points, following))
    count = len(lanes)
    return [(points, [n for n in following if 0 <= n < count]) for points, following in lanes]


def length_m(points):
    return sum(math.dist(a, b) for a, b in zip(points, points[1:])) / 100.0


def in_ring(points, centre, low_cm, high_cm):
    # Same points as the game draws from: index 1 to len - 3.
    return any(low_cm <= math.dist(p, centre) <= high_cm for p in points[1:len(points) - 2])


def report(path, player):
    lanes = load(path)
    short = sum(1 for p, _ in lanes if len(p) < 4)
    dead = sum(1 for p, n in lanes if not n)
    usable = [(p, n) for p, n in lanes if len(p) >= 4 and n]
    lengths = sorted(length_m(p) for p, _ in lanes)
    points = sorted(len(p) for p, _ in lanes)
    median = lambda values: values[len(values) // 2] if values else 0
    print(path)
    print(f"  lanes {len(lanes)}, usable {len(usable)}, under 4 points {short}, dead ends {dead}")
    print(f"  median length {median(lengths):.1f} m, median points {median(points)}, total {sum(lengths) / 1000:.2f} km")
    if player:
        far = sum(1 for p, _ in usable if in_ring(p, player, 12000, 25000))
        near = sum(1 for p, _ in usable if in_ring(p, player, 4000, 25000))
        print(f"  usable lanes in the far ring 120-250 m: {far}; in the driven ring 40-250 m: {near}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="+")
    parser.add_argument("--player", nargs=2, type=float, metavar=("X", "Y"))
    args = parser.parse_args()
    for path in args.files:
        report(path, tuple(args.player) if args.player else None)
    return 0


if __name__ == "__main__":
    sys.exit(main())
