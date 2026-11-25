"""Ground mask of a focus street: where the terrain is asphalt, kerb, paving or marking.

Input: assets/geo/processed/<zone>/roads.geojson (IGN BD TOPO road centre lines,
local metres, x east, y north) and gen/<focus>/buildings_gen.csv (extent).
Output (not committed, like the rest of gen/):
  gen/<focus>/ground/T_GroundMask.png   RGB (no alpha), one texel ~15 cm, Unreal axes:
      R = asphalt carriageway, G = pedestrian paving (streets closed to cars, paths, steps),
      B = road marking where R is set, kerb stones where it is not
      (everything else is sidewalk paving). No alpha channel: the Unreal PNG importer
      fills the colour of transparent texels, which wiped R and G off the first version.
  gen/<focus>/ground/ground.json        extent in Unreal cm, read by
      tools/city/unreal/apply_city_materials.py to map world X/Y to the mask
Road widths come from largeur_de_chaussee, else 3 m per lane. A road with
vitesse_moyenne_vl = 0 is closed to cars (rue de la République) and gets paving.
Tunnels (position_par_rapport_au_sol < 0) are left out.

  python3 tools/city/ground_mask.py [--zone presqu-ile] [--focus rue-de-la-republique]

--far: the same mask over the whole terrain (terrain/terrain.glb), 4096 texels a side (34 cm),
drawn twice as fine and averaged. The focus mask stops 60 m past the generated buildings: gate 81,
long take, the flight starts on quai Jules Courmont, 60 m east of it, on a bare striped plain with
no carriageway and no river. The ground material reads the far mask wherever the focus mask
ends. Output (not committed, like the rest; route_dressing.py makes it before its materials step):
  assets/geo/processed/<zone>/ground_far/T_GroundMaskFar.png
  assets/geo/processed/<zone>/ground_far/ground_far.json   extent, and the coarse outlines and
      levels of the rivers (water.geojson, BD TOPO) in Unreal cm, which the material draws as water

  python3 tools/city/ground_mask.py --far [--processed <the zone's folder of processed data>]

--probe <Map>.circuit.csv: nothing is made; one line of JSON says what the focus, far and route
masks of this machine hold under the points of the driven line (asphalt, marking, paving, kerb,
sidewalk). route_dressing.py puts it in the note of its materials step.

--route <Map>.circuit.csv: the route mask. On the quay the far mask tells carriageway from
pavement, but a kerb or a lane line is thinner than one of its texels: gate 83, long take, the
flight drives a plain of asphalt with no line and no kerb. This mask is as fine as the focus mask
(4096 texels on its long side, 11 to 16 cm on today's circuit), over the box of the driven line
(column line_ue_cm) the focus mask does not hold, 40 m wider; the material reads it there, the
far mask past it. Nothing is written when the focus mask holds the whole line. Output:
  assets/geo/processed/<zone>/ground_far/T_GroundMaskRoute.png, ground_route.json

Lane lines: a one-way road of two lanes or more has a dashed line between each pair of its
lanes when they are at least 2.5 m wide (the quays: four lanes, three lines); a two-way road
keeps its one centre line. In the focus and route masks a line is two texels wide at least.
A focus mask made before 2026-11-25 has thin centre lines only: route_dressing.py now makes the
focus mask again before its materials step, from the data of the machine.
"""

import argparse
import csv
import json
import math
import os
import sys

from shapely.geometry import shape
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
MARGIN_CM = 6000.0
TEXELS_LONG = 8192
KERB_M = 0.35
PEDESTRIAN_W_M = 22.0
DASH_M, GAP_M, MARK_W_M = 3.0, 3.5, 0.15
FAR_TEXELS = 4096
ROUTE_TEXELS = 4096          # the long side of the route mask
ROUTE_MARGIN_CM = 4000.0     # the route mask reaches this far around the driven line
LANE_MIN_M = 2.5             # no lane line on a road whose lanes would be narrower
FAR_FINER = 2                # the far mask is drawn this much finer, then averaged
RIVER_MIN_M2 = 2000.0
RIVER_TOLERANCE_M = 10.0     # each corner of a river is paid for on every pixel of its box
RIVER_CORE_M = 15.0          # the level of a river is read this far from its banks


def road_width(p):
    """Carriageway width. BD TOPO often gives the width of the traffic lanes only
    (3-4 m in the Presqu'ile) while the asphalt runs kerb to kerb, parking lanes
    included: at least 3.2 m per lane and 6 m in all."""
    w = float(p.get("largeur_de_chaussee") or 0)
    return max(w, 3.2 * (p.get("nombre_de_voies") or 1), 6.0)


def is_pedestrian(p):
    return p.get("nature") in ("Sentier", "Escalier") or (p.get("vitesse_moyenne_vl") == 0)


def terrain_tiles(base):
    import terrain_file
    return terrain_file.read_glb(os.path.join(base, "terrain", "terrain.glb"))


def terrain_extent(tiles):
    """(x0, y0, x1, y1) in Unreal cm of the terrain tiles of the zone."""
    import terrain_file
    boxes = [terrain_file.box(pts) for _, pts in tiles]
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))


def rivers(base, extent, tiles):
    """[{"name", "area_m2", "level_cm", "ring"}] of the rivers inside the extent: outline in Unreal
    cm with few corners (the material pays for each on every pixel near the river, and only
    paints water below level_cm + a margin, so the bank is where the terrain drops, not where a
    coarse outline runs), level = median height of the terrain well inside the outline (the
    LiDAR model is flat on the water)."""
    path = os.path.join(base, "water.geojson")
    if not os.path.exists(path):
        return []
    from shapely.geometry import Point, box
    import terrain_file
    z, gx0, gy0, step = terrain_file.grid(tiles)
    x0, y0, x1, y1 = (v / 100.0 for v in extent)
    frame = box(x0, -y1, x1, -y0)       # local metres, y north
    out = []
    with open(path, encoding="utf-8") as f:
        feats = json.load(f)["features"]
    for ft in feats:
        if ft["properties"].get("nature") != "Ecoulement naturel":
            continue
        g = shape(ft["geometry"]).buffer(0).intersection(frame)
        for poly in getattr(g, "geoms", [g]):
            if poly.geom_type != "Polygon" or poly.area < RIVER_MIN_M2:
                continue
            core = poly.buffer(-RIVER_CORE_M)
            hs = sorted(z[r][c] for r in range(0, len(z), 3) for c in range(0, len(z[0]), 3) if z[r][c] is not None
                        and core.contains(Point((gx0 + c * step) / 100.0, -(gy0 + r * step) / 100.0)))
            ring = list(poly.simplify(RIVER_TOLERANCE_M).exterior.coords)[:-1]
            out.append({"name": ft["properties"].get("cleabs", ""), "area_m2": round(poly.area),
                        "level_cm": round(hs[len(hs) // 2], 1) if hs else None,
                        "level_spread_cm": round(hs[-len(hs) // 20] - hs[len(hs) // 20], 1) if hs else None,
                        "ring": [[round(x * 100.0), round(-y * 100.0)] for x, y in ring]})
    return out


def focus_extent(gen):
    """(x0, y0, x1, y1) in Unreal cm of the focus mask: the generated buildings and a margin."""
    made = os.path.join(gen, "ground", "ground.json")
    if not os.path.exists(os.path.join(gen, "buildings_gen.csv")) and os.path.exists(made):
        with open(made, encoding="utf-8") as f:
            return tuple(json.load(f)["extent_ue_cm"])       # the focus mask as it was made
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    xs = [float(r["ue_x"]) for r in rows]
    ys = [float(r["ue_y"]) for r in rows]
    return min(xs) - MARGIN_CM, min(ys) - MARGIN_CM, max(xs) + MARGIN_CM, max(ys) + MARGIN_CM


def circuit_points(path):
    """The line the flight drives, [(x, y)] in Unreal cm, from a <Map>.circuit.csv."""
    with open(path, encoding="utf-8") as f:
        rows = csv.DictReader(line for line in f if not line.startswith("#"))
        out = []
        for row in rows:
            for p in (row.get("line_ue_cm") or "").split(";"):
                v = p.split()
                if len(v) >= 2:
                    out.append((float(v[0]), float(v[1])))
    return out


def route_box(points, focus, terrain, margin_cm=ROUTE_MARGIN_CM):
    """The box of the route mask: around the points of the driven line the focus mask does not
    hold, a margin wider, kept on the terrain. None when the focus mask holds the whole line."""
    fx0, fy0, fx1, fy1 = focus
    out = [(x, y) for x, y in points if not (fx0 <= x <= fx1 and fy0 <= y <= fy1)]
    if not out:
        return None
    x0, y0 = min(p[0] for p in out) - margin_cm, min(p[1] for p in out) - margin_cm
    x1, y1 = max(p[0] for p in out) + margin_cm, max(p[1] for p in out) + margin_cm
    tx0, ty0, tx1, ty1 = terrain
    return max(x0, tx0), max(y0, ty0), min(x1, tx1), min(y1, ty1)


def offset_line(pts, d):
    """pts moved d to the left of their direction (canvas pixels), corner by corner."""
    out = []
    for i, (x, y) in enumerate(pts):
        ax, ay = pts[max(i - 1, 0)]
        bx, by = pts[min(i + 1, len(pts) - 1)]
        n = math.hypot(bx - ax, by - ay)
        if n == 0:
            out.append((x, y))
            continue
        out.append((x + (by - ay) / n * d, y - (bx - ax) / n * d))
    return out


def lane_offsets(p, width_m):
    """Where the dashed lines of a road run, in metres from its centre line. A two-way road of two
    lanes or more has its centre line, as before; a one-way road has a line between each pair of
    its lanes (the quays are four lanes one way: three lines), if the lanes are wide enough."""
    n = p.get("nombre_de_voies") or 1
    if n < 2:
        return []
    if p.get("sens_de_circulation") == "Double sens":
        return [0.0] if width_m >= 6 else []
    if p.get("sens_de_circulation") not in ("Sens direct", "Sens inverse") or width_m / n < LANE_MIN_M:
        return []
    return [(k - n / 2.0) * width_m / n for k in range(1, n)]


def probe(zone, focus, points, processed=None):
    """What each mask of this machine says under the driven line: {mask: {"extent_ue_cm",
    "size_px", "points", "asphalt", "marking", "paving", "kerb", "sidewalk"}} for the focus, far
    and route masks that exist, counting the points of the line inside each. Gate 86: the quay
    the flight drives was pale and smooth in the game while the far mask made in the cloud has
    asphalt there, and the focus mask of the PC, made days before, could not be read from here."""
    base = processed or os.path.join(REPO, "assets", "geo", "processed", zone)
    files = {"focus": (os.path.join(base, "gen", focus, "ground", "T_GroundMask.png"),
                       os.path.join(base, "gen", focus, "ground", "ground.json")),
             "far": (os.path.join(base, "ground_far", "T_GroundMaskFar.png"),
                     os.path.join(base, "ground_far", "ground_far.json")),
             "route": (os.path.join(base, "ground_far", "T_GroundMaskRoute.png"),
                       os.path.join(base, "ground_far", "ground_route.json"))}
    out = {}
    for name, (png, meta_path) in files.items():
        if not (os.path.exists(png) and os.path.exists(meta_path)):
            continue
        with open(meta_path, encoding="utf-8") as f:
            x0, y0, x1, y1 = json.load(f)["extent_ue_cm"]
        img = Image.open(png).convert("RGB")
        w, h = img.size
        n = {"extent_ue_cm": [round(v) for v in (x0, y0, x1, y1)], "size_px": [w, h], "points": 0,
             "asphalt": 0, "marking": 0, "paving": 0, "kerb": 0, "sidewalk": 0}
        for x, y in points:
            if not (x0 <= x < x1 and y0 <= y < y1):
                continue
            r, g, b = img.getpixel((int((x - x0) / (x1 - x0) * w), int((y - y0) / (y1 - y0) * h)))
            n["points"] += 1
            n["marking" if r > 127 and b > 127 else "asphalt" if r > 127 else "paving" if g > 127
              else "kerb" if b > 127 else "sidewalk"] += 1
        out[name] = n
    return out


def build(zone, focus, far=False, processed=None, far_texels=FAR_TEXELS, route=None):
    """route: the points of the driven line (circuit_points): the route mask, as fine as the
    focus mask, around the part of the line the focus mask does not hold. None is returned, and
    nothing written, when it holds the whole line."""
    base = processed or os.path.join(REPO, "assets", "geo", "processed", zone)
    gen = os.path.join(base, "gen", focus)
    if route is not None:
        far = False
        box = route_box(route, focus_extent(gen), terrain_extent(terrain_tiles(base)))
        if box is None:
            return None
        x0, y0, x1, y1 = box
    elif far:
        tiles = terrain_tiles(base)
        x0, y0, x1, y1 = terrain_extent(tiles)
    else:
        x0, y0, x1, y1 = focus_extent(gen)
    # Power-of-two sides (mipmaps in Unreal); texels may be slightly non-square.
    texels = far_texels if far else (ROUTE_TEXELS if route is not None else TEXELS_LONG)
    long_cm = max(x1 - x0, y1 - y0)
    w = h = texels
    if far:
        pass                    # the terrain is nearly square: 4096 both ways
    elif x1 - x0 < y1 - y0:
        w = 2 ** int(round(math.log2(texels * (x1 - x0) / long_cm)))
    else:
        h = 2 ** int(round(math.log2(texels * (y1 - y0) / long_cm)))
    sx, sy = (x1 - x0) / w, (y1 - y0) / h
    cm_per_px = max(sx, sy)
    # Draw on square texels (the finer axis) and resample to (w, h) at the end:
    # PIL widths are in pixels, so drawing straight onto non-square texels made the
    # streets across the short axis too narrow (6 m carriageways came out 3.6 m).
    s = min(sx, sy) / (FAR_FINER if far else 1)
    cw, ch = int(math.ceil((x1 - x0) / s)), int(math.ceil((y1 - y0) / s))

    def px(pt):  # local metres (y north) -> canvas pixel (Unreal Y = -north)
        return ((pt[0] * 100.0 - x0) / s, (-pt[1] * 100.0 - y0) / s)

    with open(os.path.join(base, "roads.geojson"), encoding="utf-8") as f:
        roads = json.load(f)["features"]
    layers = {k: Image.new("L", (cw, ch), 0) for k in ("asphalt", "kerb", "paving", "mark")}
    draw = {k: ImageDraw.Draw(v) for k, v in layers.items()}
    m2px = 100.0 / s
    # Gate 86: no lane line showed on the quay. A line one texel wide is lost to the filtering of
    # the texture a few metres from the camera: in the focus and route masks a line is two texels
    # of the coarser axis at least (29 cm in the focus mask). The far mask is seen from afar.
    mark_w_m = MARK_W_M if far else max(MARK_W_M, 2.0 * cm_per_px / 100.0)
    counts = {"asphalt": 0, "paving": 0, "marked": 0, "lane_lines": 0, "tunnel": 0}

    def line(d, pts, width_m):
        wpx = max(1, int(round(width_m * m2px)))
        d.line(pts, fill=255, width=wpx, joint="curve")
        r = wpx / 2.0
        for x, y in (pts[0], pts[-1]):
            d.ellipse((x - r, y - r, x + r, y + r), fill=255)

    # Carriageways fitted to the Grand Lyon kerbs (carriageway.py), where available: the
    # asphalt and kerbs then match the LiDAR kerbs the cars drive on.
    fitted = {}
    cw_path = os.path.join(base, "carriageway.geojson")
    if os.path.exists(cw_path):
        with open(cw_path, encoding="utf-8") as f:
            for ft in json.load(f)["features"]:
                fitted.setdefault(ft["properties"]["road_id"], []).append(ft)
    counts["fitted_to_kerbs"] = 0

    def poly_px(geom):
        for g in getattr(geom, "geoms", [geom]):
            if g.geom_type == "Polygon" and not g.is_empty:
                yield [px(c) for c in g.exterior.coords]

    for feat in roads:
        p = feat["properties"]
        if (p.get("position_par_rapport_au_sol") or "0") not in ("0", 0):
            counts["tunnel"] += 1
            continue
        if p.get("nature") == "Bac ou liaison maritime":
            continue
        fit = fitted.get(p.get("cleabs")) if not is_pedestrian(p) else None
        if fit:
            counts["fitted_to_kerbs"] += 1
            for ft in fit:
                g = shape(ft["geometry"])
                for ring in poly_px(g.buffer(KERB_M)):
                    draw["kerb"].polygon(ring, fill=255)
                for ring in poly_px(g):
                    draw["asphalt"].polygon(ring, fill=255)
                # Round caps at both ends close the joints between pieces at junctions.
                hw = ft["properties"]["half_width_m"]
                cen = [px(c) for c in ft["properties"]["centre"]]
                for (x, y), half_m in ((cen[0], hw[0]), (cen[-1], hw[-1])):
                    r = half_m * m2px
                    draw["asphalt"].ellipse((x - r, y - r, x + r, y + r), fill=255)
                    rk = r + KERB_M * m2px
                    draw["kerb"].ellipse((x - rk, y - rk, x + rk, y + rk), fill=255)
            centres = [[px(c) for c in ft["properties"]["centre"]] for ft in fit]
            wd = 2.0 * min(min(ft["properties"]["half_width_m"]) for ft in fit)
        else:
            centres = None
        for pts in (centres or [[px(c) for c in coords] for coords in feat["geometry"]["coordinates"]]):
            if len(pts) < 2:
                continue
            if centres is None:
                wd = road_width(p)
            if is_pedestrian(p):
                # Streets closed to cars are paved from facade to facade (the buildings
                # hide the excess): rue de la Republique is granite all across.
                line(draw["paving"], pts, PEDESTRIAN_W_M)
                counts["paving"] += 1
                continue
            if centres is None:
                line(draw["asphalt"], pts, wd)
                line(draw["kerb"], pts, wd + 2 * KERB_M)
            counts["asphalt"] += 1
            lines = lane_offsets(p, wd)
            if lines:
                counts["marked"] += 1
            for off in lines:
                counts["lane_lines"] += 1
                run = offset_line(pts, off * m2px) if off else pts
                period = (DASH_M + GAP_M) * m2px
                carry = 0.0
                for a, b in zip(run, run[1:]):
                    seg = math.hypot(b[0] - a[0], b[1] - a[1])
                    if seg == 0:
                        continue
                    t = -carry
                    while t < seg:
                        s0, s1 = max(t, 0.0), min(t + DASH_M * m2px, seg)
                        if s1 > s0:
                            ax = a[0] + (b[0] - a[0]) * s0 / seg
                            ay = a[1] + (b[1] - a[1]) * s0 / seg
                            bx = a[0] + (b[0] - a[0]) * s1 / seg
                            by = a[1] + (b[1] - a[1]) * s1 / seg
                            draw["mark"].line([(ax, ay), (bx, by)], fill=255,
                                              width=max(1, int(round(mark_w_m * m2px))))
                        t += period
                    carry = (seg + carry) % period

    if not far:
        layers = {k: v.resize((w, h), Image.BILINEAR, box=(0, 0, w * sx / s, h * sy / s)) for k, v in layers.items()}
    asphalt = np.asarray(layers["asphalt"], np.uint8)
    kerb = np.asarray(layers["kerb"], np.uint8)
    paving = np.asarray(layers["paving"], np.uint8)
    kerb = np.where(asphalt > 0, 0, kerb)
    paving = np.where((asphalt > 0) | (kerb > 0), 0, paving)
    mark = np.where(asphalt > 0, np.asarray(layers["mark"], np.uint8), 0)
    rgb = np.dstack([asphalt, paving, np.maximum(mark, kerb)]).astype(np.uint8)
    if far:
        # composed on the fine canvas (a kerb one texel wide would not survive being averaged
        # before the asphalt is cut out of it), then averaged down
        img = Image.fromarray(rgb, "RGB").resize((w, h), Image.BOX, box=(0, 0, w * sx / s, h * sy / s))
        out = os.path.join(base, "ground_far")
        name, meta_name = "T_GroundMaskFar.png", "ground_far.json"
    elif route is not None:
        img = Image.fromarray(rgb, "RGB").filter(ImageFilter.SMOOTH)
        out = os.path.join(base, "ground_far")
        name, meta_name = "T_GroundMaskRoute.png", "ground_route.json"
    else:
        img = Image.fromarray(rgb, "RGB").filter(ImageFilter.SMOOTH)
        out = os.path.join(gen, "ground")
        name, meta_name = "T_GroundMask.png", "ground.json"
    os.makedirs(out, exist_ok=True)
    img.save(os.path.join(out, name), optimize=True)
    meta = {"extent_ue_cm": [x0, y0, x1, y1], "size_px": [w, h], "cm_per_px": round(cm_per_px, 3),
            "channels": "R asphalt, G pedestrian paving, B marking (on R) or kerb (off R); else sidewalk", "counts": counts,
            "coverage": {"asphalt": round(float((asphalt > 0).mean()), 3),
                         "paving": round(float((paving > 0).mean()), 3)}}
    if far:
        meta["rivers"] = rivers(base, (x0, y0, x1, y1), tiles)
        meta["sources"] = "IGN BD TOPO (roads, water) and Grand Lyon kerbs, Licence Ouverte Etalab 2.0"
    with open(os.path.join(out, meta_name), "w") as f:
        json.dump(meta, f, indent=2 if not far else None, separators=None if not far else (",", ":"))
    return meta


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--zone", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--far", action="store_true", help="the far mask, over the whole terrain")
    ap.add_argument("--route", metavar="CIRCUIT_CSV",
                    help="the route mask: as fine as the focus mask, around the part of this circuit past it")
    ap.add_argument("--probe", metavar="CIRCUIT_CSV",
                    help="make nothing: print what the masks of this machine say under this circuit")
    ap.add_argument("--processed", help="folder of the zone's processed data (default assets/geo/processed/<zone>)")
    a = ap.parse_args()
    if a.probe:
        print(json.dumps(probe(a.zone, a.focus, circuit_points(a.probe), a.processed)))
        return
    meta = build(a.zone, a.focus, a.far, a.processed, route=circuit_points(a.route) if a.route else None)
    if meta is None:
        stale = os.path.join(a.processed or os.path.join(REPO, "assets", "geo", "processed", a.zone), "ground_far")
        for name in ("T_GroundMaskRoute.png", "ground_route.json"):
            if os.path.exists(os.path.join(stale, name)):
                os.remove(os.path.join(stale, name))        # the mask of an older circuit
        print(json.dumps({"route_mask": "the focus mask holds the whole circuit"}))
        return
    print(json.dumps({k: v for k, v in meta.items() if k != "rivers"}, indent=2))


if __name__ == "__main__":
    main()
