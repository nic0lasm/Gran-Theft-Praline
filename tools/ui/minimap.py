"""Mini-map data for the game: roads, water and building outlines as simple polylines.

Input: assets/geo/processed/<zone>/{roads,water,buildings}.geojson (IGN BD TOPO,
local metres, x east, y north), the same files as tools/city/traffic.py.
Output (not committed, derived data like the rest of the city tables):
  assets/geo/processed/<zone>/gen/minimap/minimap.csv   one row per polyline
  GranTheftPraline/Content/City/Minimap/<Map>.minimap.csv   with --map, staged with the game

Row format, UTF-8 CSV with a header: kind,width_m,points_ue_cm,name
  kind        road_major | road_minor | path | tunnel | water | building
  width_m     carriageway width (0 for water and buildings)
  points      "x y;x y;..." in Unreal cm (X east, Y south), integers; a closed
              ring repeats its first point.
  name        street name (BD TOPO nom_voie_ban), commas removed; empty for
              water, buildings and unnamed roads.
The game (GTPMinimap.h) reads it at start; see docs/ui/minimap.md.

  python3 tools/ui/minimap.py [--zone presqu-ile] [--map L_City_RueDeLaRepublique] \
      [--no-buildings] [--simplify 1.5]
"""

import argparse
import csv
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

PATH_NATURES = {"Sentier", "Escalier", "Piste cyclable", "Chemin"}
MAJOR_IMPORTANCE = {"1", "2", "3"}
MIN_BUILDING_AREA_M2 = 25.0


def road_kind(props):
    """Class of a BD TOPO road for the map; None when it is not drawn."""
    if props.get("nature") in PATH_NATURES:
        return "path"
    try:
        if int(props.get("position_par_rapport_au_sol") or 0) < 0:
            return "tunnel"
    except (TypeError, ValueError):
        pass
    if str(props.get("importance")) in MAJOR_IMPORTANCE:
        return "road_major"
    return "road_minor"


def road_width(props):
    w = props.get("largeur_de_chaussee") or 0
    if not w:
        w = 3.0 * (props.get("nombre_de_voies") or 1)
    return max(float(w), 2.5)


def _perp_dist(p, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(p[0] - ax, p[1] - ay)
    t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - (ax + t * dx), p[1] - (ay + t * dy))


def simplify(points, tol):
    """Douglas-Peucker; keeps the ends (and a closed ring stays closed)."""
    if len(points) < 3 or tol <= 0:
        return list(points)
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        i, j = stack.pop()
        far, far_d = -1, tol
        for k in range(i + 1, j):
            d = _perp_dist(points[k], points[i], points[j])
            if d > far_d:
                far, far_d = k, d
        if far >= 0:
            keep[far] = True
            stack.append((i, far))
            stack.append((far, j))
    return [p for p, k in zip(points, keep) if k]


def ring_area(ring):
    s = 0.0
    for (x0, y0), (x1, y1) in zip(ring, ring[1:]):
        s += x0 * y1 - x1 * y0
    return abs(s) / 2.0


def to_ue_cm(points):
    """Local metres (y north) -> Unreal cm (Y south), integers."""
    return [(int(round(x * 100.0)), int(round(-y * 100.0))) for x, y in points]


def _lines(geom):
    t = geom["type"]
    if t == "LineString":
        return [geom["coordinates"]]
    if t == "MultiLineString":
        return geom["coordinates"]
    return []


def _rings(geom):
    t = geom["type"]
    if t == "Polygon":
        return [geom["coordinates"][0]]
    if t == "MultiPolygon":
        return [poly[0] for poly in geom["coordinates"]]
    return []


def road_name(props):
    name = props.get("nom_voie_ban_gauche") or props.get("nom_voie_ban_droite") or ""
    return " ".join(str(name).replace(",", " ").replace('"', "").split())


def build_rows(roads, water, buildings, tol=1.5):
    """Rows (kind, width_m, points in UE cm) from the three feature lists."""
    rows = []
    for feat in roads:
        props = feat["properties"]
        kind = road_kind(props)
        width = road_width(props)
        name = road_name(props) if kind in ("road_major", "road_minor") else ""
        for line in _lines(feat["geometry"]):
            pts = simplify([(p[0], p[1]) for p in line], tol)
            if len(pts) >= 2:
                rows.append((kind, width, to_ue_cm(pts), name))
    for feat in water:
        for ring in _rings(feat["geometry"]):
            pts = simplify([(p[0], p[1]) for p in ring], tol * 2)
            if len(pts) >= 4:
                rows.append(("water", 0.0, to_ue_cm(pts), ""))
    for feat in buildings:
        for ring in _rings(feat["geometry"]):
            pts = [(p[0], p[1]) for p in ring]
            if ring_area(pts) < MIN_BUILDING_AREA_M2:
                continue
            pts = simplify(pts, tol)
            if len(pts) >= 4:
                rows.append(("building", 0.0, to_ue_cm(pts), ""))
    return rows


def write_rows(rows, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["kind", "width_m", "points_ue_cm", "name"])
        for kind, width, pts, name in rows:
            w.writerow([kind, "%g" % width, ";".join("%d %d" % p for p in pts), name])


def _load(path):
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return json.load(f)["features"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--zone", default="presqu-ile")
    ap.add_argument("--map", help="map name: copy the data into the game for this map")
    ap.add_argument("--no-buildings", action="store_true")
    ap.add_argument("--simplify", type=float, default=1.5, help="tolerance in metres")
    ap.add_argument("--base", help="folder holding the geojson files (default: the zone's)")
    args = ap.parse_args()
    base = args.base or os.path.join(REPO, "assets", "geo", "processed", args.zone)
    rows = build_rows(
        _load(os.path.join(base, "roads.geojson")),
        _load(os.path.join(base, "water.geojson")),
        [] if args.no_buildings else _load(os.path.join(base, "buildings.geojson")),
        args.simplify,
    )
    out = os.path.join(base, "gen", "minimap", "minimap.csv")
    write_rows(rows, out)
    counts = {}
    for kind, _, _, _ in rows:
        counts[kind] = counts.get(kind, 0) + 1
    print("%d polylines %s -> %s" % (len(rows), counts, out))
    if args.map:
        dest = os.path.join(REPO, "GranTheftPraline", "Content", "City", "Minimap", args.map + ".minimap.csv")
        write_rows(rows, dest)
        print("copied to", dest)


if __name__ == "__main__":
    main()
