"""Broken variants of street furniture for the physics subsystem (0.4).

Works on a prepared asset (metres, glTF frame: Y up, pivot at the bottom centre). Two kinds:

  snap  a post or a lamp torn off at the base: ``Stump`` (what stays in the ground, with a
        ragged cap) and ``Top`` (the part that falls, same pivot height as the original)
  bend  a sign pole or a bike hoop hit by a car: ``Bent``, the part above the bend height
        turned about the horizontal X axis, with a smooth knee

The variants keep the UVs and material slots of the original, so Unreal reuses its material
instances. See docs/gameplay/breakable-street-furniture.md.
"""

from __future__ import annotations

import numpy as np

from .geometry import Asset, Part

# Catalogue id -> (kind, height of the break or knee in metres, bend angle in degrees)
BROKEN = {
    "W1-04": ("snap", 0.35, 0.0),     # heritage street lamp
    "W8-01": ("snap", 0.30, 0.0),     # vehicle traffic light
    "W8-02": ("snap", 0.30, 0.0),     # pedestrian light
    "W1-13": ("snap", 0.20, 0.0),     # bike-share terminal
    "W1-27": ("snap", 0.12, 0.0),     # fire hydrant (the jet comes from the stump)
    "W1-12": ("bend", 0.10, 32.0),    # bike hoop bends, does not come off
    "W1-57": ("bend", 0.40, 48.0),    # pedestrian-zone sign
}
BROKEN.update({"W8-%02d" % k: ("bend", 0.40, 48.0) for k in range(5, 18)})   # signs on poles
KNEE_M = 0.25            # length over which a bend reaches its full angle
CAP_JITTER_M = 0.03      # ragged top of a stump


def _split(part: Part, keep: np.ndarray) -> Part:
    faces = part.faces[keep]
    used, inverse = np.unique(faces, return_inverse=True)
    return Part(vertices=part.vertices[used].copy(), faces=inverse.reshape(-1, 3).astype(np.uint32),
                uv=part.uv[used].copy(), normals=part.normals[used].copy(), material=part.material,
                name=part.name)


def _cap(stump: Part, y: float, seed: int) -> Part:
    """Close the cut with a ragged fan over the convex hull of the cut ring."""
    ring = stump.vertices[stump.vertices[:, 1] > y - 0.05]
    if len(ring) < 3:
        return stump
    pts = ring[:, [0, 2]].astype(np.float64)
    hull = _hull(pts)
    if len(hull) < 3:
        return stump
    rng = np.random.default_rng(seed)
    centre = pts[hull].mean(axis=0)
    rim = np.array([[p[0], y + rng.uniform(-CAP_JITTER_M, CAP_JITTER_M), p[1]] for p in pts[hull]], np.float32)
    top = np.array([[centre[0], y + CAP_JITTER_M, centre[1]]], np.float32)
    v = np.vstack([rim, top])
    n0 = len(stump.vertices)
    c = n0 + len(rim)
    faces = np.array([[n0 + i, c, n0 + (i + 1) % len(rim)] for i in range(len(rim))], np.uint32)
    return Part(vertices=np.vstack([stump.vertices, v]).astype(np.float32),
                faces=np.vstack([stump.faces, faces]).astype(np.uint32),
                uv=np.vstack([stump.uv, np.zeros((len(v), 2), np.float32)]).astype(np.float32),
                normals=np.vstack([stump.normals, np.tile([0.0, 1.0, 0.0], (len(v), 1))]).astype(np.float32),
                material=stump.material, name=stump.name)


def _hull(pts: np.ndarray) -> list[int]:
    """Indices of the 2D convex hull, counter-clockwise (monotone chain)."""
    order = sorted(range(len(pts)), key=lambda i: (pts[i][0], pts[i][1]))

    def cross(o, a, b):
        return (pts[a][0] - pts[o][0]) * (pts[b][1] - pts[o][1]) - (pts[a][1] - pts[o][1]) * (pts[b][0] - pts[o][0])

    lower, upper = [], []
    for i in order:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], i) <= 0:
            lower.pop()
        lower.append(i)
    for i in reversed(order):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], i) <= 0:
            upper.pop()
        upper.append(i)
    return lower[:-1] + upper[:-1]


def snap(asset: Asset, height_m: float, seed: int = 1) -> dict[str, tuple[Asset, list[int]]]:
    """Stump (triangles below the cut, capped) and Top (the rest), each with the original
    material slot index of every part it keeps."""
    stump, top, s_slots, t_slots = Asset(), Asset(), [], []
    for i, part in enumerate(asset.parts):
        cy = part.vertices[part.faces][:, :, 1].mean(axis=1)
        low = cy < height_m
        if low.any():
            stump.parts.append(_cap(_split(part, low), height_m, seed))
            s_slots.append(i)
        if (~low).any():
            top.parts.append(_split(part, ~low))
            t_slots.append(i)
    if not stump.parts or not top.parts:
        raise ValueError("cut at %.2f m leaves an empty piece" % height_m)
    return {"Stump": (stump, s_slots), "Top": (top, t_slots)}


def bend(asset: Asset, height_m: float, angle_deg: float) -> dict[str, tuple[Asset, list[int]]]:
    """Bent: everything above ``height_m`` turned about X through (0, height_m, 0)."""
    out = Asset()
    for part in asset.parts:
        v = part.vertices.astype(np.float64).copy()
        n = part.normals.astype(np.float64).copy()
        t = np.clip((v[:, 1] - height_m) / KNEE_M, 0.0, 1.0)
        a = np.radians(angle_deg) * t
        ca, sa = np.cos(a), np.sin(a)
        dy, z = v[:, 1] - height_m, v[:, 2].copy()
        v[:, 1] = np.where(t > 0, height_m + dy * ca - z * sa, v[:, 1])
        v[:, 2] = np.where(t > 0, dy * sa + z * ca, v[:, 2])
        ny, nz = n[:, 1].copy(), n[:, 2].copy()
        n[:, 1], n[:, 2] = ny * ca - nz * sa, ny * sa + nz * ca
        out.parts.append(Part(vertices=v.astype(np.float32), faces=part.faces.copy(), uv=part.uv.copy(),
                              normals=n.astype(np.float32), material=part.material, name=part.name))
    return {"Bent": (out, list(range(len(asset.parts))))}


def variants(asset_id: str, asset: Asset) -> dict[str, tuple[Asset, list[int]]]:
    kind, h, angle = BROKEN[asset_id]
    return snap(asset, h) if kind == "snap" else bend(asset, h, angle)
