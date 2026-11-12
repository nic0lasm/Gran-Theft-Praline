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
    for jid in sorted(junctions):
        ends = junctions[jid]
        if len(ends) < 2:
            continue
        cx = sum(p[1][0] for p in ends) / len(ends)
        cy = sum(p[1][1] for p in ends) / len(ends)
        ends.sort(key=lambda p: math.atan2(p[1][1] - cy, p[1][0] - cx))
        shapes.append({"name": "Junction_%d" % jid, "type": "polygon", "profile": "Car", "tags": ["Car", "Intersection"],
                       "points": [[round(p[1][0], 1), round(p[1][1], 1)] for p in ends],
                       "roles": [p[0] for p in ends], "yaw_deg": [round(p[2], 1) for p in ends],
                       "lanes": [p[3] for p in ends], "centre": [round(cx, 1), round(cy, 1)]})
        polygons += 1
    crossings = 0
    for e in walkways:
        crossing = e["kind"] == KIND_CROSSING
        crossings += 1 if crossing else 0
        shapes.append({"name": "Walk_%s" % e["id"], "type": "spline", "profile": "Crossing" if crossing else "Pedestrian",
                       "tags": ["Pedestrian", "Crossing"] if crossing else ["Pedestrian"],
                       "points": [[round(x, 1), round(y, 1)] for x, y in e["points"]], "width_cm": round(e["width_m"] * 100.0, 1)})
    counts = {"car_lanes": len(lanes), "junctions": polygons, "walkways": len(walkways), "crossings": crossings,
              "shapes": len(shapes)}
    return shapes, counts


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
