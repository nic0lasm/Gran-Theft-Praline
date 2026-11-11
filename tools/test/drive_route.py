"""Turns a named test drive into a timed path the game can replay (-TestDrive).

A test drive is a list of waypoints (Unreal metres, x y) in tools/test/scenarios.json.
This script snaps each waypoint to the nearest driving lane of the map
(Content/City/Traffic/<Map>.lanes.csv, from tools/city/traffic.py), links them by
the shortest way the lanes allow (one-way streets, no U-turn), then gives every
point a speed (lane limit, bends, accelerating from and braking to a stop) and the
time at which the car passes it. The game only reads this file: the same file gives
the same drive on every build, every machine and every frame rate.

Output CSV (one row per point): t_s,x_cm,y_cm,speed_kmh
With --svg, a top view of the lanes and the route, for a quick check.

  python3 tools/test/drive_route.py --lanes <map.lanes.csv> --scenario <name> --out <path.csv> [--svg <view.svg>]
  python3 tools/test/drive_route.py --lanes <map.lanes.csv> --list
  python3 tools/test/drive_route.py --lanes <map.lanes.csv> --longest --out <path.csv>

Exit code 0 when the route is valid, 2 when a waypoint is off the lanes or cannot
be reached from the previous one (the message says which).
"""

import argparse
import csv
import heapq
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SCENARIOS = os.path.join(HERE, "scenarios.json")
SNAP_M = 30.0          # a waypoint farther than this from every lane is an error
LATERAL_ACCEL = 2.2    # m/s2 in bends, as FGTPDriveSettings::MaxLateralAccel
ACCEL = 2.0            # m/s2 from a stop
DECEL = 3.5            # m/s2, as FGTPDriveSettings::ComfortDecel
MIN_BEND_SPEED = 2.0   # m/s, never crawl slower than this in a bend


class RouteError(Exception):
    pass


def load_lanes(path):
    """lanes.csv -> list of dicts {points: [(x_cm, y_cm)], next: [ids], speed: km/h}."""
    lanes = []
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            pts = []
            for p in row["points_ue_cm"].split(";"):
                xy = p.split()
                if len(xy) == 2:
                    pts.append((float(xy[0]), float(xy[1])))
            nxt = [int(n) for n in (row.get("next") or "").split()]
            lanes.append({"points": pts, "next": nxt, "speed": float(row["speed_kmh"] or 30)})
    n = len(lanes)
    for lane in lanes:
        lane["next"] = [i for i in lane["next"] if 0 <= i < n]
    return lanes


def lane_length(lane):
    p = lane["points"]
    return sum(math.dist(p[i], p[i + 1]) for i in range(len(p) - 1))


def shortest_lanes(lanes, start, goal):
    """Lane ids from start to goal along 'next' links (start and goal included), None if unreachable."""
    if start == goal:
        return [start]
    lengths = [lane_length(l) for l in lanes]
    dist = {start: 0.0}
    prev = {}
    heap = [(0.0, start)]
    while heap:
        d, lane = heapq.heappop(heap)
        if lane == goal:
            break
        if d > dist.get(lane, float("inf")):
            continue
        for n in lanes[lane]["next"]:
            nd = d + lengths[n]
            if nd < dist.get(n, float("inf")):
                dist[n] = nd
                prev[n] = lane
                heapq.heappush(heap, (nd, n))
    if goal not in dist:
        return None
    chain = [goal]
    while chain[-1] != start:
        chain.append(prev[chain[-1]])
    return chain[::-1]


def leg(lanes, a, b):
    """Points (cm) with their lane speed, from snapped waypoint a=(lane, idx) to b."""
    (la, pa), (lb, pb) = a, b
    if la == lb and pb >= pa:
        return [(p, lanes[la]["speed"]) for p in lanes[la]["points"][pa:pb + 1]]
    # Leave the lane we are on, come back to it if needed (a loop on the same lane).
    if la == lb:
        chain = None
        for n in lanes[la]["next"]:
            c = shortest_lanes(lanes, n, lb)
            if c and (chain is None or len(c) < len(chain)):
                chain = [la] + c
    else:
        chain = shortest_lanes(lanes, la, lb)
    if not chain:
        return None
    out = []
    for i, lane in enumerate(chain):
        pts = lanes[lane]["points"]
        lo = pa if i == 0 else 0
        hi = pb + 1 if i == len(chain) - 1 else len(pts)
        out.extend((p, lanes[lane]["speed"]) for p in pts[lo:hi])
    return out


def candidates(lanes, x_cm, y_cm, tie_m=3.0):
    """Lane points near a waypoint: the nearest one and any within tie_m metres of it
    (a junction point is the end of one lane and the start of others)."""
    found = []
    for li, lane in enumerate(lanes):
        best = min(((math.dist(p, (x_cm, y_cm)), pi) for pi, p in enumerate(lane["points"])), default=None)
        if best:
            found.append((best[0] / 100.0, li, best[1]))
    found.sort()
    if not found:
        return []
    return [(li, pi, d) for d, li, pi in found if d <= found[0][0] + tie_m]


def build_path(lanes, waypoints_m, max_kmh=None, loop=False):
    """Waypoints in metres -> list of (x_cm, y_cm, limit m/s). Raises RouteError."""
    if len(waypoints_m) < 2:
        raise RouteError("a drive needs at least two waypoints")
    cands = []
    for i, (x, y) in enumerate(waypoints_m):
        c = candidates(lanes, x * 100.0, y * 100.0)
        if not c or c[0][2] > SNAP_M:
            d = c[0][2] if c else float("inf")
            raise RouteError(f"waypoint {i} ({x:.0f}, {y:.0f}) m is {d:.0f} m from the nearest lane (max {SNAP_M:.0f})")
        cands.append([(li, pi) for li, pi, d in c if d <= SNAP_M])
    if loop:
        cands.append(cands[0])
    # Cheapest chain of legs over the candidates of each waypoint (few candidates: plain DP).
    best = {c: (0.0, []) for c in cands[0]}
    for i in range(1, len(cands)):
        nxt = {}
        for b in cands[i]:
            for a, (cost, legs) in best.items():
                part = leg(lanes, a, b)
                if part is None:
                    continue
                length = sum(math.dist(part[k][0], part[k + 1][0]) for k in range(len(part) - 1))
                if b not in nxt or cost + length < nxt[b][0]:
                    nxt[b] = (cost + length, legs + [(a, part)])
        if not nxt:
            raise RouteError(f"waypoint {i % len(waypoints_m)} cannot be reached from waypoint {i - 1} along the lanes (one way, dead end or no link)")
        best = nxt
    _, legs = min(best.values(), key=lambda v: v[0])
    pts = []
    for _, part in legs:
        if pts and part and math.dist(pts[-1][0], part[0][0]) < 1.0:
            part = part[1:]
        pts.extend(part)
    out = []
    for (x, y), kmh in pts:
        if out and math.dist(out[-1][:2], (x, y)) < 1.0:
            continue
        lim = kmh if max_kmh is None else min(kmh, max_kmh)
        out.append((x, y, lim / 3.6))
    if len(out) < 2:
        raise RouteError("the route is shorter than two points")
    return out


def timed(path, stop_at_end=True):
    """Adds bend limits, acceleration and braking; returns rows (t_s, x_cm, y_cm, speed_kmh)."""
    n = len(path)
    v = [p[2] for p in path]
    for i in range(1, n - 1):
        a, b, c = path[i - 1][:2], path[i][:2], path[i + 1][:2]
        h1 = math.atan2(b[1] - a[1], b[0] - a[0])
        h2 = math.atan2(c[1] - b[1], c[0] - b[0])
        turn = abs((h2 - h1 + math.pi) % (2 * math.pi) - math.pi)
        seg = (math.dist(a, b) + math.dist(b, c)) / 200.0
        if turn > 1e-3:
            radius = seg / turn
            v[i] = min(v[i], max(MIN_BEND_SPEED, math.sqrt(LATERAL_ACCEL * radius)))
    v[0] = 0.0
    if stop_at_end:
        v[-1] = 0.0
    seg = [math.dist(path[i][:2], path[i + 1][:2]) / 100.0 for i in range(n - 1)]
    for i in range(1, n):
        v[i] = min(v[i], math.sqrt(v[i - 1] ** 2 + 2 * ACCEL * seg[i - 1]))
    for i in range(n - 2, -1, -1):
        v[i] = min(v[i], math.sqrt(v[i + 1] ** 2 + 2 * DECEL * seg[i]))
    rows = [(0.0, path[0][0], path[0][1], 0.0)]
    t = 0.0
    for i in range(1, n):
        mean = (v[i - 1] + v[i]) / 2.0
        t += seg[i - 1] / max(mean, 0.1)
        rows.append((t, path[i][0], path[i][1], v[i] * 3.6))
    return rows


def longest_route(lanes, max_lanes=60):
    """Waypoints (m) of the longest simple chain of lanes: a drive that needs no map knowledge."""
    lengths = [lane_length(l) for l in lanes]
    best_len, best_chain = -1.0, []
    sys.setrecursionlimit(10000)

    def walk(chain, total, seen):
        nonlocal best_len, best_chain
        if total > best_len:
            best_len, best_chain = total, list(chain)
        if len(chain) >= max_lanes:
            return
        for n in lanes[chain[-1]]["next"]:
            if n not in seen:
                seen.add(n)
                chain.append(n)
                walk(chain, total + lengths[n], seen)
                chain.pop()
                seen.discard(n)

    for s in range(len(lanes)):
        if lanes[s]["points"]:
            walk([s], lengths[s], {s})
    first = lanes[best_chain[0]]["points"][0]
    last = lanes[best_chain[-1]]["points"][-1]
    mids = [lanes[l]["points"][len(lanes[l]["points"]) // 2] for l in best_chain]
    wps = [first] + mids + [last]
    return [(x / 100.0, y / 100.0) for x, y in wps]


def write_csv(rows, path):
    with open(path, "w", newline="", encoding="utf-8") as f:
        f.write("t_s,x_cm,y_cm,speed_kmh\n")
        for t, x, y, s in rows:
            f.write(f"{t:.3f},{x:.1f},{y:.1f},{s:.1f}\n")


def write_svg(lanes, rows, waypoints_m, path):
    xs = [p[0] for l in lanes for p in l["points"]] + [r[1] for r in rows]
    ys = [p[1] for l in lanes for p in l["points"]] + [r[2] for r in rows]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    w = 600.0
    k = w / max(x1 - x0, 1.0)
    h = (y1 - y0) * k
    pt = lambda x, y: f"{(x - x0) * k + 10:.1f},{(y - y0) * k + 10:.1f}"
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w + 20:.0f}" height="{h + 20:.0f}" style="background:#181818">']
    for l in lanes:
        out.append(f'<polyline fill="none" stroke="#777" stroke-width="1" points="{" ".join(pt(*p) for p in l["points"])}"/>')
    out.append(f'<polyline fill="none" stroke="#ffb000" stroke-width="3" points="{" ".join(pt(r[1], r[2]) for r in rows)}"/>')
    for i, (x, y) in enumerate(waypoints_m):
        out.append(f'<circle cx="{pt(x * 100, y * 100).split(",")[0]}" cy="{pt(x * 100, y * 100).split(",")[1]}" r="5" fill="#3fa9f5"/>')
    out.append("</svg>")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(out))


def load_scenarios(path=SCENARIOS):
    with open(path, encoding="utf-8") as f:
        return {s["name"]: s for s in json.load(f)["drives"]}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lanes", required=True, help="Content/City/Traffic/<Map>.lanes.csv")
    ap.add_argument("--scenario", help="name in scenarios.json")
    ap.add_argument("--scenarios", default=SCENARIOS)
    ap.add_argument("--longest", action="store_true", help="drive the longest chain of lanes of the map")
    ap.add_argument("--list", action="store_true", help="check every drive of scenarios.json against these lanes")
    ap.add_argument("--out", help="timed path CSV for -TestDrive")
    ap.add_argument("--svg", help="top view of the route")
    args = ap.parse_args(argv)
    lanes = load_lanes(args.lanes)
    if not lanes:
        print(f"no lane in {args.lanes}", file=sys.stderr)
        return 2
    if args.list:
        bad = 0
        for name, s in load_scenarios(args.scenarios).items():
            try:
                rows = timed(build_path(lanes, s["waypoints_m"], s.get("max_kmh"), s.get("loop", False)), True)
                print(f"OK   {name}: {rows[-1][0]:.0f} s, {sum(math.dist(rows[i][1:3], rows[i + 1][1:3]) for i in range(len(rows) - 1)) / 100:.0f} m")
            except RouteError as e:
                bad += 1
                print(f"FAIL {name}: {e}")
        return 2 if bad else 0
    if args.longest:
        wps, max_kmh, loop = longest_route(lanes), None, False
    elif args.scenario:
        s = load_scenarios(args.scenarios).get(args.scenario)
        if not s:
            print(f"unknown drive '{args.scenario}'", file=sys.stderr)
            return 2
        wps, max_kmh, loop = s["waypoints_m"], s.get("max_kmh"), s.get("loop", False)
    else:
        ap.error("--scenario, --longest or --list")
    try:
        rows = timed(build_path(lanes, wps, max_kmh, loop))
    except RouteError as e:
        print(f"FAIL: {e}", file=sys.stderr)
        return 2
    if args.out:
        write_csv(rows, args.out)
    if args.svg:
        write_svg(lanes, rows, wps, args.svg)
    length = sum(math.dist(rows[i][1:3], rows[i + 1][1:3]) for i in range(len(rows) - 1)) / 100.0
    print(f"OK: {len(rows)} points, {length:.0f} m, {rows[-1][0]:.1f} s, top {max(r[3] for r in rows):.0f} km/h")
    return 0


if __name__ == "__main__":
    sys.exit(main())
