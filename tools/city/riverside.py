"""Rhone riverside of a generated street: water, quay parapets and a road bridge.

For the drivable loop of 0.3 (rue de la Republique -> quai -> pont Lafayette ->
left bank). Greybox, vertex colours like the building shells.

Input: assets/geo/processed/<zone>/water.geojson (BD TOPO surfaces, local metres),
assets/geo/raw/<zone>/grandlyon_bridges.geojson (Grand Lyon bridge decks, Lambert-93)
and the LiDAR terrain (lidar_mnt.f32).
Output (not committed): gen/<focus>/riverside.glb and riverside.json
  SM_River_Water      flat water surface at the river level (no collision)
  SM_Bridge_<name>    deck (top follows the two banks), sides, parapets
  SM_Quay_Parapet     stone parapet along the water's edge, except under the bridge
riverside.json lists the nodes with their Unreal position, read by import_city.py.

  python3 tools/city/riverside.py --name presqu-ile --focus rue-de-la-republique \
      --bbox 250,-60,700,220 --bridge 41:Lafayette
"""

import argparse
import json
import os
import sys

import numpy as np
import shapely
from shapely.geometry import LineString, MultiPolygon, Point, Polygon, box, shape
from shapely.ops import unary_union

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import citylib as cl  # noqa: E402
import meshkit  # noqa: E402
import raster  # noqa: E402

WATER = (0.03, 0.05, 0.06)
DECK = (0.05, 0.05, 0.055)
STONE = (0.60, 0.57, 0.52)
DECK_THICK_M = 1.2
PARAPET_H_M, PARAPET_W_M = 1.05, 0.4
WATER_CELL_M = 4.0
MAX_STEP_M = 0.12            # deck to quay, a car rolls over it
BED_DEPTH_M = 2.5            # river bed with collision under the water surface
RECOVERY_CELL_M = 25.0
EXIT_EVERY_M = 25.0
STREET_ABOVE_WATER_M = 5.0   # quays at street level, not the bas-ports


def polys(geom):
    if geom.is_empty:
        return []
    if isinstance(geom, Polygon):
        return [geom]
    return [g for g in getattr(geom, "geoms", []) if isinstance(g, Polygon)]


def triangles(poly):
    """Constrained Delaunay triangles of a polygon (holes respected)."""
    out = []
    for t in shapely.constrained_delaunay_triangles(poly).geoms:
        c = list(t.exterior.coords)[:3]
        out.append(c)
    return out


def ground(mnt, origin, origin_alt, x, y):
    z = mnt.sample(np.asarray(x) + origin[0], np.asarray(y) + origin[1]) - origin_alt
    return z


def water_mesh(water, mnt, origin, origin_alt):
    """Flat surface just above the LiDAR ground inside the water: the LiDAR terrain
    is the (noisy) water surface there, so a plane at its 90th percentile + 10 cm
    covers it without z-fighting; the quays stand metres higher."""
    xs, ys = [], []
    x0, y0, x1, y1 = water.bounds
    for x in np.arange(x0, x1, WATER_CELL_M):
        for y in np.arange(y0, y1, WATER_CELL_M):
            if water.contains(Point(x, y)):
                xs.append(x)
                ys.append(y)
    z = ground(mnt, origin, origin_alt, xs, ys)
    level = float(np.nanpercentile(z, 90)) + 0.1 if len(xs) else 0.0
    m = meshkit.Mesh()
    for p in polys(water):
        for a, b, c in triangles(p):
            pts = [(a[0], a[1], level), (b[0], b[1], level), (c[0], c[1], level)]
            if np.cross(np.subtract(pts[1], pts[0]), np.subtract(pts[2], pts[0]))[2] < 0:
                pts[1], pts[2] = pts[2], pts[1]
            m.tri(*pts, WATER)
    return m, level


def deck_geometry(deck, mnt, mns, origin, origin_alt):
    """Deck footprint stretched to meet the quays, and its top height.

    The Grand Lyon footprint stops where the deck leaves the lower quays (bas-ports,
    3-7 m down), not where it meets the street. The top follows the LiDAR surface
    (MNS, the deck itself) at both ends; each end is stretched along the axis until
    the ground (MNT) is within MAX_STEP_M of it, so cars roll on without a step."""
    rect = deck.minimum_rotated_rectangle
    c = list(rect.exterior.coords)[:4]
    e = [np.subtract(c[(i + 1) % 4], c[i]) for i in range(4)]
    i = int(np.argmax([np.linalg.norm(v) for v in e[:2]]))
    axis = e[i] / np.linalg.norm(e[i])
    side = np.array([-axis[1], axis[0]])
    width = np.linalg.norm(e[(i + 1) % 4])
    cen = np.array(rect.centroid.coords[0])
    half = np.linalg.norm(e[i]) / 2

    def sample(r, p):
        return float(np.nanmedian(r.sample(np.array([p[0] + d for d in (-1, 0, 1)]) + origin[0],
                                           np.array([p[1] + d for d in (-1, 0, 1)]) + origin[1]) - origin_alt))
    z_end, ext = [], []
    for sgn in (-1, 1):
        z = sample(mns, cen + sgn * axis * (half - 3.0))
        d_ok = 0.0
        for d in np.arange(0.0, 20.0, 0.5):
            if abs(sample(mnt, cen + sgn * axis * (half + d)) - z) < MAX_STEP_M:
                d_ok = d + 1.0
                break
        # Settle the end on the street it meets: median ground across the deck width.
        tip = cen + sgn * axis * (half + d_ok + 1.0)
        line = [sample(mnt, tip + side * w) for w in np.linspace(-width / 2 + 1, width / 2 - 1, 9)]
        z_end.append(float(np.median(line)) if d_ok else z)
        ext.append(d_ok)
    a0, a1 = -(half + ext[0]), half + ext[1]
    corners = [cen + axis * a + side * w for a, w in ((a0, -width / 2), (a1, -width / 2), (a1, width / 2), (a0, width / 2))]
    full = shapely.geometry.polygon.orient(deck.union(Polygon(corners)).buffer(0), 1.0)
    if not isinstance(full, Polygon):
        full = max(polys(full), key=lambda p: p.area)

    def at(x, y):
        t = (np.dot(np.subtract((x, y), cen), axis) - a0) / (a1 - a0)
        return z_end[0] + (z_end[1] - z_end[0]) * min(1.0, max(0.0, t))
    return full, at, axis, z_end, ext


def bridge_mesh(deck, mnt, mns, origin, origin_alt):
    deck, at, axis, banks, ext = deck_geometry(deck, mnt, mns, origin, origin_alt)
    m = meshkit.Mesh()
    for a, b, c in triangles(deck):
        top = [(p[0], p[1], at(*p)) for p in (a, b, c)]
        if np.cross(np.subtract(top[1], top[0]), np.subtract(top[2], top[0]))[2] < 0:
            top[1], top[2] = top[2], top[1]
        m.tri(*top, DECK)
        bot = [(p[0], p[1], p[2] - DECK_THICK_M) for p in reversed(top)]
        m.tri(*bot, STONE)
    ring = list(deck.exterior.coords)
    if deck.exterior.is_ccw is False:
        ring = ring[::-1]
    for p, q in zip(ring, ring[1:]):
        zp, zq = at(*p), at(*q)
        m.quad((p[0], p[1], zp - DECK_THICK_M), (q[0], q[1], zq - DECK_THICK_M), (q[0], q[1], zq), (p[0], p[1], zp),
               STONE)
    # Parapets on the two long sides of the deck (edges parallel to its axis).
    inner = deck.buffer(-PARAPET_W_M, join_style=2)
    band = deck.difference(inner)
    for p, q in zip(ring, ring[1:]):
        d = np.subtract(q, p)
        L = np.linalg.norm(d)
        if L < 2.0 or abs(np.dot(d / L, axis)) < 0.9:
            continue
        n = np.array([d[1], -d[0]]) / L          # outward for a counter-clockwise ring
        steps = max(1, int(L // 4))
        for k in range(steps):
            a = np.add(p, d * k / steps)
            b = np.add(p, d * (k + 1) / steps)
            za, zb = at(*a), at(*b)
            ia, ib = a - n * PARAPET_W_M, b - n * PARAPET_W_M
            box_between(m, a, b, ia, ib, za, zb, PARAPET_H_M)
    del band
    return m, banks, ext, deck


def box_between(m, a, b, ia, ib, za, zb, h):
    """A wall slab between the outer edge a-b and the inner edge ia-ib, from the ground up h."""
    A, B = (a[0], a[1], za), (b[0], b[1], zb)
    IA, IB = (ia[0], ia[1], za), (ib[0], ib[1], zb)
    At, Bt = (a[0], a[1], za + h), (b[0], b[1], zb + h)
    IAt, IBt = (ia[0], ia[1], za + h), (ib[0], ib[1], zb + h)
    m.quad(A, B, Bt, At, STONE)         # outer face
    m.quad(IB, IA, IAt, IBt, STONE)     # inner face
    m.quad(At, Bt, IBt, IAt, STONE)     # top
    m.quad(IA, A, At, IAt, STONE)       # ends
    m.quad(B, IB, IBt, Bt, STONE)


def quay_mesh(water, bbox, bridges, mnt, origin, origin_alt, water_level):
    """Parapet along the water's edge inside the box, land side, cut under the bridges.
    water is the unclipped river: the box sides crossing it are not edges."""
    edge = water.boundary.intersection(bbox)
    keep_out = unary_union([b.buffer(3.0) for b in bridges]) if bridges else None
    if keep_out is not None:
        edge = edge.difference(keep_out)
    lines = [g for g in getattr(edge, "geoms", [edge]) if isinstance(g, LineString)]
    m = meshkit.Mesh()
    length = 0.0
    exits, since = [], EXIT_EVERY_M
    for line in lines:
        pts = list(line.coords)
        dense = []
        for p, q in zip(pts, pts[1:]):        # 4 m pieces so the parapet follows the quay
            k = max(1, int(np.hypot(q[0] - p[0], q[1] - p[1]) // 4.0))
            dense += [tuple(np.add(p, np.subtract(q, p) * j / k)) for j in range(k)]
        dense.append(pts[-1])
        for p, q in zip(dense, dense[1:]):
            d = np.subtract(q, p)
            L = float(np.linalg.norm(d))
            if L < 0.2:
                continue
            n = np.array([-d[1], d[0]]) / L
            mid = np.add(p, d / 2)
            if water.contains(Point(*(mid + n * 1.0))):
                n = -n                      # n points to the land
            a, b = np.add(p, n * 0.3), np.add(q, n * 0.3)
            ia, ib = a + n * PARAPET_W_M, b + n * PARAPET_W_M
            za, zb = (float(v) for v in ground(mnt, origin, origin_alt, [a[0] + n[0] * 1.5, b[0] + n[0] * 1.5],
                                               [a[1] + n[1] * 1.5, b[1] + n[1] * 1.5]))
            if np.isnan(za) or np.isnan(zb):
                continue
            box_between(m, b, a, ib, ia, za, zb, PARAPET_H_M)
            length += L
            since += L
            if since >= EXIT_EVERY_M:         # where someone fished out of the river stands
                since = 0.0
                # Street level, not the lower quay (bas-port) 3-6 m down: walk inland
                # until the ground stands well above the water.
                e, ez = None, float("nan")
                for d_in in np.arange(3.0, 60.0, 1.0):
                    p_in = mid + n * d_in
                    z_in = float(ground(mnt, origin, origin_alt, [p_in[0]], [p_in[1]])[0])
                    if z_in > water_level + STREET_ABOVE_WATER_M:
                        e = p_in + n * 5.0
                        ez = float(ground(mnt, origin, origin_alt, [e[0]], [e[1]])[0])
                        break
                if e is not None and not np.isnan(ez):
                    exits.append({"x_m": round(float(e[0]), 2), "y_m": round(float(e[1]), 2), "z_m": round(ez, 2),
                                  "yaw_deg": round(float(np.degrees(np.arctan2(n[1], n[0]))), 1)})
    return m, length, exits


def river_safety(water, level):
    """Fall-in safety: a river bed with collision BED_DEPTH_M under the surface (nothing
    falls through the world) and recovery boxes over the water, which the game uses to
    swim or fish the player and the cars out to the nearest quay exit."""
    bed = meshkit.Mesh()
    for p in polys(water):
        for a, b, c in triangles(p):
            pts = [(a[0], a[1], level - BED_DEPTH_M), (b[0], b[1], level - BED_DEPTH_M), (c[0], c[1], level - BED_DEPTH_M)]
            if np.cross(np.subtract(pts[1], pts[0]), np.subtract(pts[2], pts[0]))[2] < 0:
                pts[1], pts[2] = pts[2], pts[1]
            bed.tri(*pts, WATER)
    boxes = []
    x0, y0, x1, y1 = water.bounds
    for x in np.arange(x0, x1, RECOVERY_CELL_M):
        for y in np.arange(y0, y1, RECOVERY_CELL_M):
            cell = box(x, y, x + RECOVERY_CELL_M, y + RECOVERY_CELL_M)
            if water.intersection(cell).area > 0.3 * cell.area:
                boxes.append({"centre_m": [round(x + RECOVERY_CELL_M / 2, 2), round(y + RECOVERY_CELL_M / 2, 2),
                                           round(level - BED_DEPTH_M / 2 + 0.3, 2)],
                              "half_m": [RECOVERY_CELL_M / 2, RECOVERY_CELL_M / 2, BED_DEPTH_M / 2 + 0.6]})
    return bed, boxes


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    ap.add_argument("--bbox", default="250,-60,700,220", help="x0,y0,x1,y1 local metres")
    ap.add_argument("--bridge", action="append", default=[], metavar="GID:NAME",
                    help="Grand Lyon bridge deck to build (gid of grandlyon_bridges), e.g. 41:Lafayette")
    a = ap.parse_args()
    raw = os.path.join(a.geo, "raw", a.name)
    proc = os.path.join(a.geo, "processed", a.name)
    out = os.path.join(proc, "gen", a.focus)
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    origin, origin_alt = man["origin_lambert93_m"], man["origin_altitude_m"]
    mnt = raster.Raster.load(raw, "mnt")
    mns = raster.Raster.load(raw, "mns")
    bx = box(*[float(v) for v in a.bbox.split(",")])

    with open(os.path.join(proc, "water.geojson"), encoding="utf-8") as f:
        river = unary_union([shape(ft["geometry"]) for ft in json.load(f)["features"]
                             if ft["properties"].get("persistance") == "Permanent"])
    water = river.intersection(bx)
    with open(os.path.join(raw, "grandlyon_bridges.geojson"), encoding="utf-8") as f:
        by_gid = {str(ft["properties"]["gid"]): ft for ft in json.load(f)["features"]}
    decks = []
    for spec in a.bridge or ["41:Lafayette"]:
        gid, name = spec.split(":", 1)
        g = shape(by_gid[gid]["geometry"])
        g = shapely.transform(g, lambda xy: xy - np.array(origin[:2]))
        g = max(polys(g), key=lambda p: p.area).buffer(0)
        decks.append((name, shapely.geometry.polygon.orient(g, 1.0)))

    items, meta = [], {"zone": a.name, "focus": a.focus, "bbox_m": a.bbox, "nodes": []}
    wm, level = water_mesh(water, mnt, origin, origin_alt)
    items.append(("SM_River_Water", wm, (0.0, 0.0, 0.0)))
    meta["water_level_m"] = round(level, 2)
    fulls = []
    for name, deck in decks:
        bm, banks, ext, full = bridge_mesh(deck, mnt, mns, origin, origin_alt)
        fulls.append(full)
        items.append(("SM_Bridge_" + name, bm, (0.0, 0.0, 0.0)))
        meta.setdefault("bridges", []).append({"name": name, "deck_m2": round(full.area), "end_z_m": [round(z, 2) for z in banks],
                                               "stretched_to_quays_m": ext, "max_step_m": MAX_STEP_M})
    qm, qlen, exits = quay_mesh(river, bx, fulls, mnt, origin, origin_alt, level)
    items.append(("SM_Quay_Parapet", qm, (0.0, 0.0, 0.0)))
    meta["quay_parapet_m"] = round(qlen)
    bed, boxes = river_safety(water, level)
    items.append(("SM_River_Bed", bed, (0.0, 0.0, 0.0)))
    meta["recovery"] = {"tag_boxes": "GTP_RiverRecovery", "tag_exits": "GTP_RiverExit",
                        "bed_z_m": round(level - BED_DEPTH_M, 2), "boxes": boxes, "exits": exits}
    for n, m, o in items:
        meta["nodes"].append({"name": n, "triangles": m.tris, "ue_cm": list(cl.to_unreal(*o)),
                              "collision": n != "SM_River_Water"})
    meshkit.write_glb(os.path.join(out, "riverside.glb"), [i for i in items if i[1].tris])
    with open(os.path.join(out, "riverside.json"), "w") as f:
        json.dump(meta, f, indent=2)
    print(json.dumps(meta, indent=2))


if __name__ == "__main__":
    main()
