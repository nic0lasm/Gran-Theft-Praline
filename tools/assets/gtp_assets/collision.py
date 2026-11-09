"""Collision recipe: computed here, built by Unreal's own convex decomposition."""
from __future__ import annotations

import numpy as np
import trimesh

from .geometry import Asset

FLAT_THICKNESS_M = 0.03  # thinner than this: walked over, no collision


def recipe(asset: Asset, mode: str = "auto") -> dict:
    """Return ``{"type": "none"|"box"|"convex", "hulls": n}``.

    mode: ``auto`` | ``none`` | ``box`` | ``convex:N``.
    """
    if mode == "none":
        return {"type": "none", "hulls": 0}
    if mode == "box":
        return {"type": "box", "hulls": 1}
    if mode.startswith("convex:"):
        return {"type": "convex", "hulls": max(1, int(mode.split(":", 1)[1]))}
    if mode != "auto":
        raise ValueError(f"unknown collision mode {mode!r}")

    lo, hi = asset.bounds()
    ext = hi - lo
    if float(ext.min()) < FLAT_THICKNESS_M:
        return {"type": "none", "hulls": 0, "reason": "flat"}

    pts = np.vstack([p.vertices for p in asset.parts])
    try:
        hull = trimesh.convex.convex_hull(pts)
    except Exception:  # degenerate point cloud
        return {"type": "box", "hulls": 1, "reason": "hull failed"}

    # Surface complexity: a convex object has about as many surface voxels as
    # its hull; frames, benches and racks have many more.
    mesh = trimesh.util.concatenate([
        trimesh.Trimesh(p.vertices, p.faces, process=False) for p in asset.parts])
    pitch = float(ext.max()) / 24
    ratio = len(mesh.voxelized(pitch).points) / max(1, len(hull.voxelized(pitch).points))
    hulls = 1 if ratio <= 1.3 else 4 if ratio <= 2.5 else 8
    return {"type": "convex", "hulls": hulls, "surface_ratio": round(float(ratio), 2)}
