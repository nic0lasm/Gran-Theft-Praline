"""Greybox geometry for the generated city and a minimal binary glTF (GLB) writer.

Meshes are built in local metres (x east, y north, z up) with per-vertex
colours (one material, vertex colour). The GLB is written in glTF axes:
X = east, Y = up, Z = south (right-handed, metres).
"""

import json
import struct

import numpy as np

GLASS = (0.16, 0.20, 0.24)
FRAME = (0.30, 0.28, 0.26)
ROOF_COLOURS = {"tile": (0.62, 0.33, 0.24), "zinc": (0.43, 0.46, 0.49), "flat": (0.50, 0.49, 0.47)}
PARTY = (0.66, 0.63, 0.58)
SIGN_DEFAULT = (0.10, 0.12, 0.14)
REVEAL = 0.22  # window recess depth (m)


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


class Mesh:
    def __init__(self):
        self.v, self.n, self.c, self.i = [], [], [], []

    def tri(self, a, b, c, col):
        a, b, c = (np.asarray(p, float) for p in (a, b, c))
        nrm = np.cross(b - a, c - a)
        L = np.linalg.norm(nrm)
        if L < 1e-10:
            return
        nrm /= L
        base = len(self.v)
        self.v += [a, b, c]
        self.n += [nrm] * 3
        self.c += [col] * 3
        self.i += [base, base + 1, base + 2]

    def quad(self, a, b, c, d, col):
        """Counter-clockwise seen from the front."""
        self.tri(a, b, c, col)
        self.tri(a, c, d, col)

    def box(self, centre, u, w, h, depth, n, col):
        """Axis box on a wall frame: u = along wall, n = out of wall, z up; centre = bottom-back-middle."""
        up = np.array([0, 0, 1.0])
        o = np.asarray(centre, float) - u * (w / 2)
        p = [o + u * a + n * b + up * c for a in (0, w) for b in (0, depth) for c in (0, h)]
        # indices: (a,b,c) bits -> 4a + 2b + c
        faces = [(2, 6, 7, 3), (0, 1, 5, 4), (4, 5, 7, 6), (0, 2, 3, 1), (1, 3, 7, 5), (0, 4, 6, 2)]
        for f in faces:
            self.quad(*(p[k] for k in f), col)

    def extend(self, other):
        base = len(self.v)
        self.v += other.v
        self.n += other.n
        self.c += other.c
        self.i += [k + base for k in other.i]

    @property
    def tris(self):
        return len(self.i) // 3


# --- Facade modules ---------------------------------------------------------------
def wall_with_opening(m, a, u, n, w, h, ow, oh, sill, wall_col, inner_col):
    """Wall panel (origin a = bottom-left, u along wall, n outward) with a recessed opening."""
    up = np.array([0, 0, 1.0])
    x0, x1 = (w - ow) / 2, (w + ow) / 2
    z0, z1 = sill, min(h, sill + oh)
    P = lambda x, z, d=0.0: a + u * x + up * z - n * d  # noqa: E731
    m.quad(P(0, 0), P(w, 0), P(w, z0), P(0, z0), wall_col)
    m.quad(P(0, z1), P(w, z1), P(w, h), P(0, h), wall_col)
    m.quad(P(0, z0), P(x0, z0), P(x0, z1), P(0, z1), wall_col)
    m.quad(P(x1, z0), P(w, z0), P(w, z1), P(x1, z1), wall_col)
    d = REVEAL
    m.quad(P(x0, z0), P(x0, z0, d), P(x0, z1, d), P(x0, z1), wall_col)
    m.quad(P(x1, z1), P(x1, z1, d), P(x1, z0, d), P(x1, z0), wall_col)
    m.quad(P(x0, z1), P(x0, z1, d), P(x1, z1, d), P(x1, z1), wall_col)
    m.quad(P(x1, z0), P(x1, z0, d), P(x0, z0, d), P(x0, z0), wall_col)
    m.quad(P(x0, z0, d), P(x1, z0, d), P(x1, z1, d), P(x0, z1, d), inner_col)


def plain_wall(m, a, u, w, h, col):
    up = np.array([0, 0, 1.0])
    m.quad(a, a + u * w, a + u * w + up * h, a + up * h, col)


def sign_colour(name):
    import hashlib
    if not name:
        return SIGN_DEFAULT
    hsh = hashlib.sha1(name.encode("utf-8")).digest()
    return (0.25 + hsh[0] / 400.0, 0.12 + hsh[1] / 600.0, 0.10 + hsh[2] / 500.0)


def add_module(mesh, mod):
    n = np.array([mod["nx"], mod["ny"], 0.0])
    u = np.array([-mod["ny"], mod["nx"], 0.0])  # along the wall, left to right seen from outside
    w, h = mod["width_m"], mod["height_m"]
    a = np.array([mod["x"], mod["y"], mod["z"]]) - u * (w / 2)
    wall = hex_rgb(mod["colour"])
    k = mod["kind"]
    if k == "blank" or w < 0.8:
        plain_wall(mesh, a, u, w, h, wall)
    elif k in ("window", "window_balcony", "window_small", "court_window", "gf_window"):
        ow = min(1.35, max(0.9, 0.42 * w))
        oh = min(h - 1.1, 0.62 * h if k != "window_small" else 0.45 * h)
        sill = 0.9 if k != "gf_window" else 1.1
        wall_with_opening(mesh, a, u, n, w, h, ow, oh, sill, wall, GLASS)
        if k == "window_balcony":
            mesh.box(a + u * (w / 2), u, w, 0.16, 0.65, n, wall)
            mesh.box(a + u * (w / 2) + n * 0.6 + np.array([0, 0, 0.16]), u, w, 0.95, 0.04, n, FRAME)
    elif k == "entresol_window":
        wall_with_opening(mesh, a, u, n, w, h, min(1.3, 0.42 * w), min(h - 0.9, 1.3), 0.6, wall, GLASS)
    elif k in ("shop", "shopfront_closed"):
        band = 0.8
        wall_with_opening(mesh, a, u, n, w, h - band, w - 0.5, h - band - 0.3, 0.3, wall, GLASS)
        plain_wall(mesh, a + np.array([0, 0, h - band]), u, w, band, wall)
        if k == "shop" and not mod.get("sign_continued"):
            mesh.box(a + u * (w / 2) + np.array([0, 0, h - band + 0.1]), u, min(w - 0.3, 2.8), 0.55, 0.12, n,
                     sign_colour(mod.get("sign_brand") or mod.get("sign_name")))
    elif k == "entrance":
        wall_with_opening(mesh, a, u, n, w, h, min(w - 0.6, 1.6), min(h - 0.4, 3.2), 0.0, wall, FRAME)
    elif k == "monument_bay":
        wall_with_opening(mesh, a, u, n, w, h, 0.35 * w, 0.7 * h, 0.15 * h, wall, GLASS)
    else:
        plain_wall(mesh, a, u, w, h, wall)


def building_mesh(poly, ground_z, roof, modules, info, roof_tris):
    """Walls (party/plain where no module), modules, roof. Returns Mesh."""
    m = Mesh()
    up = np.array([0, 0, 1.0])
    eave = roof["eave_z"]
    base = ground_z - 0.5  # bury a little so slopes never show a gap
    covered = {(mod["ring"], mod["edge"]) for mod in modules}
    for ri, ei, p0, p1, n, kind, street in info["edge_list"]:
        a = np.array([p0[0], p0[1], base])
        b = np.array([p1[0], p1[1], base])
        col = PARTY if kind == "party" else hex_rgb(info["colour"])
        if np.dot(np.cross(b - a, up)[:2], n) < 0:
            a, b = b, a
        if (ri, ei) in covered:
            # below-ground skirt + gap between the top storey and the eave are already filled by modules
            m.quad(a, b, b + up * (ground_z - base), a + up * (ground_z - base), col)
            continue
        m.quad(a, b, b + up * (eave - base), a + up * (eave - base), col)
    for mod in modules:
        add_module(m, mod)
    verts, tris = roof_tris
    rc = ROOF_COLOURS["flat" if roof["kind"] == "flat" else
                      ("zinc" if info["typology"] == "haussmann" else "tile")]
    for t in tris:
        a, b, c = verts[t[0]], verts[t[1]], verts[t[2]]
        if np.cross(b - a, c - a)[2] < 0:
            b, c = c, b
        m.tri(a, b, c, rc)
    return m


# --- GLB --------------------------------------------------------------------------
def write_glb(path, named_meshes, extras=None):
    """named_meshes: list of (name, Mesh, (ox, oy, oz) node translation in local metres)."""
    bin_parts, views, accessors, meshes, nodes = [], [], [], [], []
    offset = 0

    def add_view(arr, target):
        nonlocal offset
        data = arr.tobytes()
        pad = (-len(data)) % 4
        bin_parts.append(data + b"\0" * pad)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target})
        offset += len(data) + pad
        return len(views) - 1

    for name, mesh, origin in named_meshes:
        if mesh.tris == 0:
            continue
        v = np.asarray(mesh.v, np.float32) - np.asarray(origin, np.float32)
        nrm = np.asarray(mesh.n, np.float32)
        to_gltf = lambda a: np.column_stack([a[:, 0], a[:, 2], -a[:, 1]]).astype(np.float32)  # noqa: E731
        v, nrm = to_gltf(v), to_gltf(nrm)
        col = np.clip(np.asarray(mesh.c, np.float32) * 255 + 0.5, 0, 255).astype(np.uint8)
        col = np.column_stack([col, np.full(len(col), 255, np.uint8)])
        idx = np.asarray(mesh.i, np.uint32)
        acc = {}
        for key, arr, comp, typ, extra in (
                ("POSITION", v, 5126, "VEC3", {"min": v.min(0).tolist(), "max": v.max(0).tolist()}),
                ("NORMAL", nrm, 5126, "VEC3", {}),
                ("COLOR_0", col, 5121, "VEC4", {"normalized": True})):
            accessors.append({"bufferView": add_view(np.ascontiguousarray(arr), 34962), "componentType": comp,
                              "count": len(arr), "type": typ, **extra})
            acc[key] = len(accessors) - 1
        accessors.append({"bufferView": add_view(idx, 34963), "componentType": 5125, "count": len(idx),
                          "type": "SCALAR"})
        meshes.append({"name": name, "primitives": [{"attributes": acc, "indices": len(accessors) - 1,
                                                      "material": 0}]})
        o = origin
        nodes.append({"name": name, "mesh": len(meshes) - 1, "translation": [float(o[0]), float(o[2]), float(-o[1])]})

    gltf = {
        "asset": {"version": "2.0", "generator": "gran-theft-praline city pipeline"},
        "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}], "nodes": nodes, "meshes": meshes,
        "materials": [{"name": "M_CityGreybox", "doubleSided": True, "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1],
                                                                       "metallicFactor": 0, "roughnessFactor": 0.85}}],
        "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": offset}],
    }
    if extras:
        gltf["asset"]["extras"] = extras
    js = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    js += b" " * ((-len(js)) % 4)
    binary = b"".join(bin_parts)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(binary)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(binary), 0x004E4942) + binary)
