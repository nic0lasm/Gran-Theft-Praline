"""Driving lanes of a focus street, for the cars driven by the game (AI drivers).

Input: assets/geo/processed/<zone>/roads.geojson (IGN BD TOPO road centre lines,
local metres, x east, y north) and gen/<focus>/buildings_gen.csv (extent), as
ground_mask.py.
Output (not committed, like the rest of gen/):
  gen/<focus>/traffic/lanes.csv      one row per lane: id, road, name, speed,
                                     width, points in Unreal cm "x y;x y;...",
                                     ids of the lanes it leads to
  gen/<focus>/traffic/traffic.json   counts and extent
  gen/<focus>/traffic/lanes.png      top view for a quick check
With --map <MapName>, lanes.csv is also copied to
GranTheftPraline/Content/City/Traffic/<MapName>.lanes.csv (not committed, staged
with the game), where the traffic network (GTPTraffic.h) reads it at start.

Rules:
- lanes are kept within 60 m of a generated building;
- roads closed to cars are left out, as in ground_mask.py (vitesse_moyenne_vl = 0,
  paths, steps, cycle tracks), and tunnels (position_par_rapport_au_sol < 0);
  bridges (position 1) are kept: the deck built by riverside.py carries them,
  and cars find its height through physics;
- sens_de_circulation gives the directions: "Sens direct" follows the drawing,
  "Sens inverse" goes against it, anything else is two-way;
- France drives on the right: two-way roads share their width, each direction
  on its right half; lanes of one direction sit side by side;
- at each end of a road the lane continues into every lane leaving the same
  junction (numbered in from_junction / to_junction: one car at a time crosses
  it), except the same road backwards or any turn sharper than 135 degrees
  (no U-turn): a lane with no other way on is a dead end, where the game takes
  the car away out of sight.

  python3 tools/city/traffic.py [--zone presqu-ile] [--focus rue-de-la-republique] \
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

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
MARGIN_CM = 6000.0
STEP_M = 4.0           # spacing of lane points
NODE_SNAP_M = 0.5      # road ends closer than this are the same junction
MIN_LANE_M = 3.0       # shorter lanes (clipping slivers) are dropped
MIN_TWO_WAY_OFFSET_M = 1.4
JUNCTION_MERGE_M = 18.0
AREA_BUFFER_M = 60.0
MAX_TURN_DEG = 135.0   # sharper turns at a junction count as U-turns
CAR_NATURES_OUT = {"Sentier", "Escalier", "Piste cyclable", "Chemin", "Bac ou liaison maritime"}


def road_width(p):
    w = p.get("largeur_de_chaussee") or 0
    if not w:
        w = 3.0 * (p.get("nombre_de_voies") or 1)
    return max(float(w), 2.5)


def is_car_road(p):
    if p.get("nature") in CAR_NATURES_OUT:
        return False
    if p.get("vitesse_moyenne_vl") == 0:
        return False
    try:
        return int(p.get("position_par_rapport_au_sol") or 0) >= 0
    except (TypeError, ValueError):
        return True


def directions(p):
    sens = p.get("sens_de_circulation")
    if sens == "Sens direct":
        return [1]
    if sens == "Sens inverse":
        return [-1]
    return [1, -1]


def speed_kmh(p):
    v = p.get("vitesse_moyenne_vl") or 30
    return max(20, min(50, int(v)))


def resample(coords, step):
    line = LineString(coords)
    n = max(1, int(math.ceil(line.length / step)))
    return [tuple(line.interpolate(i * line.length / n).coords[0]) for i in range(n + 1)]


def node_key(pt):
    return (round(pt[0] / NODE_SNAP_M), round(pt[1] / NODE_SNAP_M))


def build_lanes(features, extent_m=None):
    """Lanes (dicts) from road features in local metres, clipped to extent_m:
    a box (x0, y0, x1, y1) or any shapely geometry."""
    if extent_m is None:
        clip = None
    elif isinstance(extent_m, tuple):
        clip = box(*extent_m)
    else:
        clip = extent_m
    lanes = []
    for feat in features:
        p = feat["properties"]
        if not is_car_road(p):
            continue
        width = road_width(p)
        dirs = directions(p)
        total = max(1, int(p.get("nombre_de_voies") or 1))
        per_dir = max(1, total // len(dirs)) if len(dirs) == 2 else total
        lane_w = width / (per_dir * len(dirs))
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
                if part.length < MIN_LANE_M:
                    continue
                # Junctions are the ends of the drawn centre line, before any offset.
                start, end = part.coords[0], part.coords[-1]
                for d in dirs:
                    base = part if d == 1 else LineString(list(part.coords)[::-1])
                    a, b = (start, end) if d == 1 else (end, start)
                    for k in range(per_dir):
                        if len(dirs) == 2:
                            # Narrow two-way streets: keep oncoming cars 2.8 m apart, so they pass.
                            offset = max((k + 0.5) * lane_w, MIN_TWO_WAY_OFFSET_M)
                        else:
                            offset = -width / 2.0 + (k + 0.5) * lane_w
                        # shapely: positive offset = left side; France drives on the right.
                        lane_line = base.offset_curve(-offset) if abs(offset) > 1e-3 else base
                        if lane_line.is_empty or lane_line.geom_type != "LineString" or lane_line.length < MIN_LANE_M:
                            continue
                        lanes.append({
                            "road": p.get("cleabs") or "",
                            "name": (p.get("nom_collaboratif_gauche") or p.get("nom_collaboratif_droite")
                                     or p.get("nom_1_gauche") or p.get("nom_1_droite") or ""),
                            "speed_kmh": speed_kmh(p),
                            "width_m": round(lane_w, 2),
                            "points": resample(list(lane_line.coords), STEP_M),
                            "from": node_key(a), "to": node_key(b), "dir": d,
                            "slot": (k + 0.5) / per_dir,
                        })
    for i, lane in enumerate(lanes):
        lane["id"] = i
    link_lanes(lanes)
    return lanes


def link_lanes(lanes):
    starts = {}
    for lane in lanes:
        starts.setdefault(lane["from"], []).append(lane)
    for lane in lanes:
        options = [o for o in starts.get(lane["to"], []) if o["id"] != lane["id"]]
        no_uturn = [o for o in options if not (o["road"] == lane["road"] and o["dir"] == -lane["dir"])
                    and heading_change_deg(lane, o) <= MAX_TURN_DEG]
        # Several lanes of one road ahead: keep only the one in the same position
        # across the road (left lane to left lane), so lanes never cross mid-junction.
        best = {}
        for o in no_uturn:
            key = (o["road"], o["dir"])
            if key not in best or abs(o["slot"] - lane["slot"]) < abs(best[key]["slot"] - lane["slot"]):
                best[key] = o
        lane["next"] = sorted(o["id"] for o in best.values())


def heading_change_deg(a, b):
    """Turn between the end of lane a and the start of lane b, degrees (0 = straight on)."""
    (x0, y0), (x1, y1) = a["points"][-2], a["points"][-1]
    (x2, y2), (x3, y3) = b["points"][0], b["points"][1]
    h0 = math.atan2(y1 - y0, x1 - x0)
    h1 = math.atan2(y3 - y2, x3 - x2)
    return abs((math.degrees(h1 - h0) + 180.0) % 360.0 - 180.0)


def write(lanes, out_dir, extent_cm):
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "lanes.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["lane_id", "road_id", "name", "speed_kmh", "width_m", "points_ue_cm", "next",
                    "from_junction", "to_junction"])
        # A real junction joins three roads or more; plain joins between two pieces
        # of the same street are not numbered (-1), so cars do not queue there.
        roads_at = {}
        for lane in lanes:
            for key in (lane["from"], lane["to"]):
                roads_at.setdefault(key, set()).add(lane["road"])
        # Big crossings are drawn as several nodes a few metres apart (dual
        # carriageways, islands): nodes closer than JUNCTION_MERGE_M share one id.
        real = [key for key, roads in sorted(roads_at.items()) if len(roads) >= 3]
        parent = {key: key for key in real}

        def find(key):
            while parent[key] != key:
                parent[key] = parent[parent[key]]
                key = parent[key]
            return key

        reach = JUNCTION_MERGE_M / NODE_SNAP_M
        for i, a in enumerate(real):
            for b in real[i + 1:]:
                if math.hypot(a[0] - b[0], a[1] - b[1]) <= reach:
                    parent[find(a)] = find(b)
        ids = {}
        junctions = {key: ids.setdefault(find(key), len(ids)) for key in real}
        for lane in lanes:
            pts = ";".join(f"{x * 100.0:.0f} {-y * 100.0:.0f}" for x, y in lane["points"])
            w.writerow([lane["id"], lane["road"], lane["name"], lane["speed_kmh"], lane["width_m"], pts,
                        " ".join(str(n) for n in lane["next"]), junctions.get(lane["from"], -1), junctions.get(lane["to"], -1)])
    dead_ends = sum(1 for lane in lanes if not lane["next"])
    meta = {"lanes": len(lanes), "dead_ends": dead_ends,
            "length_km": round(sum(LineString(lane["points"]).length for lane in lanes) / 1000.0, 2),
            "extent_ue_cm": extent_cm}
    with open(os.path.join(out_dir, "traffic.json"), "w") as f:
        json.dump(meta, f, indent=2)
    return meta


def preview(lanes, path, size=2048):
    from PIL import Image, ImageDraw
    xs = [x for lane in lanes for x, _ in lane["points"]]
    ys = [y for lane in lanes for _, y in lane["points"]]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    s = (size - 40) / max(x1 - x0, y1 - y0)
    img = Image.new("RGB", (int((x1 - x0) * s) + 40, int((y1 - y0) * s) + 40), (24, 24, 24))
    d = ImageDraw.Draw(img)

    def px(p):
        return (20 + (p[0] - x0) * s, 20 + (y1 - p[1]) * s)

    for lane in lanes:
        pts = [px(p) for p in lane["points"]]
        colour = (230, 230, 230) if lane["next"] else (230, 60, 50)
        d.line(pts, fill=colour, width=2)
        # Arrow head at the end shows the direction.
        (ax, ay), (bx, by) = pts[-2], pts[-1]
        ang = math.atan2(by - ay, bx - ax)
        for side in (2.6, -2.6):
            d.line([(bx, by), (bx + 9 * math.cos(ang + side), by + 9 * math.sin(ang + side))], fill=(255, 190, 40), width=2)
    img.save(path)


def focus_area_m(gen):
    """Where cars may drive: within AREA_BUFFER_M of a generated building, so no car
    shows up on a road with no city around it (the zone's corners)."""
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    return unary_union([Point(float(r["cx"]), float(r["cy"])).buffer(AREA_BUFFER_M, 8) for r in rows])


def focus_extent_m(gen):
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    xs = [float(r["ue_x"]) for r in rows]
    ys = [float(r["ue_y"]) for r in rows]
    x0, x1 = min(xs) - MARGIN_CM, max(xs) + MARGIN_CM
    y0, y1 = min(ys) - MARGIN_CM, max(ys) + MARGIN_CM
    # Unreal cm (Y south) -> local metres (y north).
    return (x0 / 100.0, -y1 / 100.0, x1 / 100.0, -y0 / 100.0), [x0, y0, x1, y1]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--zone", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--roads", help="roads.geojson to read instead of the zone's")
    ap.add_argument("--map", help="map name: copy the lanes into the game for this map")
    args = ap.parse_args()
    base = os.path.join(REPO, "assets", "geo", "processed", args.zone)
    gen = os.path.join(base, "gen", args.focus)
    with open(args.roads or os.path.join(base, "roads.geojson"), encoding="utf-8") as f:
        roads = json.load(f)["features"]
    _, extent_cm = focus_extent_m(gen)
    lanes = build_lanes(roads, focus_area_m(gen))
    out = os.path.join(gen, "traffic")
    meta = write(lanes, out, extent_cm)
    preview(lanes, os.path.join(out, "lanes.png"))
    if args.map:
        dest = os.path.join(REPO, "GranTheftPraline", "Content", "City", "Traffic", args.map + ".lanes.csv")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(os.path.join(out, "lanes.csv"), dest)
        meta["installed"] = dest
    print(json.dumps(meta, indent=2))


if __name__ == "__main__":
    main()
