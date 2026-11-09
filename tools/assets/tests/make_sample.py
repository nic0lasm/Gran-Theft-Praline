"""Procedural stand-in for a Tripo master: a dense textured bench-like object.

Deliberately wrong in scale, orientation and pivot, like a raw export.
"""
import numpy as np
import trimesh
from PIL import Image
from trimesh.visual import TextureVisuals
from trimesh.visual.material import PBRMaterial


def _planar_uv(vertices):
    lo, hi = vertices.min(axis=0), vertices.max(axis=0)
    span = np.maximum(hi - lo, 1e-9)
    return ((vertices[:, [0, 2]] - lo[[0, 2]]) / span[[0, 2]]).astype(np.float32)


def make_sample_glb(path, dense=True, seed=0):
    rng = np.random.default_rng(seed)
    parts = []
    seat = trimesh.creation.box(extents=[180, 8, 50])
    seat.apply_translation([0, 45, 0])
    parts.append(seat)
    for x in (-70, 70):
        leg = trimesh.creation.cylinder(radius=4, height=45, sections=24)
        leg.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [1, 0, 0]))
        leg.apply_translation([x, 22, 0])
        parts.append(leg)
    mesh = trimesh.util.concatenate(parts)
    if dense:
        for _ in range(3):
            mesh = mesh.subdivide()
        mesh.vertices += rng.normal(0, 0.02, mesh.vertices.shape)
    # raw export quirks: arbitrary scale, offset, sideways orientation
    mesh.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, [0, 1, 0]))
    mesh.apply_translation([37.0, 12.0, -80.0])
    uv = _planar_uv(np.asarray(mesh.vertices))
    tex = Image.fromarray((rng.random((256, 256, 3)) * 255).astype(np.uint8), "RGB")
    mr = Image.fromarray(np.dstack([np.zeros((256, 256)), np.full((256, 256), 180),
                                    np.full((256, 256), 60)]).astype(np.uint8), "RGB")
    nrm = Image.fromarray(np.dstack([np.full((256, 256), 128), np.full((256, 256), 128),
                                     np.full((256, 256), 255)]).astype(np.uint8), "RGB")
    mat = PBRMaterial(name="raw", baseColorTexture=tex, metallicRoughnessTexture=mr,
                      normalTexture=nrm)
    mesh.visual = TextureVisuals(uv=uv, material=mat)
    trimesh.Scene(mesh).export(path)
    return path
