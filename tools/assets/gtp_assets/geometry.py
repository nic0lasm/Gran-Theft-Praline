"""Loading, normalising and decimating Tripo GLB meshes.

Conventions kept on purpose (Unreal's glTF importer converts them):
glTF is Y-up, +Z front, metres. Unreal receives Z-up, centimetres.
"""
from __future__ import annotations

import ctypes
from dataclasses import dataclass, field
from typing import Optional

import numpy as np
import trimesh

from meshoptimizer._loader import lib as _meshopt

# The pip wrapper declares no ctypes signature for this entry point (pointers
# get truncated, hence crashes), so declare it here, matching meshoptimizer.h.
_P = ctypes.POINTER
_meshopt.meshopt_simplifyWithAttributes.argtypes = [
    _P(ctypes.c_uint), _P(ctypes.c_uint), ctypes.c_size_t,
    _P(ctypes.c_float), ctypes.c_size_t, ctypes.c_size_t,
    _P(ctypes.c_float), ctypes.c_size_t, _P(ctypes.c_float), ctypes.c_size_t,
    _P(ctypes.c_ubyte), ctypes.c_size_t, ctypes.c_float, ctypes.c_uint,
    _P(ctypes.c_float),
]
_meshopt.meshopt_simplifyWithAttributes.restype = ctypes.c_size_t
_meshopt.meshopt_simplifySloppy.argtypes = [
    _P(ctypes.c_uint), _P(ctypes.c_uint), ctypes.c_size_t,
    _P(ctypes.c_float), ctypes.c_size_t, ctypes.c_size_t,
    ctypes.c_size_t, ctypes.c_float, _P(ctypes.c_float),
]
_meshopt.meshopt_simplifySloppy.restype = ctypes.c_size_t


@dataclass
class Part:
    """One geometry of the asset (one material slot)."""

    vertices: np.ndarray  # (n, 3) float32, metres
    faces: np.ndarray  # (m, 3) uint32
    uv: np.ndarray  # (n, 2) float32
    normals: np.ndarray  # (n, 3) float32
    material: Optional[object] = None  # trimesh PBRMaterial or None
    name: str = "part"


@dataclass
class Asset:
    parts: list[Part] = field(default_factory=list)
    used_sloppy: bool = False

    @property
    def tri_count(self) -> int:
        return int(sum(len(p.faces) for p in self.parts))

    def bounds(self) -> np.ndarray:
        lo = np.min([p.vertices.min(axis=0) for p in self.parts], axis=0)
        hi = np.max([p.vertices.max(axis=0) for p in self.parts], axis=0)
        return np.array([lo, hi], dtype=np.float64)


def load_glb(path: str) -> Asset:
    """Load a GLB with node transforms baked into the vertices."""
    scene = trimesh.load(path, force="scene", process=False)
    meshes = scene.dump(concatenate=False)
    asset = Asset()
    for i, m in enumerate(meshes):
        if not isinstance(m, trimesh.Trimesh) or len(m.faces) == 0:
            continue
        vis = m.visual
        uv = getattr(vis, "uv", None)
        if uv is None or len(uv) != len(m.vertices):
            uv = np.zeros((len(m.vertices), 2), dtype=np.float32)
        material = getattr(vis, "material", None)
        asset.parts.append(
            Part(
                vertices=np.asarray(m.vertices, dtype=np.float32),
                faces=np.asarray(m.faces, dtype=np.uint32),
                uv=np.asarray(uv, dtype=np.float32),
                normals=np.asarray(m.vertex_normals, dtype=np.float32),
                material=material,
                name=str(getattr(m, "metadata", {}).get("name", f"part{i}")),
            )
        )
    if not asset.parts:
        raise ValueError(f"no triangle geometry in {path}")
    return asset


def _rot_y(deg: float) -> np.ndarray:
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]], dtype=np.float32)


PIVOTS = ("bottom_center", "center", "back_center")


def normalize(asset: Asset, size_m: float, pivot: str = "bottom_center",
              rotate_y_deg: float = 0.0) -> float:
    """Rotate about Y, scale so the longest extent equals ``size_m``, move the pivot.

    Returns the uniform scale factor applied. Works in place.
    """
    if pivot not in PIVOTS:
        raise ValueError(f"pivot must be one of {PIVOTS}")
    if size_m <= 0:
        raise ValueError("size_m must be positive")
    if rotate_y_deg:
        r = _rot_y(rotate_y_deg)
        for p in asset.parts:
            p.vertices = p.vertices @ r.T
            p.normals = p.normals @ r.T
    lo, hi = asset.bounds()
    longest = float(np.max(hi - lo))
    if longest <= 0:
        raise ValueError("degenerate mesh (zero extent)")
    scale = size_m / longest
    lo, hi = lo * scale, hi * scale
    cx, cz = (lo[0] + hi[0]) / 2, (lo[2] + hi[2]) / 2
    if pivot == "bottom_center":
        offset = np.array([cx, lo[1], cz])
    elif pivot == "center":
        offset = np.array([cx, (lo[1] + hi[1]) / 2, cz])
    else:  # back_center: centred in X and Y, flush with the back (-Z) face
        offset = np.array([cx, (lo[1] + hi[1]) / 2, lo[2]])
    for p in asset.parts:
        p.vertices = (p.vertices * scale - offset).astype(np.float32)
    return scale


def _compact(vertices, uv, normals, indices):
    used, inverse = np.unique(indices, return_inverse=True)
    return (vertices[used], uv[used], normals[used],
            inverse.reshape(-1, 3).astype(np.uint32))


def decimate_part(part: Part, target_tris: int, uv_weight: float = 4.0,
                  normal_weight: float = 1.0, target_error: float = 0.2) -> Part:
    """Attribute-aware quadric decimation (UV and normals steer the collapses)."""
    if len(part.faces) <= target_tris:
        return part
    verts = np.ascontiguousarray(part.vertices, dtype=np.float32)
    attrs = np.ascontiguousarray(np.hstack([part.uv, part.normals]), dtype=np.float32)
    weights = np.array([uv_weight] * 2 + [normal_weight] * 3, dtype=np.float32)
    idx = np.ascontiguousarray(part.faces.reshape(-1), dtype=np.uint32)
    dest = np.zeros_like(idx)
    result_error = ctypes.c_float(0.0)
    n = _meshopt.meshopt_simplifyWithAttributes(
        dest.ctypes.data_as(_P(ctypes.c_uint)), idx.ctypes.data_as(_P(ctypes.c_uint)),
        len(idx), verts.ctypes.data_as(_P(ctypes.c_float)), len(verts), 12,
        attrs.ctypes.data_as(_P(ctypes.c_float)), 20,
        weights.ctypes.data_as(_P(ctypes.c_float)), 5,
        None, int(target_tris) * 3, target_error, 0, ctypes.byref(result_error),
    )
    new_idx = dest[:n].reshape(-1, 3)
    v, u, nm, f = _compact(part.vertices, part.uv, part.normals, new_idx)
    return Part(v, f, u, nm, part.material, part.name)


def decimate_part_sloppy(part: Part, target_tris: int) -> Part:
    """Topology-blind fallback for meshes the attribute-aware pass cannot reduce.

    Tripo masters with many open borders and UV islands hit a floor (about 480 k
    triangles on a 2 M master). Sloppy simplification ignores borders; it keeps
    original vertices, so UVs stay valid, at the price of rougher silhouettes.
    """
    if len(part.faces) <= target_tris:
        return part
    verts = np.ascontiguousarray(part.vertices, dtype=np.float32)
    idx = np.ascontiguousarray(part.faces.reshape(-1), dtype=np.uint32)
    dest = np.zeros_like(idx)
    err = ctypes.c_float(0.0)
    n = _meshopt.meshopt_simplifySloppy(
        dest.ctypes.data_as(_P(ctypes.c_uint)), idx.ctypes.data_as(_P(ctypes.c_uint)),
        len(idx), verts.ctypes.data_as(_P(ctypes.c_float)), len(verts), 12,
        int(target_tris) * 3, 1.0, ctypes.byref(err),
    )
    v, u, nm, f = _compact(part.vertices, part.uv, part.normals, dest[:n].reshape(-1, 3))
    return Part(v, f, u, nm, part.material, part.name)


def decimate(asset: Asset, target_tris: int) -> Asset:
    """Decimate to at most ``target_tris`` triangles, split across parts by share.

    The simplifier may stop slightly above the target: retry with a tighter
    target (up to 3 times) so the art bible budgets are real caps.
    """
    total = asset.tri_count
    if total <= target_tris:
        return Asset(list(asset.parts))
    goal = target_tris
    out = asset
    for attempt in range(4):
        # From the second attempt the error limit stops binding (1.0 = whole extent).
        error = 0.2 if attempt == 0 else 1.0
        shares = []
        for p in asset.parts:
            share = max(64, int(round(goal * len(p.faces) / total)))
            shares.append(decimate_part(p, share, target_error=error))
        out = Asset(shares)
        if out.tri_count <= target_tris:
            break
        goal = max(64, int(goal * target_tris / out.tri_count * 0.99))
    if out.tri_count > target_tris:  # floor reached: finish with the topology-blind pass
        out = Asset([decimate_part_sloppy(p, max(64, int(target_tris * len(p.faces) / out.tri_count)))
                     for p in out.parts])
        out.used_sloppy = True
    return out


def export_geometry_glb(asset: Asset, path: str, material_names: list[str]) -> None:
    """Write geometry + UVs only. Textures travel as separate PNG files.

    One scene node per part, named after its material slot, so Unreal creates
    slots in a known order.
    """
    from trimesh.visual import TextureVisuals
    from trimesh.visual.material import PBRMaterial

    scene = trimesh.Scene()
    for p, mname in zip(asset.parts, material_names):
        mesh = trimesh.Trimesh(
            vertices=p.vertices, faces=p.faces, vertex_normals=p.normals,
            process=False,
        )
        mesh.visual = TextureVisuals(uv=p.uv, material=PBRMaterial(name=mname))
        scene.add_geometry(mesh, node_name=mname, geom_name=mname)
    data = scene.export(file_type="glb")
    with open(path, "wb") as f:
        f.write(data)
