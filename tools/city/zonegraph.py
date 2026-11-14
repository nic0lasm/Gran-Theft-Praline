"""ZoneGraph shapes for Mass (ADR-0015): the driving lanes and the walkways of a focus street,
written as a table the Unreal Editor turns into ZoneShape actors (unreal/place_zonegraph.py).
The map is never edited by hand: run_city_import.sh rebuilds it, ZoneGraph included.

Inputs (not committed, see traffic.py and pedestrians.py):
  gen/<focus>/traffic/lanes.csv          one-way driving lanes, Unreal cm, next lanes, junction ids
  gen/<focus>/pedestrians/walkways.csv   undirected walkway edges, Unreal cm, kind
Output (not committed): gen/<focus>/zonegraph/zonegraph.json

Shapes:
- every driving lane is a spline with the one-way "Car" profile (tag Car); where it meets a
  junction (three roads or more, traffic.py) it is cut JUNCTION_TRIM_M short;
- every junction is a polygon whose points are those cut ends, sorted around its centre:
  "in" for a lane arriving, "out" for a lane leaving. ZoneGraph routes the turns inside the
  polygon (tag Car + Intersection), as MassTraffic expects;
- lanes that only continue (no junction id) touch end to start and ZoneGraph links them;
- a lane that leads nowhere (the edge of the focus area, a junction with one end) is a dead end,
  where MassTraffic's cars stop for good (it expects none: "Should never happen" in its lane
  choice). Where a lane fed by nothing starts within UTURN_MAX_M the other way (the other side of
  a two-way road), a turnaround spline (tag Car) joins the dead end to it: the cars turn back at
  the edge of the map. Before that, a dead end whose lane has a next lane in lanes.csv whose start
  does not touch it (lanes of two roads meeting off a junction, offset differently) is joined to it:
  its last JOIN_SHIFT_M are redrawn to end on that start. Counts: dead_ends_joined, dead_ends_closed, dead_ends_left, and of those left
  dead_ends_left_with_next (lanes.csv says where they go, too far to join);
- every walkway edge is a spline with the two-way "Pedestrian" profile (tag Pedestrian),
  crossings add the tag Crossing.

Frame: Unreal cm, X east, Y south (as lanes.csv). No Z: the editor drops points on the ground.

  python3 tools/city/zonegraph.py [--zone presqu-ile] [--focus rue-de-la-republique]
      [--lanes <lanes.csv>] [--walkways <walkways.csv>]
      --lanes / --walkways take other tables of the same columns, e.g. the clipped slice of
      tools/city/npc_data.py (pnj-data), instead of the ones of gen/<focus>.
"""

import argparse
import csv
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

JUNCTION_TRIM_M = 6.0      # a lane stops this far before the junction node
MIN_KEPT_M = 2.0           # a cut lane keeps at least this much
CAR_LANE_WIDTH_CM = 320.0
UTURN_MAX_M = 12.0         # a dead end turns back onto a lane starting this close, the other way
UTURN_MIN_TURN_DEG = 150.0 # "the other way": headings this far apart
UTURN_OVERSHOOT_M = 3.0    # the turnaround runs on this far past the dead end
UTURN_MIN_RADIUS_M = 2.5
UTURN_SPEED_KMH = 15.0
JOIN_EPS_CM = 1.0
JOIN_MAX_M = 10.0          # a lane whose next lane (lanes.csv) starts this close is joined to it
JOIN_SHIFT_M = 6.0         # over this last stretch of the lane
KIND_CROSSING = 2
PROFILES = {
    # Lane profiles the editor script creates in the ZoneGraph settings.
    "Car": {"lanes": [{"width_cm": CAR_LANE_WIDTH_CM, "direction": "forward", "tags": ["Car"]}]},
    "Pedestrian": {"lanes": [{"width_cm": 150.0, "direction": "forward", "tags": ["Pedestrian"]},
                             {"width_cm": 150.0, "direction": "backward", "tags": ["Pedestrian"]}]},
    "Crossing": {"lanes": [{"width_cm": 150.0, "direction": "forward", "tags": ["Pedestrian", "Crossing"]},
                           {"width_cm": 150.0, "direction": "backward", "tags": ["Pedestrian", "Crossing"]}]},
}
TAGS = ["Car", "Pedestrian", "Crossing", "Intersection"]


def parse_points(text):
    pts = []
    for pair in text.split(";"):
        pair = pair.strip()
        if pair:
            x, y = pair.split()[:2]
            pts.append((float(x), float(y)))
    return pts


def read_lanes(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    lanes = []
    for r in rows:
        pts = parse_points(r["points_ue_cm"])
        if len(pts) < 2:
            continue
        lanes.append({"id": int(r["lane_id"]), "points": pts, "speed_kmh": float(r.get("speed_kmh") or 30.0),
                      "next": [int(n) for n in (r.get("next") or "").split()],
                      "from_junction": int(r.get("from_junction") or -1), "to_junction": int(r.get("to_junction") or -1)})
    return lanes


def read_walkways(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    edges = []
    for r in rows:
        pts = parse_points(r["points_ue_cm"])
        if len(pts) >= 2:
            edges.append({"id": r["edge_id"], "kind": int(r["kind"]), "width_m": float(r.get("width_m") or 3.0), "points": pts})
    return edges


def length(pts):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:]))


def cut_start(pts, dist):
    """The polyline without its first dist (same units), the new first point interpolated."""
    left = dist
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        seg = math.hypot(b[0] - a[0], b[1] - a[1])
        if seg > left:
            t = left / seg if seg > 0 else 0.0
            return [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)] + list(pts[i + 1:])
        left -= seg
    return list(pts[-2:])


def cut_end(pts, dist):
    return list(reversed(cut_start(list(reversed(pts)), dist)))


def heading(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def build(lanes, walkways):
    """Shapes (dicts) and counts, see the module docstring."""
    shapes = []
    junctions = {}
    for lane in lanes:
        pts = lane["points"]
        total = length(pts) / 100.0
        trim_start = lane["from_junction"] >= 0
        trim_end = lane["to_junction"] >= 0
        cuts = (1 if trim_start else 0) + (1 if trim_end else 0)
        # A short lane between two junctions keeps MIN_KEPT_M in its middle.
        trim = min(JUNCTION_TRIM_M, max(0.0, (total - MIN_KEPT_M) / cuts)) if cuts else 0.0
        if trim_start and trim > 0:
            pts = cut_start(pts, trim * 100.0)
        if trim_end and trim > 0:
            pts = cut_end(pts, trim * 100.0)
        shapes.append({"name": "Car_%d" % lane["id"], "type": "spline", "profile": "Car", "tags": ["Car"],
                       "points": [[round(x, 1), round(y, 1)] for x, y in pts], "speed_kmh": lane["speed_kmh"]})
        if trim_end:
            junctions.setdefault(lane["to_junction"], []).append(("in", pts[-1], heading(pts[-2], pts[-1]), lane["id"]))
        if trim_start:
            junctions.setdefault(lane["from_junction"], []).append(("out", pts[0], heading(pts[0], pts[1]), lane["id"]))
    polygons = 0
    emitted = set()
    for jid in sorted(junctions):
        ends = junctions[jid]
        if len(ends) < 2:
            continue
        emitted.add(jid)
        cx = sum(p[1][0] for p in ends) / len(ends)
        cy = sum(p[1][1] for p in ends) / len(ends)
        ends.sort(key=lambda p: math.atan2(p[1][1] - cy, p[1][0] - cx))
        shapes.append({"name": "Junction_%d" % jid, "type": "polygon", "profile": "Car", "tags": ["Car", "Intersection"],
                       "points": [[round(p[1][0], 1), round(p[1][1], 1)] for p in ends],
                       "roles": [p[0] for p in ends], "yaw_deg": [round(p[2], 1) for p in ends],
                       "lanes": [p[3] for p in ends], "centre": [round(cx, 1), round(cy, 1)]})
        polygons += 1
    dead_counts = close_dead_ends(lanes, shapes, emitted)
    crossings = 0
    for e in walkways:
        crossing = e["kind"] == KIND_CROSSING
        crossings += 1 if crossing else 0
        shapes.append({"name": "Walk_%s" % e["id"], "type": "spline", "profile": "Crossing" if crossing else "Pedestrian",
                       "tags": ["Pedestrian", "Crossing"] if crossing else ["Pedestrian"],
                       "points": [[round(x, 1), round(y, 1)] for x, y in e["points"]], "width_cm": round(e["width_m"] * 100.0, 1)})
    counts = {"car_lanes": len(lanes), "junctions": polygons, "walkways": len(walkways), "crossings": crossings,
              **dead_counts, "shapes": len(shapes)}
    return shapes, counts


def close_dead_ends(lanes, shapes, emitted):
    """Joins each dead-end lane to its next lane of lanes.csv when that one starts within JOIN_MAX_M
    and is fed by nothing (a short spline), else adds a turnaround spline to a lane fed by nothing
    that starts within UTURN_MAX_M the other way (nearest first, each start used once). Returns the
    counts dead_ends_joined, dead_ends_closed, dead_ends_left, dead_ends_left_with_next."""
    cars = {s["name"]: s for s in shapes if s["type"] == "spline" and s["profile"] == "Car"}
    by_lane = [(lane, cars["Car_%d" % lane["id"]]) for lane in lanes if "Car_%d" % lane["id"] in cars]
    starts = [tuple(c["points"][0]) for _, c in by_lane]
    ends = [tuple(c["points"][-1]) for _, c in by_lane]

    def near(p, q):
        return math.hypot(p[0] - q[0], p[1] - q[1]) <= JOIN_EPS_CM

    dead, entries = [], []
    for i, (lane, car) in enumerate(by_lane):
        if not (lane["to_junction"] in emitted or any(near(ends[i], s) for j, s in enumerate(starts) if j != i)):
            dead.append(i)
        if not (lane["from_junction"] in emitted or any(near(starts[i], e) for j, e in enumerate(ends) if j != i)):
            entries.append(i)
    index_of = {lane["id"]: i for i, (lane, _) in enumerate(by_lane)}
    used_dead, used_entry = set(), set()
    joined = 0
    for i in dead:
        lane_i, car_i = by_lane[i]
        options = []
        for nid in lane_i["next"]:
            j = index_of.get(nid)
            if j is None or j == i or j not in entries or j in used_entry:
                continue
            gap = math.hypot(starts[j][0] - ends[i][0], starts[j][1] - ends[i][1])
            if gap <= JOIN_MAX_M * 100.0:
                options.append((gap, j))
        if options:
            _, j = min(options)
            used_dead.add(i)
            used_entry.add(j)
            car_i["points"] = joined_points(car_i["points"], by_lane[j][1]["points"][0])
            joined += 1
    pairs = []
    for i in dead:
        if i in used_dead:
            continue
        pi = by_lane[i][1]["points"]
        h_in = heading(pi[-2], pi[-1])
        for j in entries:
            if j == i:
                continue
            pj = by_lane[j][1]["points"]
            gap = math.hypot(pj[0][0] - pi[-1][0], pj[0][1] - pi[-1][1])
            turn = abs((heading(pj[0], pj[1]) - h_in + 180.0) % 360.0 - 180.0)
            if gap <= UTURN_MAX_M * 100.0 and turn >= UTURN_MIN_TURN_DEG:
                pairs.append((gap, i, j))
    closed = 0
    for gap, i, j in sorted(pairs):
        if i in used_dead or j in used_entry:
            continue
        used_dead.add(i)
        used_entry.add(j)
        shapes.append(turnaround(by_lane[i][0]["id"], by_lane[i][1]["points"], by_lane[j][1]["points"]))
        closed += 1
    left = [i for i in dead if i not in used_dead]
    return {"dead_ends_joined": joined, "dead_ends_closed": closed, "dead_ends_left": len(left),
            "dead_ends_left_with_next": sum(1 for i in left if by_lane[i][0]["next"])}


def joined_points(pts, target):
    """The lane's points ending on target (the start of its next lane): its last JOIN_SHIFT_M cut and
    the end drawn straight to target, a gentle sideways shift (ZoneGraph links ends that touch)."""
    pts = [tuple(p) for p in pts]
    cut = min(JOIN_SHIFT_M * 100.0, length(pts) / 2.0)
    out = cut_end(pts, cut) if cut > 0 else pts[:-1]
    return [[round(x, 1), round(y, 1)] for x, y in out + [tuple(target)]]


def turnaround(lane_id, into, out_of):
    """A one-way Car spline from the end of into to the start of out_of: on past the end, round,
    and back onto out_of in its own direction (the ends coincide so ZoneGraph links them)."""
    e, s = into[-1], out_of[0]
    h = math.radians(heading(into[-2], into[-1]))
    dx, dy = math.cos(h), math.sin(h)
    over = UTURN_OVERSHOOT_M * 100.0
    radius = max(math.hypot(s[0] - e[0], s[1] - e[1]) / 2.0, UTURN_MIN_RADIUS_M * 100.0)
    p1 = (e[0] + dx * over, e[1] + dy * over)
    p3 = (s[0] + dx * over, s[1] + dy * over)
    apex = ((p1[0] + p3[0]) / 2.0 + dx * radius, (p1[1] + p3[1]) / 2.0 + dy * radius)
    pts = [e, p1, apex, p3, s]
    return {"name": "UTurn_%d" % lane_id, "type": "spline", "profile": "Car", "tags": ["Car"],
            "points": [[round(x, 1), round(y, 1)] for x, y in pts], "speed_kmh": UTURN_SPEED_KMH}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--zone", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--lanes")
    ap.add_argument("--walkways")
    args = ap.parse_args()
    gen = os.path.join(REPO, "assets", "geo", "processed", args.zone, "gen", args.focus)
    lanes_csv = args.lanes or os.path.join(gen, "traffic", "lanes.csv")
    walk_csv = args.walkways or os.path.join(gen, "pedestrians", "walkways.csv")
    lanes = read_lanes(lanes_csv) if os.path.exists(lanes_csv) else []
    walks = read_walkways(walk_csv) if os.path.exists(walk_csv) else []
    if not lanes and not walks:
        raise SystemExit("no lanes.csv nor walkways.csv in " + gen + " (run traffic.py and pedestrians.py)")
    shapes, counts = build(lanes, walks)
    out = os.path.join(gen, "zonegraph")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "zonegraph.json"), "w", encoding="utf-8") as f:
        json.dump({"version": 1, "frame": "unreal_cm_x_east_y_south", "tags": TAGS, "profiles": PROFILES,
                   "shapes": shapes, "counts": counts}, f)
    print(json.dumps(counts))


if __name__ == "__main__":
    main()
