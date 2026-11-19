"""Keep the driving lanes clear of street furniture and details.

The furniture comes from surveyed positions (props.py, details.py). On the streets of the
Presqu'ile closed to cars in real life, benches, bike racks, parked scooters and signs stand in
the middle of the street, where the game's traffic lanes run (traffic.py keeps every road the
BD TOPO does not close). A car met them head on: the chase car stopped on a bollard, rue Palais
Grillet (candidate 45). Rule: nothing a car can hit stands within CLEAR_M of a lane centre line,
nor of the straight link from the end of a lane to the start of the next one.

- "A car can hit" = the row has collision (no "collision" column, as in props_placement.csv, or
  any value but "false"). Awnings, plaques and the like are left alone.
- An object is a disc (RADIUS_M, DEFAULT_RADIUS_M) or, for the long ones, a rectangle turned by
  its yaw (RECT_M).
- A row in the way is taken out, not moved: beside the lane there is a facade, a doorway or
  another object, and a moved object would no longer be a surveyed one.

props.py and details.py apply the rule when they write their table (lanes computed from the
same roads as traffic.py).

Buildings follow the same rule in generate.py (trim_footprints): a generated footprint loses what
stands within CLEAR_M of a lane centre line, the facade is set back along the lane. A building
that would lose more than DROP_SHARE of its area, or keep less than MIN_BUILDING_M2, is on the
road rather than beside it (a misplaced BD TOPO footprint, rue Champier) and is left out. This file is also the filter for tables that already exist, for a
machine without the raw data:

    python3 tools/city/clear_lanes.py --name presqu-ile --focus rue-de-la-republique \
        [--lanes <Content/City/Traffic/<Map>.lanes.csv>] [--dry-run]

It rewrites gen/<focus>/props_placement.csv and details_placement.csv (the ones that exist),
keeps <table>.before-lanes.csv once, and prints one line, "decor off the lanes: {json}".
Without --lanes it reads gen/<focus>/traffic/lanes.csv. Pass the file the game reads when the
map's lanes may differ from the generation's. Then place_props.py puts each set back
(GTP_PROPS_SET unset for the props, GTP_PROPS_SET=details for the details).
"""

import argparse
import csv
import json
import math
import os
import shutil
import sys

from shapely.geometry import LineString, Point, Polygon
from shapely.ops import unary_union
from shapely.strtree import STRtree

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

CLEAR_M = 1.5              # half a car (1.0 m) and what a driver strays from the centre line
DEFAULT_RADIUS_M = 0.35
# Catalogue id -> radius of the disc (metres), where the default is too small.
RADIUS_M = {"W1-12": 0.4, "W1-13": 0.65, "W1-14": 0.9, "W1-31": 0.85, "W1-54": 0.9, "W1-55": 0.6,
            "W14-47": 1.0}
# Catalogue id -> (depth along the object's front axis, length across it), metres.
RECT_M = {"W1-01": (0.7, 1.8), "W1-21": (1.7, 4.2)}
TABLES = ("props_placement.csv", "details_placement.csv")
DROP_SHARE = 0.4           # a footprint that loses more than this to the lanes is left out
MIN_BUILDING_M2 = 25.0     # and so is one with less than this left
MIN_CUT_M2 = 0.05          # a footprint that only grazes the corridor is left as it is


def lanes_from_csv(path):
    """Lanes of a lanes.csv (traffic.py, Unreal cm, Y south) as traffic.build_lanes gives them
    (local metres, y north): id, name, points, next."""
    lanes = []
    with open(path, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            pts = [(float(p.split()[0]) / 100.0, -float(p.split()[1]) / 100.0) for p in r["points_ue_cm"].split(";")]
            lanes.append({"id": int(r["lane_id"]), "name": r["name"], "points": pts,
                          "next": [int(n) for n in r["next"].split()]})
    return lanes


def lanes_of_focus(proc, gen):
    """The lanes traffic.py will write for this focus, computed from the same inputs. Empty when
    the roads or the generated buildings are not there yet. A streaming cell (gen/cell-*) gets
    the lanes of every car road of the zone instead: the quarter's lanes depend on the buildings
    of all the cells, which may not be generated yet. Those lanes carry no id (the game numbers
    the quarter's lanes differently)."""
    import traffic
    roads = os.path.join(proc, "roads.geojson")
    if not os.path.exists(roads) or not os.path.exists(os.path.join(gen, "buildings_gen.csv")):
        return []
    with open(roads, encoding="utf-8") as f:
        features = json.load(f)["features"]
    carriageways = traffic.load_carriageways(os.path.join(proc, "carriageway.geojson"))
    if os.path.basename(os.path.normpath(gen)).startswith("cell-"):
        return [dict(lane, anonymous=True) for lane in traffic.build_lanes(features, None, carriageways)]
    return traffic.build_lanes(features, traffic.focus_area_m(gen), carriageways)


def lanes_of_zone(proc):
    """Lanes of every car road of the zone (no id: the game numbers the lanes of a focus). For
    generate.py, which runs before the buildings that bound a focus exist."""
    import traffic
    roads = os.path.join(proc, "roads.geojson")
    if not os.path.exists(roads):
        return []
    with open(roads, encoding="utf-8") as f:
        features = json.load(f)["features"]
    carriageways = traffic.load_carriageways(os.path.join(proc, "carriageway.geojson"))
    return [dict(lane, anonymous=True) for lane in traffic.build_lanes(features, None, carriageways)]


class Lanes:
    """Centre lines and lane-to-lane links, ready for distance queries."""

    def __init__(self, lanes):
        by_id = {lane["id"]: lane for lane in lanes}
        self.lines, self.labels = [], []
        for lane in lanes:
            anonymous = lane.get("anonymous")
            if len(lane["points"]) >= 2:
                self.lines.append(LineString(lane["points"]))
                self.labels.append(lane["name"] if anonymous else "%d %s" % (lane["id"], lane["name"]))
            for n in lane["next"]:
                nxt = by_id.get(n)
                if nxt and nxt["points"] and lane["points"] and lane["points"][-1] != nxt["points"][0]:
                    self.lines.append(LineString([lane["points"][-1], nxt["points"][0]]))
                    self.labels.append(lane["name"] if anonymous else "%d>%d %s" % (lane["id"], n, lane["name"]))
        self.tree = STRtree(self.lines) if self.lines else None

    def in_the_way(self, shape):
        """(label of the lane, gap to its centre line in metres) when the shape is within CLEAR_M
        of a lane, else None."""
        if self.tree is None:
            return None
        best = None
        for i in self.tree.query(shape.buffer(CLEAR_M)):
            d = self.lines[i].distance(shape)
            if d < CLEAR_M and (best is None or d < best[1]):
                best = (self.labels[i], d)
        return best


    def corridor(self, shape):
        """(union of the corridors of the lanes within CLEAR_M of the shape, their labels), or
        (None, []). Square ends and mitred bends: the cut through a wall stays a straight line."""
        if self.tree is None:
            return None, []
        near = [i for i in self.tree.query(shape.buffer(CLEAR_M)) if self.lines[i].distance(shape) < CLEAR_M]
        if not near:
            return None, []
        return (unary_union([self.lines[i].buffer(CLEAR_M, cap_style=3, join_style=2) for i in near]),
                sorted({self.labels[i] for i in near}))


def trim_footprints(buildings, lanes):
    """Building footprints kept clear of the lanes. buildings: dicts with "id" and "poly" (as
    generate.load_buildings); the "poly" of a trimmed building is replaced. Returns (buildings
    kept, report): report lists what was trimmed and what was left out."""
    index = lanes if isinstance(lanes, Lanes) else Lanes(lanes)
    kept, trimmed, left_out = [], [], []
    for b in buildings:
        poly = b["poly"]
        corridor, labels = index.corridor(poly)
        rest = poly.difference(corridor) if corridor is not None else poly
        cut = poly.area - rest.area
        if cut < MIN_CUT_M2:
            kept.append(b)
            continue
        if rest.geom_type == "MultiPolygon":       # the lane cuts it in two: the larger side stays
            rest = max(rest.geoms, key=lambda g: g.area)
        rest = rest.simplify(0.02) if not rest.is_empty else rest
        entry = {"id": b["id"], "area_m2": round(poly.area, 1), "cut_m2": round(poly.area - rest.area, 1), "lanes": labels}
        if rest.is_empty or rest.geom_type != "Polygon" or not rest.is_valid \
                or rest.area < MIN_BUILDING_M2 or poly.area - rest.area > DROP_SHARE * poly.area:
            left_out.append(entry)
            continue
        b["poly"] = rest
        trimmed.append(entry)
        kept.append(b)
    return kept, {"trimmed": trimmed, "left_out": left_out}


def footprint(row):
    """Ground shape of a placed row (local metres)."""
    x, y = float(row["x_m"]), float(row["y_m"])
    rect = RECT_M.get(row["catalog_id"])
    if not rect:
        return Point(x, y).buffer(RADIUS_M.get(row["catalog_id"], DEFAULT_RADIUS_M), 8)
    yaw = math.radians(float(row["ue_yaw"]))
    # Unreal yaw turns from +X (east) towards +Y (south): front and side axes in local metres.
    fx, fy = math.cos(yaw), -math.sin(yaw)
    sx, sy = -math.sin(yaw), -math.cos(yaw)
    hd, hl = rect[0] / 2.0, rect[1] / 2.0
    return Polygon([(x + fx * a * hd + sx * b * hl, y + fy * a * hd + sy * b * hl)
                    for a, b in ((1, 1), (1, -1), (-1, -1), (-1, 1))])


def collides(row):
    return row.get("collision", "true") != "false"


def split(rows, lanes):
    """(rows kept, rows taken out). Each row taken out is (row, lane label, gap in metres)."""
    index = lanes if isinstance(lanes, Lanes) else Lanes(lanes)
    kept, out = [], []
    for row in rows:
        hit = index.in_the_way(footprint(row)) if collides(row) else None
        if hit:
            out.append((row, hit[0], round(hit[1], 2)))
        else:
            kept.append(row)
    return kept, out


def summary(out):
    """Counts for a report: by catalogue id and asset, and by lane."""
    by_asset, by_lane = {}, {}
    for row, lane, _ in out:
        key = (row["catalog_id"] + " " + row.get("asset", "")).strip()
        by_asset[key] = by_asset.get(key, 0) + 1
        by_lane[lane] = by_lane.get(lane, 0) + 1
    return {"removed": len(out), "by_asset": dict(sorted(by_asset.items())), "by_lane": dict(sorted(by_lane.items()))}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    ap.add_argument("--lanes", help="lanes.csv to clear (default: gen/<focus>/traffic/lanes.csv)")
    ap.add_argument("--dry-run", action="store_true", help="count only, write nothing")
    args = ap.parse_args()
    gen = os.path.join(args.geo, "processed", args.name, "gen", args.focus)
    lanes_csv = args.lanes or os.path.join(gen, "traffic", "lanes.csv")
    if not os.path.exists(lanes_csv):
        sys.exit("no lanes: " + lanes_csv)
    lanes = lanes_from_csv(lanes_csv)
    index = Lanes(lanes)
    report = {"lanes": len(lanes), "clear_m": CLEAR_M, "dry_run": args.dry_run, "tables": {}}
    for name in TABLES:
        path = os.path.join(gen, name)
        if not os.path.exists(path):
            continue
        with open(path, encoding="utf-8", newline="") as f:
            reader = csv.DictReader(f)
            fields, rows = reader.fieldnames, list(reader)
        kept, out = split(rows, index)
        entry = dict(summary(out), rows_before=len(rows), rows_after=len(kept),
                     refs=sorted({"%s %s" % (row["catalog_id"], row["ref"]) for row, _, _ in out}))
        if out and not args.dry_run:
            backup = path[:-4] + ".before-lanes.csv"
            if not os.path.exists(backup):
                shutil.copyfile(path, backup)
            entry["backup"] = os.path.basename(backup)
            with open(path, "w", encoding="utf-8", newline="") as f:
                w = csv.DictWriter(f, fields)
                w.writeheader()
                w.writerows(kept)
        report["tables"][name] = entry
    print("decor off the lanes: " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
