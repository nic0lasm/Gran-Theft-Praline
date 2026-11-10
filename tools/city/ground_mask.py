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
"""

import argparse
import csv
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
MARGIN_CM = 6000.0
TEXELS_LONG = 8192
KERB_M = 0.35
PEDESTRIAN_W_M = 22.0
DASH_M, GAP_M, MARK_W_M = 3.0, 3.5, 0.15


def road_width(p):
    """Carriageway width. BD TOPO often gives the width of the traffic lanes only
    (3-4 m in the Presqu'ile) while the asphalt runs kerb to kerb, parking lanes
    included: at least 3.2 m per lane and 6 m in all."""
    w = float(p.get("largeur_de_chaussee") or 0)
    return max(w, 3.2 * (p.get("nombre_de_voies") or 1), 6.0)


def is_pedestrian(p):
    return p.get("nature") in ("Sentier", "Escalier") or (p.get("vitesse_moyenne_vl") == 0)


def build(zone, focus):
    base = os.path.join(REPO, "assets", "geo", "processed", zone)
    gen = os.path.join(base, "gen", focus)
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    xs = [float(r["ue_x"]) for r in rows]
    ys = [float(r["ue_y"]) for r in rows]
    x0, x1 = min(xs) - MARGIN_CM, max(xs) + MARGIN_CM
    y0, y1 = min(ys) - MARGIN_CM, max(ys) + MARGIN_CM
    # Power-of-two sides (mipmaps in Unreal); texels may be slightly non-square.
    long_cm = max(x1 - x0, y1 - y0)
    w = h = TEXELS_LONG
    if x1 - x0 < y1 - y0:
        w = 2 ** int(round(math.log2(TEXELS_LONG * (x1 - x0) / long_cm)))
    else:
        h = 2 ** int(round(math.log2(TEXELS_LONG * (y1 - y0) / long_cm)))
    sx, sy = (x1 - x0) / w, (y1 - y0) / h
    cm_per_px = max(sx, sy)
    # Draw on square texels (the finer axis) and resample to (w, h) at the end:
    # PIL widths are in pixels, so drawing straight onto non-square texels made the
    # streets across the short axis too narrow (6 m carriageways came out 3.6 m).
    s = min(sx, sy)
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

    for feat in roads:
        p = feat["properties"]
        if (p.get("position_par_rapport_au_sol") or "0") not in ("0", 0):
            counts["tunnel"] += 1
            continue
        if p.get("nature") == "Bac ou liaison maritime":
            continue
        for coords in feat["geometry"]["coordinates"]:
            pts = [px(c) for c in coords]
            if len(pts) < 2:
                continue
            wd = road_width(p)
            if is_pedestrian(p):
                # Streets closed to cars are paved from facade to facade (the buildings
                # hide the excess): rue de la Republique is granite all across.
                line(draw["paving"], pts, PEDESTRIAN_W_M)
                counts["paving"] += 1
                continue
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

    layers = {k: v.resize((w, h), Image.BILINEAR, box=(0, 0, w * sx / s, h * sy / s)) for k, v in layers.items()}
    asphalt = np.asarray(layers["asphalt"], np.uint8)
    kerb = np.asarray(layers["kerb"], np.uint8)
    paving = np.asarray(layers["paving"], np.uint8)
    kerb = np.where(asphalt > 0, 0, kerb)
    paving = np.where((asphalt > 0) | (kerb > 0), 0, paving)
    mark = np.where(asphalt > 0, np.asarray(layers["mark"], np.uint8), 0)
    rgb = np.dstack([asphalt, paving, np.maximum(mark, kerb)]).astype(np.uint8)
    img = Image.fromarray(rgb, "RGB").filter(ImageFilter.SMOOTH)
    out = os.path.join(gen, "ground")
    os.makedirs(out, exist_ok=True)
    img.save(os.path.join(out, "T_GroundMask.png"), optimize=True)
    meta = {"extent_ue_cm": [x0, y0, x1, y1], "size_px": [w, h], "cm_per_px": round(cm_per_px, 3),
            "channels": "R asphalt, G pedestrian paving, B marking (on R) or kerb (off R); else sidewalk", "counts": counts,
            "coverage": {"asphalt": round(float((asphalt > 0).mean()), 3),
                         "paving": round(float((paving > 0).mean()), 3)}}
    with open(os.path.join(out, "ground.json"), "w") as f:
        json.dump(meta, f, indent=2)
    return meta


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--zone", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    a = ap.parse_args()
    print(json.dumps(build(a.zone, a.focus), indent=2))


if __name__ == "__main__":
    main()
