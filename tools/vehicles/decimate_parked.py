#!/usr/bin/env python3
"""Step 2 of make_parked_static.sh: attribute-aware decimation of the raw body (meshoptimizer).

    python3 tools/vehicles/decimate_parked.py <staging>/<id>/parked --tris 8000
Reads _raw.glb, writes _dec.glb (seconds, where Blender's collapse took 18 min and missed the budget).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets"))

import numpy as np  # noqa: E402
import meshoptimizer  # noqa: E402
from gtp_assets.geometry import Asset, Part, _compact, decimate, export_geometry_glb, load_glb  # noqa: E402


def sloppy(part, target):
    """Topology-blind fallback when the attribute-aware pass hits its floor (UV seams): it keeps
    the original vertices, so UVs and normals stay valid; the body texture is coarse paint,
    a little UV smear on a car seen from the pavement is acceptable."""
    v = np.ascontiguousarray(part.vertices, dtype=np.float32)
    f = np.ascontiguousarray(part.faces.reshape(-1), dtype=np.uint32)
    dest = np.zeros_like(f)
    n = meshoptimizer.simplify_sloppy(dest, f, v, target_index_count=target * 3, target_error=1.0)
    vv, uu, nn, ff = _compact(part.vertices, part.uv, part.normals, dest[:n].reshape(-1, 3))
    return Part(vv, ff, uu, nn, part.material, part.name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--tris", type=int, default=8000)
    a = ap.parse_args()
    asset = load_glb(os.path.join(a.folder, "_raw.glb"))
    print(f"[parked] raw: {asset.tri_count} tris in {len(asset.parts)} part(s)", flush=True)
    out = decimate(asset, a.tris)
    if out.tri_count > a.tris * 1.1:
        print(f"[parked] attribute-aware floor at {out.tri_count} tris: topology-blind pass", flush=True)
        total = out.tri_count
        out = Asset([sloppy(p, max(64, int(a.tris * len(p.faces) / total))) for p in out.parts])
    export_geometry_glb(out, os.path.join(a.folder, "_dec.glb"), ["M_Parked"] * len(out.parts))
    print(f"[parked] decimated to {out.tri_count} tris", flush=True)
    if out.tri_count > a.tris * 1.1:
        sys.exit(f"over budget: {out.tri_count} > {a.tris}")


main()
