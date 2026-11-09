#!/usr/bin/env python3
"""Terrain of a zone from the IGN LiDAR HD terrain model (MNT, 0.5 m).

Output (regenerated, not committed): assets/geo/processed/<name>/terrain/
  heightmap.png    16-bit greyscale for an Unreal Landscape (size 1009/2017/4033)
  terrain.glb      the same ground as static mesh tiles (one node per tile), for
                   the greybox and for anything that does not want a Landscape
  terrain.json     Landscape import settings (location, scale) and statistics

Unreal Landscape height: z_cm = location_z + (value - 32768) * scale_z / 128.

Usage: python3 tools/city/terrain.py --name presqu-ile
"""

import argparse
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import meshkit  # noqa: E402
from citylib import to_unreal as _to_ue  # noqa: E402


def cl_to_ue(o):
    return _to_ue(*o)
import raster  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LANDSCAPE_SIZES = (505, 1009, 2017, 4033, 8129)  # recommended Unreal Landscape resolutions


def fill_nodata(grid):
    """Replace NaN cells by their nearest valid neighbour."""
    from scipy import ndimage
    bad = np.isnan(grid)
    if not bad.any():
        return grid
    idx = ndimage.distance_transform_edt(bad, return_distances=False, return_indices=True)
    return grid[tuple(idx)]


def landscape_size(extent_m, max_spacing_m):
    for n in LANDSCAPE_SIZES:
        if extent_m / (n - 1) <= max_spacing_m:
            return n
    return LANDSCAPE_SIZES[-1]


def heightmap(mnt, origin, origin_alt, max_spacing_m=1.0):
    xmin, ymin, xmax, ymax = mnt.env
    extent = max(xmax - xmin, ymax - ymin)
    n = landscape_size(extent, max_spacing_m)
    spacing = extent / (n - 1)
    xs = xmin + np.arange(n) * spacing
    ys = ymax - np.arange(n) * spacing  # row 0 = north, like Unreal (+Y = south)
    gx, gy = np.meshgrid(xs, ys)
    z = mnt.sample(gx.ravel(), gy.ravel()).reshape(n, n)
    z = fill_nodata(z)
    lo, hi = float(z.min()), float(z.max())
    mid = (lo + hi) / 2
    # scale_z (percent) so the range fits 16 bits with 5 % headroom: half range = 256 m * scale_z / 100
    scale_z = max(1.0, math.ceil((hi - lo) / 2 * 1.05 / 256.0 * 100.0 * 100) / 100)
    unit_m = scale_z / 100.0 / 128.0  # metres per height step
    values = np.clip(np.round(32768 + (z - mid) / unit_m), 0, 65535).astype(np.uint16)
    settings = {
        "resolution": n, "spacing_m": round(spacing, 6),
        "location_ue_cm": [round((xmin - origin[0]) * 100, 1), round(-(ymax - origin[1]) * 100, 1),
                           round((mid - origin_alt) * 100, 1)],
        "scale_ue": [round(spacing * 100, 4), round(spacing * 100, 4), scale_z],
        "height_step_cm": round(unit_m * 100, 4), "altitude_min_m": round(lo, 2), "altitude_max_m": round(hi, 2),
        "formula": "z_cm = location_z + (value - 32768) * scale_z / 128",
    }
    return values, z, settings


def tiles_mesh(mnt, origin, origin_alt, tile_m=256.0, step_m=4.0):
    xmin, ymin, xmax, ymax = mnt.env
    items = []
    for tx in np.arange(xmin, xmax, tile_m):
        for ty in np.arange(ymin, ymax, tile_m):
            xs = np.arange(tx, min(tx + tile_m, xmax) + 1e-6, step_m)
            ys = np.arange(ty, min(ty + tile_m, ymax) + 1e-6, step_m)
            if len(xs) < 2 or len(ys) < 2:
                continue
            gx, gy = np.meshgrid(xs, ys)
            z = fill_nodata(mnt.sample(gx.ravel(), gy.ravel()).reshape(gx.shape)) - origin_alt
            ox, oy = tx - origin[0], ty - origin[1]
            m = meshkit.Mesh()
            col = (0.45, 0.44, 0.42)
            for i in range(len(ys) - 1):
                for j in range(len(xs) - 1):
                    p = [(gx[a, b] - origin[0], gy[a, b] - origin[1], z[a, b])
                         for a, b in ((i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j))]
                    m.quad(*p, col)
            name = f"SM_Terrain_{int(round(ox)):+05d}_{int(round(oy)):+05d}".replace("+", "p").replace("-", "m")
            items.append((name, m, (ox, oy, 0.0)))
    return items


def main():
    from PIL import Image

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    ap.add_argument("--max-spacing", type=float, default=1.0, help="metres between Landscape vertices")
    ap.add_argument("--no-mesh", action="store_true")
    args = ap.parse_args()
    raw = os.path.join(args.geo, "raw", args.name)
    proc = os.path.join(args.geo, "processed", args.name)
    out = os.path.join(proc, "terrain")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    origin, origin_alt = man["origin_lambert93_m"], man["origin_altitude_m"]
    mnt = raster.Raster.load(raw, "mnt")
    values, z, settings = heightmap(mnt, origin, origin_alt, args.max_spacing)
    Image.fromarray(values).save(os.path.join(out, "heightmap.png"))
    if not args.no_mesh:
        items = tiles_mesh(mnt, origin, origin_alt)
        meshkit.write_glb(os.path.join(out, "terrain.glb"), items)
        settings["mesh_tiles"] = [{"name": n, "ue_cm": list(cl_to_ue(o))} for n, _, o in items]
        settings["mesh_triangles"] = sum(m.tris for _, m, _ in items)
    settings["source"] = "IGN LiDAR HD MNT (Licence Ouverte Etalab 2.0)"
    with open(os.path.join(out, "terrain.json"), "w") as f:
        json.dump(settings, f, indent=2)
    print(json.dumps(settings, indent=2))


if __name__ == "__main__":
    main()
