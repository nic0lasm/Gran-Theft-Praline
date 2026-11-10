"""Surface grid of a focus street for the footsteps (read by UGTPAudioMix).

The ground of the generated city is a single material driven by a mask
(tools/city/ground_mask.py), so the hit material cannot tell asphalt from a
sidewalk. This tool turns that mask into a coarse grid of surface ids:
  R (asphalt carriageway, markings) -> Asphalt
  B off R (kerb stones), G (pedestrian paving), the rest (sidewalk slabs) -> Stone
Optional layers, off by default because the ground must sound like it looks
(turn them on once the ground material paints them):
  --vegetation <vegetation.geojson>  BD TOPO vegetation (not hedges) -> Grass
  --areas <areas.geojson>            polygons with a "surface" property
                                     (Gravel, Cobble, Grass, Metal, Wood, Water, ...)
Coordinates of the geojson layers: local metres, x east, y north (as in
assets/geo/processed/<zone>/); Unreal X = east, Y = -north, in cm.
Asphalt is never overwritten: a road stays a road.

Output (binary, layout in GTPSurfaceLogic.h, not committed: derived from OSM/BD TOPO):
  GranTheftPraline/Content/Sound/SurfaceMap/<focus>.gtpsurf
  plus <focus>.png, a colour preview.

  python3 -I tools/audio/surface_map.py --ground <gen/<focus>/ground> [--cell 50] [--out FILE]
"""

import argparse
import json
import os
import struct

import numpy as np
from PIL import Image, ImageDraw

SURFACES = ["Asphalt", "Stone", "Cobble", "Gravel", "Grass", "Metal", "Wood", "Water"]
ID = {name: i for i, name in enumerate(SURFACES)}
PREVIEW = {
    "Asphalt": (40, 40, 46), "Stone": (190, 182, 165), "Cobble": (130, 110, 95), "Gravel": (190, 90, 60),
    "Grass": (70, 150, 60), "Metal": (90, 120, 160), "Wood": (150, 100, 50), "Water": (60, 120, 220),
}
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
NOT_WALKABLE = {"Haie"}  # hedges: you walk beside them, not on them


def load_ground(ground_dir):
    with open(os.path.join(ground_dir, "ground.json"), encoding="utf-8") as f:
        meta = json.load(f)
    mask = np.asarray(Image.open(os.path.join(ground_dir, "T_GroundMask.png")).convert("RGB"))
    return meta, mask


def classify_mask(meta, mask, cell_cm):
    """Grid of ids from the mask; cell (i, j) covers X = x0 + i * cell, Y = y0 + j * cell."""
    x0, y0, x1, y1 = meta["extent_ue_cm"]
    h, w = mask.shape[:2]
    nx = int(np.ceil((x1 - x0) / cell_cm))
    ny = int(np.ceil((y1 - y0) / cell_cm))
    # Sample the mask at the centre of each cell (nearest texel).
    cx = x0 + (np.arange(nx) + 0.5) * cell_cm
    cy = y0 + (np.arange(ny) + 0.5) * cell_cm
    px = np.clip(((cx - x0) / (x1 - x0) * w).astype(int), 0, w - 1)
    py = np.clip(((cy - y0) / (y1 - y0) * h).astype(int), 0, h - 1)
    sample = mask[py][:, px]  # (ny, nx, 3)
    grid = np.full((ny, nx), ID["Stone"], np.uint8)
    grid[sample[..., 0] > 127] = ID["Asphalt"]
    return grid, (x0, y0)


def rasterise(grid, origin, cell_cm, polygons):
    """Paints (surface_id, [rings in local metres]) onto the grid, never over asphalt."""
    ny, nx = grid.shape
    x0, y0 = origin
    for surface_id, rings in polygons:
        layer = Image.new("L", (nx, ny), 0)
        draw = ImageDraw.Draw(layer)
        for ring in rings:
            pts = [((x * 100.0 - x0) / cell_cm, (-y * 100.0 - y0) / cell_cm) for x, y in ring[:, :2]]
            if len(pts) >= 3:
                draw.polygon(pts, fill=255)
        hit = (np.asarray(layer) > 0) & (grid != ID["Asphalt"])
        grid[hit] = surface_id
    return grid


def polygons_of(geojson_path, surface_of):
    with open(geojson_path, encoding="utf-8") as f:
        features = json.load(f)["features"]
    out = []
    for feat in features:
        surface = surface_of(feat.get("properties") or {})
        if surface is None:
            continue
        geom = feat["geometry"]
        polys = geom["coordinates"] if geom["type"] == "MultiPolygon" else [geom["coordinates"]]
        for poly in polys:
            # Outer ring only: courtyards inside a park are rare at this scale.
            out.append((ID[surface], [np.asarray(poly[0], float)]))
    return out


def write(grid, origin, cell_cm, out_path):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    ny, nx = grid.shape
    with open(out_path, "wb") as f:
        f.write(b"GTPSURF1")
        f.write(struct.pack("<iifff", nx, ny, origin[0], origin[1], cell_cm))
        f.write(np.ascontiguousarray(grid, np.uint8).tobytes())
    palette = np.array([PREVIEW[s] for s in SURFACES], np.uint8)
    Image.fromarray(palette[grid]).save(os.path.splitext(out_path)[0] + ".png", optimize=True)


def build(ground_dir, out_path, cell_cm=50.0, vegetation=None, areas=None):
    meta, mask = load_ground(ground_dir)
    grid, origin = classify_mask(meta, mask, cell_cm)
    layers = []
    if vegetation:
        layers += polygons_of(vegetation, lambda p: None if p.get("nature") in NOT_WALKABLE else "Grass")
    if areas:
        layers += polygons_of(areas, lambda p: p.get("surface") if p.get("surface") in ID else None)
    grid = rasterise(grid, origin, cell_cm, layers)
    write(grid, origin, cell_cm, out_path)
    counts = {s: int((grid == i).sum()) for i, s in enumerate(SURFACES) if (grid == i).any()}
    return grid, counts


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ground", required=True, help="gen/<focus>/ground folder (T_GroundMask.png, ground.json)")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--cell", type=float, default=50.0, help="cell size, cm")
    ap.add_argument("--vegetation")
    ap.add_argument("--areas")
    ap.add_argument("--out")
    args = ap.parse_args()
    out = args.out or os.path.join(REPO, "GranTheftPraline", "Content", "Sound", "SurfaceMap", args.focus + ".gtpsurf")
    grid, counts = build(args.ground, out, args.cell, args.vegetation, args.areas)
    total = grid.size
    share = ", ".join(f"{k} {v / total:.1%}" for k, v in counts.items())
    print(f"{out}: {grid.shape[1]} x {grid.shape[0]} cells of {args.cell:.0f} cm ({os.path.getsize(out) / 1e6:.2f} MB); {share}")


if __name__ == "__main__":
    main()
