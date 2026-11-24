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


def build(zone, focus, far=False, processed=None, far_texels=FAR_TEXELS):
    base = processed or os.path.join(REPO, "assets", "geo", "processed", zone)
    gen = os.path.join(base, "gen", focus)
    if far:
        tiles = terrain_tiles(base)
        x0, y0, x1, y1 = terrain_extent(tiles)
    else:
        with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        xs = [float(r["ue_x"]) for r in rows]
        ys = [float(r["ue_y"]) for r in rows]
        x0, x1 = min(xs) - MARGIN_CM, max(xs) + MARGIN_CM
        y0, y1 = min(ys) - MARGIN_CM, max(ys) + MARGIN_CM
    # Power-of-two sides (mipmaps in Unreal); texels may be slightly non-square.
    texels = far_texels if far else TEXELS_LONG
    long_cm = max(x1 - x0, y1 - y0)
    w = h = texels
    if far:
        pass                    # the terrain is nearly square: 4096 both ways
    elif x1 - x0 < y1 - y0:
        w = 2 ** int(round(math.log2(TEXELS_LONG * (x1 - x0) / long_cm)))
    else:
        h = 2 ** int(round(math.log2(TEXELS_LONG * (y1 - y0) / long_cm)))
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
    counts = {"asphalt": 0, "paving": 0, "marked": 0, "tunnel": 0}

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
            if (p.get("nombre_de_voies") or 1) >= 2 and wd >= 6 and p.get("sens_de_circulation") == "Double sens":
                counts["marked"] += 1
                period = (DASH_M + GAP_M) * m2px
                carry = 0.0
                for a, b in zip(pts, pts[1:]):
                    seg = math.hypot(b[0] - a[0], b[1] - a[1])
                    t = -carry
                    while t < seg:
                        s0, s1 = max(t, 0.0), min(t + DASH_M * m2px, seg)
                        if s1 > s0:
                            ax = a[0] + (b[0] - a[0]) * s0 / seg
                            ay = a[1] + (b[1] - a[1]) * s0 / seg
                            bx = a[0] + (b[0] - a[0]) * s1 / seg
                            by = a[1] + (b[1] - a[1]) * s1 / seg
                            draw["mark"].line([(ax, ay), (bx, by)], fill=255,
                                              width=max(1, int(round(MARK_W_M * m2px))))
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
    ap.add_argument("--processed", help="folder of the zone's processed data (default assets/geo/processed/<zone>)")
    a = ap.parse_args()
    meta = build(a.zone, a.focus, a.far, a.processed)
    print(json.dumps({k: v for k, v in meta.items() if k != "rivers"}, indent=2))


if __name__ == "__main__":
    main()
