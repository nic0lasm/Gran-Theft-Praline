"""The heights of the terrain tiles read from terrain.glb, in plain Python (no numpy), so the same
code runs inside the Unreal Editor (tools/city/unreal/cover_pits.py) and in the tests.

In the headless editor no trace answers on the terrain tiles: the pits step sampled 242 000
points in five seconds, read no height at all and reported "no pit" as a success from gate 78
to gate 81, while the stolen car rolled into a car park ramp. The tiles are a 4 m grid of the
LiDAR model (terrain.py, written by meshkit.write_glb): their corners are the ground the game
collides with, and they can be read from the file, or from the meshes themselves, with no trace.

  read_glb(path)    [(name, [(x, y, z)])] per tile, Unreal cm (x east, y south, z up)
  box(points)       (x0, y0, x1, y1, z0, z1)
  same_tiles(a, b)  every box of a has its twin in b: the file is the terrain of the map
  grid(tiles)       (rows of heights or None, x0, y0, step_cm) for pits.pits()
"""

import array
import json
import struct
import sys

FLOAT, VEC3 = 5126, "VEC3"


def read_glb(path):
    """Positions of every mesh node of a .glb, in Unreal cm. glTF is Y up, in metres, and
    meshkit writes local (x east, y north, z up) as (x, z, -y): Unreal is (x, -y, z) x 100."""
    with open(path, "rb") as f:
        data = f.read()
    magic, _, _ = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF":
        raise ValueError("not a .glb: " + path)
    off, doc, blob = 12, None, b""
    while off + 8 <= len(data):
        size, kind = struct.unpack_from("<I4s", data, off)
        chunk = data[off + 8:off + 8 + size]
        if kind == b"JSON":
            doc = json.loads(chunk.decode("utf-8"))
        elif kind == b"BIN\0":
            blob = chunk
        off += 8 + size
    if doc is None:
        raise ValueError("no JSON chunk in " + path)
    out = []
    for node in doc.get("nodes", []):
        if "mesh" not in node:
            continue
        tx, ty, tz = node.get("translation", (0.0, 0.0, 0.0))
        pts = []
        for prim in doc["meshes"][node["mesh"]]["primitives"]:
            acc = doc["accessors"][prim["attributes"]["POSITION"]]
            view = doc["bufferViews"][acc["bufferView"]]
            if acc["componentType"] != FLOAT or acc["type"] != VEC3 or view.get("byteStride", 12) != 12:
                raise ValueError("positions of %s are not packed float32 vec3" % node.get("name"))
            start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
            v = array.array("f")
            v.frombytes(blob[start:start + 12 * acc["count"]])
            if sys.byteorder != "little":
                v.byteswap()
            for i in range(0, len(v), 3):
                pts.append(((v[i] + tx) * 100.0, (v[i + 2] + tz) * 100.0, (v[i + 1] + ty) * 100.0))
        out.append((node.get("name", ""), pts))
    return out


def box(points):
    xs, ys, zs = [p[0] for p in points], [p[1] for p in points], [p[2] for p in points]
    return min(xs), min(ys), max(xs), max(ys), min(zs), max(zs)


def same_tiles(boxes, map_boxes, tol_cm=5.0):
    """True when each box has a twin among map_boxes, the six values within tol_cm, and there are
    as many of each: the heights read are those of the tiles the map carries."""
    if not boxes or len(boxes) != len(map_boxes):
        return False
    left = list(map_boxes)
    for b in boxes:
        twin = next((m for m in left if all(abs(u - v) <= tol_cm for u, v in zip(b, m))), None)
        if twin is None:
            return False
        left.remove(twin)
    return True


ON_GRID_CM = 3          # a corner this close to a line of the lattice is on it
ON_GRID_SHARE = 0.95    # the lattice is the widest one that carries this share of the corners


def _lattice(counts, share=ON_GRID_SHARE):
    """(step, first, last) of the lines most corners sit on, from {coordinate: corners}. The widest gap
    between two neighbouring coordinates that still carries `share` of the corners: a few corners
    off the lattice (256 of 123 104 on the tiles of gate 83) halved the smallest gap, left three
    cells in four empty and hid every pit."""
    xs = sorted(counts)
    total = sum(counts.values())
    gaps = sorted({b - a for a, b in zip(xs, xs[1:]) if b - a > 1}, reverse=True)
    for step in gaps:
        by_rest = {}
        for x, n in counts.items():
            by_rest[x % step] = by_rest.get(x % step, 0) + n
        rest = max(by_rest, key=by_rest.get)
        on = sum(n for r, n in by_rest.items()
                 if min((r - rest) % step, (rest - r) % step) <= ON_GRID_CM)
        if on >= share * total:
            on_it = [x for x in xs if min((x - rest) % step, (rest - x) % step) <= ON_GRID_CM]
            return float(step), on_it[0], on_it[-1]
    return 0.0, 0, 0


def grid(tiles, step_cm=None, stats=None):
    """(z, x0, y0, step_cm): z[r][c] is the height at (x0 + c * step, y0 + r * step), None where no
    tile has a corner. The step is that of the lattice nearly every corner sits on (_lattice);
    corners off it are left out and counted in stats["off_grid"] when a dict is given."""
    cells, nx, ny = {}, {}, {}
    for _, pts in tiles:
        for x, y, z in pts:
            k = (int(round(x)), int(round(y)))
            if k not in cells:
                nx[k[0]] = nx.get(k[0], 0) + 1
                ny[k[1]] = ny.get(k[1], 0) + 1
            cells[k] = z
    if not cells:
        return [], 0.0, 0.0, 0.0
    sx, x0, x1 = _lattice(nx)
    sy, y0, y1 = _lattice(ny)
    if step_cm is None:
        step_cm = max(sx, sy) if min(sx, sy) > 0 else 0.0
    if step_cm <= 0:
        return [], 0.0, 0.0, 0.0
    cols, rows = int(round((x1 - x0) / step_cm)) + 1, int(round((y1 - y0) / step_cm)) + 1
    z = [[None] * cols for _ in range(rows)]
    off = 0
    for (x, y), h in cells.items():
        c, r = (x - x0) / step_cm, (y - y0) / step_cm
        ci, ri = int(round(c)), int(round(r))
        if (abs(c - ci) * step_cm > ON_GRID_CM or abs(r - ri) * step_cm > ON_GRID_CM
                or not (0 <= ci < cols and 0 <= ri < rows)):
            off += 1
            continue
        z[ri][ci] = h
    if stats is not None:
        stats["off_grid"] = off
    return z, float(x0), float(y0), step_cm
