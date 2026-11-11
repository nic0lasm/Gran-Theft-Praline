"""Far proxies of the streaming cells and the streaming table of the quarter (ADR-0010).

Each 250 m cell of the quarter is its own streaming level (L_City_Cell<X><Y>, the buildings,
facade kit, signs and props of the cell). The persistent level keeps one cheap stand-in per
cell, always loaded: every building shell and landmark volume of the cell merged into one
mesh, vertex colours of the generated shells, one material, no shadow, no collision, no
Lumen cards. The streaming subsystem (GTPCityStreaming) hides a proxy while its detailed
level is visible.

Input:  gen/<cell>/shell_cells.glb (+ landmarks.glb when the cell has some), from cells.py
Output: gen/quarter/cell_proxies.glb   one node per cell, origin at the cell's south-west corner
        gen/quarter/cell_proxies.json  cell_m, budget, and per cell: name, proxy, level,
                                       ue_cm (node origin), bounds_ue_cm [x0, y0, x1, y1],
                                       triangles, buildings

Budget checks (ADR-0010 condition 2): at most 100 proxy meshes (draws) for the whole quarter,
and a triangle ceiling for all proxies together (MAX_TRIANGLES); --check fails above them.

Usage: python3 tools/city/cell_proxies.py --name presqu-ile [--check]
       python3 tools/city/cell_proxies.py --near-street rue-de-la-republique --within 60
         prints the cells whose box is within 60 m of the street axis (the cells a demo route
         along it streams in), from cells.json, without building anything.
"""

import argparse
import glob
import json
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import citylib as cl  # noqa: E402
import meshkit  # noqa: E402

MAX_DRAWS = 100
MAX_TRIANGLES = 400_000   # all proxies together; the 30 cells of the quarter make ~230 k


def camel(slug):
    return "".join(p.capitalize() for p in slug.split("-"))


def read_glb(path):
    """Minimal GLB reader for the files meshkit.write_glb writes: returns a list of
    (node name, Mesh in local metres with the node translation applied)."""
    with open(path, "rb") as f:
        data = f.read()
    magic, _version, _length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67:
        raise ValueError("not a GLB: " + path)
    jlen, _ = struct.unpack_from("<II", data, 12)
    gltf = json.loads(data[20:20 + jlen])
    boff = 20 + jlen
    blen, _ = struct.unpack_from("<II", data, boff)
    binary = data[boff + 8:boff + 8 + blen]
    sizes = {5126: (np.float32, 4), 5121: (np.uint8, 1), 5125: (np.uint32, 4), 5123: (np.uint16, 2)}
    comps = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}

    def accessor(k):
        a = gltf["accessors"][k]
        v = gltf["bufferViews"][a["bufferView"]]
        dt, _ = sizes[a["componentType"]]
        n = a["count"] * comps[a["type"]]
        arr = np.frombuffer(binary, dt, n, v.get("byteOffset", 0) + a.get("byteOffset", 0))
        return arr.reshape(a["count"], comps[a["type"]]) if comps[a["type"]] > 1 else arr

    out = []
    for node in gltf.get("nodes", []):
        if "mesh" not in node:
            continue
        t = np.asarray(node.get("translation", [0, 0, 0]), np.float64)
        m = meshkit.Mesh()
        for prim in gltf["meshes"][node["mesh"]]["primitives"]:
            att = prim["attributes"]
            g = accessor(att["POSITION"]).astype(np.float64) + t
            nr = accessor(att["NORMAL"]).astype(np.float64) if "NORMAL" in att else np.zeros_like(g)
            col = accessor(att["COLOR_0"])[:, :3].astype(np.float64) / 255.0 if "COLOR_0" in att \
                else np.full((len(g), 3), 0.6)
            idx = accessor(prim["indices"]).astype(np.int64)
            # glTF (X east, Y up, Z south) -> local (x east, y north, z up)
            loc = np.column_stack([g[:, 0], -g[:, 2], g[:, 1]])
            nrm = np.column_stack([nr[:, 0], -nr[:, 2], nr[:, 1]])
            part = meshkit.Mesh()
            part.v, part.n, part.c, part.i = list(loc), list(nrm), [tuple(c) for c in col], idx.tolist()
            m.extend(part)
        out.append((node.get("name", "node"), m))
    return out


def corner(cell):
    """cell-p0250-m0500 -> (250.0, -500.0), the south-west corner in local metres."""
    _, xs, ys = cell.split("-")
    val = lambda s: (1 if s[0] == "p" else -1) * float(s[1:])  # noqa: E731
    return val(xs), val(ys)


def build(zone, geo):
    proc = os.path.join(geo, "processed", zone)
    with open(os.path.join(proc, "cells.json"), encoding="utf-8") as f:
        grid = json.load(f)
    cell_m = float(grid["cell_m"])
    items, nodes, missing = [], [], []
    for cell in sorted(c["name"] for c in grid["cells"]):
        gen = os.path.join(proc, "gen", cell)
        merged, buildings = meshkit.Mesh(), 0
        src = os.path.join(gen, "shell_cells.glb")
        if not os.path.exists(src):
            missing.append(cell)
            continue
        for _name, m in read_glb(src):
            merged.extend(m)
        with open(os.path.join(gen, "shell_cells.json"), encoding="utf-8") as f:
            buildings = sum(len(n["buildings"]) for n in json.load(f)["nodes"])
        lm = os.path.join(gen, "landmarks.glb")
        landmarks = 0
        if os.path.exists(lm):
            for _name, m in read_glb(lm):
                merged.extend(m)
                landmarks += 1
        if merged.tris == 0:
            continue
        x0, y0 = corner(cell)
        name = "SM_CellProxy_" + cell[len("cell-"):].replace("-", "_")
        v = np.asarray(merged.v)
        items.append((name, merged, (x0, y0, 0.0)))
        ux0, uy1, _ = cl.to_unreal(v[:, 0].min(), v[:, 1].min())
        ux1, uy0, _ = cl.to_unreal(v[:, 0].max(), v[:, 1].max())
        nodes.append({"name": name, "cell": cell, "level": "/Game/Maps/L_City_" + camel(cell),
                      "ue_cm": list(cl.to_unreal(x0, y0, 0.0)),
                      "cell_bounds_ue_cm": [x0 * 100, -(y0 + cell_m) * 100, (x0 + cell_m) * 100, -y0 * 100],
                      "mesh_bounds_ue_cm": [ux0, uy0, ux1, uy1],
                      "z_range_m": [round(float(v[:, 2].min()), 2), round(float(v[:, 2].max()), 2)],
                      "triangles": merged.tris, "buildings": buildings, "landmarks": landmarks})
    out = os.path.join(proc, "gen", "quarter")
    os.makedirs(out, exist_ok=True)
    meshkit.write_glb(os.path.join(out, "cell_proxies.glb"), items)
    tris = sum(n["triangles"] for n in nodes)
    meta = {"zone": zone, "cell_m": cell_m, "nodes": nodes, "missing_cells": missing,
            "budget": {"draws": len(nodes), "max_draws": MAX_DRAWS, "triangles": tris,
                       "max_triangles": MAX_TRIANGLES,
                       "ok": len(nodes) <= MAX_DRAWS and tris <= MAX_TRIANGLES}}
    with open(os.path.join(out, "cell_proxies.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
    return meta


def cells_near_street(zone, geo, focus, within_m):
    proc = os.path.join(geo, "processed", zone)
    with open(os.path.join(proc, "gen", focus, "report.json"), encoding="utf-8") as f:
        axis = [(p[0] / 100.0, -p[1] / 100.0) for p in json.load(f)["focus_axis_ue_cm"]]   # local metres
    import cells   # the grid alone: cells.json is only there once cells.py has run
    grid = {"cells": cells.grid()}
    pts = []
    for (x0, y0), (x1, y1) in zip(axis, axis[1:]):
        n = int(np.hypot(x1 - x0, y1 - y0) / 5.0) + 1
        pts += [(x0 + (x1 - x0) * t / n, y0 + (y1 - y0) * t / n) for t in range(n)]
    pts.append(axis[-1])
    out = []
    for c in grid["cells"]:
        bx0, by0, bx1, by1 = c["bbox_m"]
        d = min(np.hypot(max(bx0 - x, 0.0, x - bx1), max(by0 - y, 0.0, y - by1)) for x, y in pts)
        if d <= within_m:
            out.append((d, c["name"]))
    return [name for _d, name in sorted(out)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    ap.add_argument("--check", action="store_true", help="exit 1 when the proxy budget is exceeded")
    ap.add_argument("--near-street", metavar="FOCUS", help="only print the cells near this generated street")
    ap.add_argument("--within", type=float, default=60.0, help="metres, with --near-street")
    a = ap.parse_args()
    if a.near_street:
        print(" ".join(cells_near_street(a.name, a.geo, a.near_street, a.within)))
        return
    meta = build(a.name, a.geo)
    print(json.dumps({"proxies": len(meta["nodes"]), "missing_cells": meta["missing_cells"], **meta["budget"]}))
    if a.check and not meta["budget"]["ok"]:
        sys.exit(1)


if __name__ == "__main__":
    main()
