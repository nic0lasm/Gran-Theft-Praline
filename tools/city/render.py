#!/usr/bin/env python3
"""Software preview of a generated zone (no engine needed): reads greybox.glb back,
adds the LiDAR ground and the Grand Lyon street trees, and renders PNG views.

It is a check tool: it proves the GLB is readable, that buildings sit on the
ground and that roofs, storeys and shops line up, before any editor time.

Usage:
  python3 tools/city/render.py --name presqu-ile --focus "Rue de la République" \
      --view street:-60,-170,1.7,-40,150,12 --view aerial:-350,-420,260,40,40,0
  (a view is label:eye_x,eye_y,eye_z,target_x,target_y,target_z in local metres, z above ground origin)
"""

import argparse
import json
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grandlyon  # noqa: E402
import raster  # noqa: E402
from generate import REPO, slug  # noqa: E402

SUN = np.array([-0.45, -0.55, 0.70])
SUN /= np.linalg.norm(SUN)
SKY_TOP, SKY_LOW = np.array([0.55, 0.68, 0.85]), np.array([0.86, 0.88, 0.90])


def read_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    jlen = struct.unpack_from("<I", data, 12)[0]
    gltf = json.loads(data[20:20 + jlen])
    binary = data[20 + jlen + 8:]
    dt = {5126: np.float32, 5125: np.uint32, 5121: np.uint8}
    ncomp = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}

    def acc(i):
        a = gltf["accessors"][i]
        v = gltf["bufferViews"][a["bufferView"]]
        arr = np.frombuffer(binary, dt[a["componentType"]], a["count"] * ncomp[a["type"]], v["byteOffset"])
        return arr.reshape(a["count"], ncomp[a["type"]]) if ncomp[a["type"]] > 1 else arr

    V, C, I = [], [], []
    base = 0
    for node in gltf["nodes"]:
        prim = gltf["meshes"][node["mesh"]]["primitives"][0]
        p = acc(prim["attributes"]["POSITION"]).astype(float) + np.array(node.get("translation", [0, 0, 0]))
        c = acc(prim["attributes"]["COLOR_0"])[:, :3] / 255.0
        idx = acc(prim["indices"]).astype(np.int64)
        V.append(np.column_stack([p[:, 0], -p[:, 2], p[:, 1]]))  # glTF -> local (east, north, up)
        C.append(c)
        I.append(idx.reshape(-1, 3) + base)
        base += len(p)
    return np.vstack(V), np.vstack(C), np.vstack(I), gltf


def ground_mesh(mnt, origin, origin_alt, bounds, step=2.0):
    x0, y0, x1, y1 = bounds
    xs, ys = np.arange(x0, x1 + step, step), np.arange(y0, y1 + step, step)
    gx, gy = np.meshgrid(xs, ys)
    z = mnt.sample(gx.ravel() + origin[0], gy.ravel() + origin[1]) - origin_alt
    z = np.where(np.isnan(z), np.nanmedian(z), z) - 0.02
    V = np.column_stack([gx.ravel(), gy.ravel(), z])
    nx, ny = len(xs), len(ys)
    k = np.arange(nx * ny).reshape(ny, nx)[:-1, :-1].ravel()
    I = np.vstack([np.column_stack([k, k + 1, k + nx + 1]), np.column_stack([k, k + nx + 1, k + nx])])
    C = np.tile([0.47, 0.46, 0.44], (len(V), 1))
    return V, C, I


def tree_mesh(trees, origin, origin_alt, mnt, bounds):
    V, C, I = [], [], []
    x0, y0, x1, y1 = bounds
    for ft in trees:
        x, y = ft["geometry"]["coordinates"][:2]
        lx, ly = x - origin[0], y - origin[1]
        if not (x0 <= lx <= x1 and y0 <= ly <= y1):
            continue
        p = ft["properties"]
        h = p.get("hauteurtotale_m") or 10
        r = (p.get("rayoncouronne_m") or 3)
        trunk = p.get("hauteurfut_m") or h * 0.35
        gz = mnt.sample(np.array([x]), np.array([y]))[0] - origin_alt
        c = np.array([lx, ly, gz + trunk + (h - trunk) / 2])
        hz = (h - trunk) / 2
        pts = [c + d for d in ([r, 0, 0], [-r, 0, 0], [0, r, 0], [0, -r, 0], [0, 0, hz], [0, 0, -hz])]
        b = sum(len(v) for v in V)
        V.append(np.array(pts + [[lx - 0.25, ly, gz], [lx + 0.25, ly, gz], [lx, ly, gz + trunk + 0.5]]))
        C.append(np.array([[0.30, 0.42, 0.20]] * 6 + [[0.30, 0.24, 0.18]] * 3))
        I.append(np.array([[0, 2, 4], [2, 1, 4], [1, 3, 4], [3, 0, 4], [2, 0, 5], [1, 2, 5], [3, 1, 5], [0, 3, 5],
                           [6, 7, 8]]) + b)
    if not V:
        return np.zeros((0, 3)), np.zeros((0, 3)), np.zeros((0, 3), int)
    return np.vstack(V), np.vstack(C), np.vstack(I)


def render(V, C, I, eye, target, w=1600, h=900, fov_deg=60.0, fog_m=900.0):
    eye, target = np.asarray(eye, float), np.asarray(target, float)
    f = target - eye
    f /= np.linalg.norm(f)
    r = np.cross(f, [0, 0, 1.0])
    r /= np.linalg.norm(r)
    u = np.cross(r, f)
    rel = V - eye
    cam = np.column_stack([rel @ r, rel @ u, rel @ f])
    fpx = (w / 2) / np.tan(np.radians(fov_deg) / 2)
    tri = cam[I]
    keep = (tri[:, :, 2] > 0.3).all(1)
    I, tri = I[keep], tri[keep]
    sx = w / 2 + fpx * tri[:, :, 0] / tri[:, :, 2]
    sy = h / 2 - fpx * tri[:, :, 1] / tri[:, :, 2]
    world = V[I]
    nrm = np.cross(world[:, 1] - world[:, 0], world[:, 2] - world[:, 0])
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12
    toward = (eye - world.mean(1))
    nrm *= np.sign((nrm * toward).sum(1))[:, None]
    shade = 0.45 + 0.55 * np.clip(nrm @ SUN, 0, 1) + 0.08 * nrm[:, 2]
    col = C[I].mean(1) * shade[:, None]
    dist = np.linalg.norm(toward, axis=1)
    fog = np.clip(dist / fog_m, 0, 1)[:, None] ** 1.5
    col = col * (1 - fog) + SKY_LOW * fog

    img = np.zeros((h, w, 3))
    t = np.linspace(0, 1, h)[:, None]
    img[:] = (SKY_TOP * (1 - t) + SKY_LOW * t)[:, None, :]
    zbuf = np.full((h, w), np.inf)
    on = (sx.max(1) >= 0) & (sx.min(1) < w) & (sy.max(1) >= 0) & (sy.min(1) < h)
    for k in np.nonzero(on)[0]:
        x, y, z = sx[k], sy[k], tri[k, :, 2]
        xa, xb = max(int(np.floor(x.min())), 0), min(int(np.ceil(x.max())), w - 1)
        ya, yb = max(int(np.floor(y.min())), 0), min(int(np.ceil(y.max())), h - 1)
        if xb < xa or yb < ya:
            continue
        px, py = np.meshgrid(np.arange(xa, xb + 1) + 0.5, np.arange(ya, yb + 1) + 0.5)
        d = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
        if abs(d) < 1e-9:
            continue
        l0 = ((y[1] - y[2]) * (px - x[2]) + (x[2] - x[1]) * (py - y[2])) / d
        l1 = ((y[2] - y[0]) * (px - x[2]) + (x[0] - x[2]) * (py - y[2])) / d
        l2 = 1 - l0 - l1
        m = (l0 >= -1e-6) & (l1 >= -1e-6) & (l2 >= -1e-6)
        if not m.any():
            continue
        zz = 1.0 / (l0 / z[0] + l1 / z[1] + l2 / z[2])
        sub = zbuf[ya:yb + 1, xa:xb + 1]
        m &= zz < sub
        sub[m] = zz[m]
        img[ya:yb + 1, xa:xb + 1][m] = col[k]
    return np.clip(img * 255, 0, 255).astype(np.uint8)


def main():
    from PIL import Image

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--focus", default="Rue de la République")
    ap.add_argument("--view", action="append", required=True)
    ap.add_argument("--width", type=int, default=1600)
    ap.add_argument("--height", type=int, default=900)
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    args = ap.parse_args()
    raw = os.path.join(args.geo, "raw", args.name)
    proc = os.path.join(args.geo, "processed", args.name)
    out = os.path.join(proc, "gen", slug(args.focus))
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    origin, origin_alt = man["origin_lambert93_m"], man["origin_altitude_m"]
    V, C, I, _ = read_glb(os.path.join(out, "greybox.glb"))
    b = (V[:, 0].min() - 150, V[:, 1].min() - 150, V[:, 0].max() + 150, V[:, 1].max() + 150)
    mnt = raster.Raster.load(raw, "mnt")
    parts = [(V, C, I), ground_mesh(mnt, origin, origin_alt, b),
             tree_mesh(grandlyon.load(raw, "trees"), origin, origin_alt, mnt, b)]
    if os.path.exists(os.path.join(out, "riverside.glb")):   # tools/city/riverside.py
        rv, rc, ri, _ = read_glb(os.path.join(out, "riverside.glb"))
        parts.append((rv, rc, ri))
    Vs, Cs, Is, base = [], [], [], 0
    for v, c, i in parts:
        Vs.append(v)
        Cs.append(c)
        Is.append(i + base)
        base += len(v)
    V, C, I = np.vstack(Vs), np.vstack(Cs), np.vstack(Is).astype(np.int64)
    for spec in args.view:
        label, nums = spec.split(":")
        n = [float(v) for v in nums.split(",")]
        img = render(V, C, I, n[:3], n[3:], args.width, args.height, fog_m=max(900.0, 6 * n[2]))
        path = os.path.join(out, f"preview_{label}.png")
        Image.fromarray(img).save(path)
        print(path)


if __name__ == "__main__":
    main()
