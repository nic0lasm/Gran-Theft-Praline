#!/usr/bin/env python3
"""Step 2 of make_parked_static.sh: attribute-aware decimation of the raw body (meshoptimizer).

    python3 tools/vehicles/decimate_parked.py <staging>/<id>/parked --tris 8000
Reads _raw.glb, writes _dec.glb (seconds, where Blender's collapse took 18 min and missed the budget).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets"))

from gtp_assets.geometry import decimate, export_geometry_glb, load_glb  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--tris", type=int, default=8000)
    a = ap.parse_args()
    asset = load_glb(os.path.join(a.folder, "_raw.glb"))
    print(f"[parked] raw: {asset.tri_count} tris in {len(asset.parts)} part(s)", flush=True)
    out = decimate(asset, a.tris)
    export_geometry_glb(out, os.path.join(a.folder, "_dec.glb"), ["M_Parked"] * len(out.parts))
    print(f"[parked] decimated to {out.tri_count} tris", flush=True)
    if out.tri_count > a.tris * 1.1:
        sys.exit(f"over budget: {out.tri_count} > {a.tris}")


main()
