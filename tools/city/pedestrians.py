"""Walkways of a focus street, for the pedestrians of the game (GTPPedestrians.h).

Input: assets/geo/processed/<zone>/roads.geojson (IGN BD TOPO road centre lines,
local metres, x east, y north) and gen/<focus>/buildings_gen.csv (area), as
traffic.py and ground_mask.py.
Output (not committed, like the rest of gen/):
  gen/<focus>/pedestrians/walkways.csv  one row per walkway edge: id, road, kind,
                                        node_a, node_b, width, points in Unreal cm,
                                        facade_side (+1 facade on the left of the
                                        walking direction, -1 right, 0 none)
  gen/<focus>/pedestrians/walkways.json counts and extent
  gen/<focus>/pedestrians/walkways.png  top view for a quick check
With --map <MapName>, walkways.csv is also copied to
GranTheftPraline/Content/City/Pedestrians/<MapName>.walkways.csv (not committed,
staged with the game), where the pedestrian subsystem reads it at start.

Rules:
- a street open to cars gets one walkway on each side, a kerb and a metre beyond
  the carriageway edge (width from ground_mask.road_width);
- a street closed to cars (rue de la Republique) gets three: one along each
  facade zone and one down the middle, as the people of a paved street spread out;
- steps, tunnels and ferries are left out; walkways stay within AREA_BUFFER_M of
  a generated building;
- the ends of walkways that meet at a road junction (same snapped node) are
  linked by straight edges: kind "crossing" where three roads or more meet (the
  walker looks out for cars there), kind "join" where a street just continues.

Edges are undirected: a walker enters one by either end. kind: 0 sidewalk,
1 pedestrian street, 2 crossing, 3 join.

  python3 tools/city/pedestrians.py [--zone presqu-ile] [--focus rue-de-la-republique] \
      [--map L_City_RueDeLaRepublique]
"""

import argparse
import csv
import json
import math
import os
import shutil

from shapely.geometry import LineString, Point, box
from shapely.ops import unary_union

import ground_mask

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
MARGIN_CM = ground_mask.MARGIN_CM
STEP_M = 5.0              # spacing of walkway points
NODE_SNAP_M = 0.5         # road ends closer than this are the same junction
MIN_WALK_M = 3.0
AREA_BUFFER_M = 60.0
SIDEWALK_GAP_M = ground_mask.KERB_M + 1.1   # carriageway edge to the walking line
PEDESTRIAN_OFFSETS_M = (-6.0, 0.0, 6.0)
MAX_LINK_M = 45.0         # crossings longer than this are not built
LINKS_PER_NODE = 3
KIND_SIDEWALK, KIND_PEDESTRIAN, KIND_CROSSING, KIND_JOIN = 0, 1, 2, 3
SKIP_NATURES = {"Escalier", "Bac ou liaison maritime"}


def walkable(p):
    if p.get("nature") in SKIP_NATURES:
        return False
    return (p.get("position_par_rapport_au_sol") or "0") in ("0", 0, None, "")


def offsets_for(p):
    if ground_mask.is_pedestrian(p):
        return KIND_PEDESTRIAN, PEDESTRIAN_OFFSETS_M, ground_mask.PEDESTRIAN_W_M / 3.0
    half = ground_mask.road_width(p) / 2.0 + SIDEWALK_GAP_M
    return KIND_SIDEWALK, (-half, half), 2.0


def node_key(pt):
    return (round(pt[0] / NODE_SNAP_M), round(pt[1] / NODE_SNAP_M))


def resample(coords, step):
    line = LineString(coords)
    n = max(1, int(math.ceil(line.length / step)))
    return [tuple(line.interpolate(i * line.length / n).coords[0]) for i in range(n + 1)]


def build_walkways(features, extent_m=None):
    """Edges (dicts) from road features in local metres, clipped to extent_m (box or
    shapely geometry). Each edge: id, road, kind, width_m, facade, points (local metres),
    node_a, node_b (ids; ends at one junction on one side of one road share an id
    only through the link edges, see link_junctions)."""
    if extent_m is None:
        clip = None
    elif isinstance(extent_m, tuple):
        clip = box(*extent_m)
    else:
        clip = extent_m
    edges = []
    for feat in features:
        p = feat["properties"]
        if not walkable(p):
            continue
        kind, offsets, width = offsets_for(p)
        for coords in feat["geometry"]["coordinates"]:
            if len(coords) < 2:
                continue
            centre = LineString([c[:2] for c in coords])
            parts = [centre]
            if clip is not None:
                cut = centre.intersection(clip)
                if cut.is_empty:
                    continue
                parts = [g for g in getattr(cut, "geoms", [cut]) if g.geom_type == "LineString"]
            for part in parts:
                if part.length < MIN_WALK_M:
                    continue
                junction_a, junction_b = node_key(part.coords[0]), node_key(part.coords[-1])
                for off in offsets:
                    line = part.offset_curve(off) if abs(off) > 1e-3 else part
                    if line.is_empty or line.geom_type != "LineString" or line.length < MIN_WALK_M:
                        continue
                    pts = resample(list(line.coords), STEP_M)
                    # offset_curve(+) is the left of the drawing; keep the drawing's direction.
                    if Point(pts[0]).distance(Point(part.coords[0])) > Point(pts[-1]).distance(Point(part.coords[0])):
                        pts.reverse()
                    # Facade on the outer side of a walkway: left of the drawing for a positive
                    # offset, right for a negative one; none for the line down the middle.
                    facade = 0 if abs(off) < 1e-3 else (1 if off > 0 else -1)
                    edges.append({"road": p.get("cleabs") or "", "kind": kind, "width_m": round(width, 2),
                                  "points": pts, "ja": junction_a, "jb": junction_b, "facade": facade})
    for i, e in enumerate(edges):
        e["id"] = i
        e["node_a"] = 2 * i
        e["node_b"] = 2 * i + 1
    return link_junctions(edges)


def link_junctions(edges):
    """Straight link edges between the walkway ends meeting at one junction."""
    ends = {}
    for e in edges:
        ends.setdefault(e["ja"], []).append((e, 0))
        ends.setdefault(e["jb"], []).append((e, -1))
    out = list(edges)
    seen = set()
    for key, members in ends.items():
        roads = {e["road"] for e, _ in members}
        kind = KIND_CROSSING if len(roads) >= 3 else KIND_JOIN
        for i, (ea, ia) in enumerate(members):
            pa = ea["points"][ia]
            na = ea["node_a"] if ia == 0 else ea["node_b"]
            near = []
            for j, (eb, ib) in enumerate(members):
                if i == j:
                    continue
                pb = eb["points"][ib]
                d = math.hypot(pa[0] - pb[0], pa[1] - pb[1])
                nb = eb["node_a"] if ib == 0 else eb["node_b"]
                if d <= MAX_LINK_M:
                    near.append((d, nb, pb))
            near.sort()
            for d, nb, pb in near[:LINKS_PER_NODE]:
                pair = (min(na, nb), max(na, nb))
                if pair in seen:
                    continue
                seen.add(pair)
                out.append({"id": len(out), "road": "", "kind": kind, "width_m": 2.0,
                            "points": [pa, pb], "node_a": na, "node_b": nb, "facade": 0})
    for i, e in enumerate(out):
        e["id"] = i
    merge_nodes(out)
    return out


def merge_nodes(edges):
    """Ends that are the same place (a walkway end and the link end touching it) share
    a node id, so the runtime can walk from an edge into the next."""
    parent = {}

    def find(x):
        parent.setdefault(x, x)
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for e in edges:
        find(e["node_a"])
        find(e["node_b"])
    # Link edges already use the node ids of the walkway ends they touch: nothing to merge,
    # but number the nodes densely.
    ids = {}
    for e in edges:
        e["node_a"] = ids.setdefault(find(e["node_a"]), len(ids))
        e["node_b"] = ids.setdefault(find(e["node_b"]), len(ids))


def write(edges, out_dir, extent_cm):
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "walkways.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["edge_id", "road_id", "kind", "node_a", "node_b", "width_m", "points_ue_cm", "facade_side"])
        for e in edges:
            pts = ";".join(f"{x * 100.0:.0f} {-y * 100.0:.0f}" for x, y in e["points"])
            w.writerow([e["id"], e["road"], e["kind"], e["node_a"], e["node_b"], e["width_m"], pts, e["facade"]])
    meta = {"edges": len(edges),
            "sidewalks": sum(1 for e in edges if e["kind"] in (KIND_SIDEWALK, KIND_PEDESTRIAN)),
            "crossings": sum(1 for e in edges if e["kind"] == KIND_CROSSING),
            "length_km": round(sum(LineString(e["points"]).length for e in edges) / 1000.0, 2),
            "extent_ue_cm": extent_cm}
    with open(os.path.join(out_dir, "walkways.json"), "w") as f:
        json.dump(meta, f, indent=2)
    return meta


def preview(edges, path, size=2048):
    from PIL import Image, ImageDraw
    xs = [x for e in edges for x, _ in e["points"]]
    ys = [y for e in edges for _, y in e["points"]]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    s = (size - 40) / max(x1 - x0, y1 - y0)
    img = Image.new("RGB", (int((x1 - x0) * s) + 40, int((y1 - y0) * s) + 40), (24, 24, 24))
    d = ImageDraw.Draw(img)
    colours = {KIND_SIDEWALK: (120, 200, 120), KIND_PEDESTRIAN: (90, 160, 255),
               KIND_CROSSING: (255, 190, 40), KIND_JOIN: (160, 160, 160)}
    for e in edges:
        d.line([(20 + (x - x0) * s, 20 + (y1 - y) * s) for x, y in e["points"]], fill=colours[e["kind"]], width=2)
    img.save(path)


def focus_area_m(gen):
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    return unary_union([Point(float(r["cx"]), float(r["cy"])).buffer(AREA_BUFFER_M, 8) for r in rows])


def focus_extent_cm(gen):
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    xs = [float(r["ue_x"]) for r in rows]
    ys = [float(r["ue_y"]) for r in rows]
    return [min(xs) - MARGIN_CM, min(ys) - MARGIN_CM, max(xs) + MARGIN_CM, max(ys) + MARGIN_CM]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--zone", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--roads", help="roads.geojson to read instead of the zone's")
    ap.add_argument("--map", help="map name: copy the walkways into the game for this map")
    args = ap.parse_args()
    base = os.path.join(REPO, "assets", "geo", "processed", args.zone)
    gen = os.path.join(base, "gen", args.focus)
    with open(args.roads or os.path.join(base, "roads.geojson"), encoding="utf-8") as f:
        roads = json.load(f)["features"]
    edges = build_walkways(roads, focus_area_m(gen))
    out = os.path.join(gen, "pedestrians")
    meta = write(edges, out, focus_extent_cm(gen))
    preview(edges, os.path.join(out, "walkways.png"))
    if args.map:
        dest = os.path.join(REPO, "GranTheftPraline", "Content", "City", "Pedestrians", args.map + ".walkways.csv")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(os.path.join(out, "walkways.csv"), dest)
        meta["installed"] = dest
    print(json.dumps(meta, indent=2))


if __name__ == "__main__":
    main()
