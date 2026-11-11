"""Ambience zones of the map for the ambience bed (read by UGTPAudioMix).

A coarse grid (2 m cells, same file layout as the surface grid, see
GTPSurfaceLogic.h) where each cell says which ambience layer leads:
  0 Street      the default street loop
  1 Pedestrian  streets closed to cars (G channel of the ground mask: rue de la République)
  2 Quay        within QUAY_M of the river, on land (quai du Rhône)
  3 Bridge      above the water (pont Lafayette: the only way to stand there)
  4 Square      open ground further than SQUARE_M from every building, off the carriageway
                (place Bellecour, place des Jacobins), when gen/<focus>/buildings_gen.csv is there
Inputs: the ground mask of the focus street (tools/city/ground_mask.py) and
water.geojson of the zone (BD TOPO, local metres, x east, y north). The grid
covers the mask and stretches east to the far bank of the Rhône.

Output (not committed: derived from BD TOPO):
  GranTheftPraline/Content/Sound/SurfaceMap/zones-<focus>.gtpsurf (+ .png preview)

  python3 -I tools/audio/zone_map.py --ground <gen/<focus>/ground> --water <water.geojson> [--out FILE]
"""

import argparse
import csv
import json
import os
import struct

import numpy as np
from PIL import Image, ImageDraw

ZONES = ["Street", "Pedestrian", "Quay", "Bridge", "Square"]
PREVIEW = [(150, 150, 150), (220, 190, 120), (60, 160, 120), (200, 60, 60), (200, 180, 60)]
QUAY_M = 45.0
SQUARE_M = 30.0
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))


def water_rings(path):
    with open(path, encoding="utf-8") as f:
        features = json.load(f)["features"]
    rings = []
    for feat in features:
        if (feat.get("properties") or {}).get("nature") == "Réservoir-bassin":
            continue  # fountains and basins are not the river
        geom = feat["geometry"]
        polys = geom["coordinates"] if geom["type"] == "MultiPolygon" else [geom["coordinates"]]
        for poly in polys:
            rings.append(np.asarray(poly[0], float))
    return rings


def building_boxes(ground_dir):
    """World boxes (Unreal cm: x0, y0, x1, y1) of the generated buildings, [] when unknown."""
    path = os.path.join(os.path.dirname(os.path.abspath(ground_dir)), "buildings_gen.csv")
    if not os.path.isfile(path):
        return []
    boxes = []
    with open(path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            try:
                bx0, by0, _, bx1, by1, _ = (float(v) for v in row["bounds_ue_cm"].split())
                ux, uy = float(row["ue_x"]), float(row["ue_y"])
            except (KeyError, ValueError):
                continue
            boxes.append((ux + bx0, uy + by0, ux + bx1, uy + by1))
    return boxes


def dilate(mask, cells):
    """Binary dilation by a square of +-cells (repeated 3x3 max), numpy only."""
    out = mask.copy()
    for _ in range(cells):
        grown = out.copy()
        grown[1:, :] |= out[:-1, :]
        grown[:-1, :] |= out[1:, :]
        grown[:, 1:] |= out[:, :-1]
        grown[:, :-1] |= out[:, 1:]
        out = grown
    return out


def build(ground_dir, water_path, out_path, cell_cm=200.0, east_cm=None):
    with open(os.path.join(ground_dir, "ground.json"), encoding="utf-8") as f:
        meta = json.load(f)
    mask = np.asarray(Image.open(os.path.join(ground_dir, "T_GroundMask.png")).convert("RGB"))
    mx0, my0, mx1, my1 = meta["extent_ue_cm"]
    rings = water_rings(water_path)
    # East edge: the far bank of the river (presqu'île: the Rhône), at most 800 m past the mask.
    if east_cm is None:
        east_cm = mx1
        for ring in rings:
            east_cm = max(east_cm, min(float(ring[:, 0].max()) * 100.0 + 2000.0, mx1 + 80000.0))
    x0, y0, x1, y1 = mx0, my0, east_cm, my1
    nx, ny = int(np.ceil((x1 - x0) / cell_cm)), int(np.ceil((y1 - y0) / cell_cm))
    grid = np.zeros((ny, nx), np.uint8)

    # Pedestrian streets from the mask (G without R), where the mask exists.
    h, w = mask.shape[:2]
    cx = x0 + (np.arange(nx) + 0.5) * cell_cm
    cy = y0 + (np.arange(ny) + 0.5) * cell_cm
    inside_x = cx < mx1
    px = np.clip(((cx - mx0) / (mx1 - mx0) * w).astype(int), 0, w - 1)
    py = np.clip(((cy - my0) / (my1 - my0) * h).astype(int), 0, h - 1)
    sample = mask[py][:, px]
    pedestrian = (sample[..., 1] > 127) & (sample[..., 0] <= 127)
    pedestrian[:, ~inside_x] = False
    grid[pedestrian] = 1

    # Squares: open ground far from every building and off the carriageway (R), inside the mask.
    boxes = building_boxes(ground_dir)
    if boxes:
        built = np.zeros((ny, nx), bool)
        for bx0, by0, bx1, by1 in boxes:
            i0, i1 = int(np.floor((bx0 - x0) / cell_cm)), int(np.ceil((bx1 - x0) / cell_cm))
            j0, j1 = int(np.floor((by0 - y0) / cell_cm)), int(np.ceil((by1 - y0) / cell_cm))
            built[max(j0, 0):max(j1, 0), max(i0, 0):max(i1, 0)] = True
        near_building = dilate(built, int(round(SQUARE_M * 100.0 / cell_cm)))
        carriageway = sample[..., 0] > 127
        square = ~near_building & ~carriageway
        square[:, ~inside_x] = False
        grid[square] = 4

    layer = Image.new("L", (nx, ny), 0)
    draw = ImageDraw.Draw(layer)
    for ring in rings:
        pts = [((x * 100.0 - x0) / cell_cm, (-y * 100.0 - y0) / cell_cm) for x, y in ring[:, :2]]
        if len(pts) >= 3:
            draw.polygon(pts, fill=255)
    water = np.asarray(layer) > 0
    quay = dilate(water, int(round(QUAY_M * 100.0 / cell_cm))) & ~water
    grid[quay] = 2
    grid[water] = 3

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(b"GTPSURF1")
        f.write(struct.pack("<iifff", nx, ny, x0, y0, cell_cm))
        f.write(np.ascontiguousarray(grid).tobytes())
    Image.fromarray(np.array(PREVIEW, np.uint8)[grid]).save(os.path.splitext(out_path)[0] + ".png", optimize=True)
    return grid


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ground", required=True)
    ap.add_argument("--water", required=True)
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--cell", type=float, default=200.0)
    ap.add_argument("--out")
    args = ap.parse_args()
    out = args.out or os.path.join(REPO, "GranTheftPraline", "Content", "Sound", "SurfaceMap", f"zones-{args.focus}.gtpsurf")
    grid = build(args.ground, args.water, out, args.cell)
    share = ", ".join(f"{z} {(grid == i).mean():.1%}" for i, z in enumerate(ZONES))
    print(f"{out}: {grid.shape[1]} x {grid.shape[0]} cells of {args.cell:.0f} cm; {share}")


if __name__ == "__main__":
    main()
