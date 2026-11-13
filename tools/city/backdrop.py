"""Low-definition backdrop around a generated street: no wide view ends on the horizon.

The street map (generate.py --focus, 45 m around the axis) stops where its buildings stop, so
the wide views of the République slice (place Bellecour, Pathé, place de la République) open
on the void. This script gives every other BD TOPO building within --radius of the street a
closed volume: walls to the LiDAR eave in the typology colour, the LiDAR-fitted roof, no facade
modules. The volumes are merged per 250 m cell (one mesh, one draw each), so import_city.py
places them like the far proxies of the quarter: no collision, no distance field.

Input:  assets/geo (as generate.py), gen/<focus>/buildings_gen.csv (buildings left out)
Output: gen/<out>/backdrop.glb   one node per 250 m cell (SM_Backdrop_<x>_<y>)
        gen/<out>/backdrop.json  cell_m, nodes (name, cell, buildings, triangles, ue_cm), report

Usage:  python3 tools/city/backdrop.py --name presqu-ile --focus rue-de-la-republique \
          [--radius 600] [--out rue-de-la-republique]
"""

import argparse
import csv
import json
import os
import sys
import time

import numpy as np
from shapely.geometry import LineString, Point

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import generate  # noqa: E402
import meshkit  # noqa: E402
import procgen  # noqa: E402
import raster  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
SIMPLIFY_M = 0.4      # footprint simplification: the backdrop is seen from 45 m and beyond
MIN_AREA_M2 = 12.0    # sheds and kiosks are not worth a draw slot in the skyline


def focus_axis_local(report):
    return [(p[0] / 100.0, -p[1] / 100.0) for p in report.get("focus_axis_ue_cm", [])]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique", help="generated street folder under gen/")
    ap.add_argument("--radius", type=float, default=600.0, help="metres around the street axis")
    ap.add_argument("--out", help="output folder under gen/ (default: the focus folder)")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    a = ap.parse_args()
    t0 = time.time()
    raw = os.path.join(a.geo, "raw", a.name)
    proc = os.path.join(a.geo, "processed", a.name)
    gen = os.path.join(proc, "gen", a.focus)
    out = os.path.join(proc, "gen", a.out or a.focus)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(gen, "report.json"), encoding="utf-8") as f:
        axis = LineString(focus_axis_local(json.load(f)))
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        have = {r["bdtopo_id"] for r in csv.DictReader(f)}
    lm_path = os.path.join(proc, "landmarks.json")
    if os.path.exists(lm_path):
        with open(lm_path, encoding="utf-8") as f:
            have |= set(json.load(f).get("replaced_bdtopo", []))   # the landmarks bring their own volume
    # The far decor (far_decor.py, run before) brings the textured 2018 model of these buildings.
    covered = 0
    far_path = os.path.join(gen, "far_decor.json")
    if os.environ.get("GTP_CITY_FAR_DECOR", "1") != "0" and os.path.exists(far_path):
        with open(far_path, encoding="utf-8") as f:
            far = set(json.load(f).get("covered_bdtopo", []))
        covered = len(far - have)
        have |= far
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        manifest = json.load(f)
    origin, origin_alt = manifest["origin_lambert93_m"], manifest["origin_altitude_m"]
    to_world = lambda p: (p[0] + origin[0], p[1] + origin[1])  # noqa: E731
    buildings = generate.load_buildings(proc)
    mnt, mns = raster.Raster.load(raw, "mnt"), raster.Raster.load(raw, "mns")
    ring = axis.buffer(a.radius)
    chosen = [b for b in buildings if b["id"] not in have and b["poly"].area >= MIN_AREA_M2
              and ring.contains(b["poly"].centroid)]
    # No roads: every edge is a plain wall, and the volumes need no neighbour lookups.
    ctx = procgen.Context({b["key"]: b["poly"] for b in chosen}, [])
    items, tris, skipped = [], 0, 0
    for b in chosen:
        poly = b["poly"].simplify(SIMPLIFY_M, preserve_topology=True)
        if poly.is_empty or poly.geom_type != "Polygon":
            skipped += 1
            continue
        ground = generate.ground_height(poly, mnt, to_world, origin_alt)
        roof = procgen.fit_roof(poly, mns, origin_alt, ground, [], to_world,
                                float(b["height_m"]) if b["height_m"] else None, None)
        if roof["top_z"] - ground < 2.5:   # the LiDAR sees ground: demolished, or a covered passage
            skipped += 1
            continue
        roof_tris = procgen.roof_triangles(poly, roof, mns, origin_alt, to_world)
        _modules, info = procgen.layout_building(ctx, {"id": b["key"], "nature": b["nature"], "usage_1": b["usage_1"],
                                                       "wall_material": b["wall_material"]}, poly, ground, roof, [])
        mesh = meshkit.building_mesh(poly, ground, roof, [], info, roof_tris, with_modules=False)
        c = poly.centroid
        items.append(("SM_Backdrop_" + b["key"].replace("BATIMENT", ""), mesh, (c.x, c.y, ground)))
        tris += mesh.tris
    generate.write_shell_cells(out, items, prefix="SM_Backdrop_", stem="backdrop")
    with open(os.path.join(out, "backdrop.json"), encoding="utf-8") as f:
        meta = json.load(f)
    meta["report"] = {"focus": a.focus, "radius_m": a.radius, "buildings": len(items), "skipped": skipped,
                      "left_to_street_map": len(have) - covered, "left_to_far_decor": covered, "triangles": tris, "cells": len(meta["nodes"]),
                      "seconds": round(time.time() - t0, 1)}
    with open(os.path.join(out, "backdrop.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
    print(json.dumps(meta["report"]))


if __name__ == "__main__":
    main()
