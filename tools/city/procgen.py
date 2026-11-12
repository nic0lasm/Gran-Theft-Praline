"""Procedural building generation: LiDAR-fitted roofs and Lyon facade layouts.

Everything here works in local metres (x = east, y = north, z = altitude minus
the dataset origin altitude), the same frame as build.py's tables.

Roofs
-----
* eave height  = low quantile of the LiDAR surface (MNS) in a band just inside
  the footprint, so roof edges sit where the gutters really are;
* ridges       = Metropole de Lyon 3D ridge lines (2012) clipped to the
  footprint, kept only when the 2021+ LiDAR still agrees with them;
* roof surface = constrained triangulation of footprint + ridges (gables and
  hips fall out of it); flat roofs and roofs without ridges use the LiDAR.

Facades
-------
Each footprint edge is classified (street / courtyard / party wall shared with
a neighbour), cut into storeys and bays following the building's typology,
and filled with module kinds (shop, entrance, window, balcony window...).
The module list is the input of the engine's procedural facade kit; the same
list builds the greybox mesh used for checks.
"""

import hashlib
import math

import numpy as np
import shapely
from shapely.geometry import LineString, Point, Polygon
from shapely.strtree import STRtree

# --- Typologies ------------------------------------------------------------------
# Streets cut through the Presqu'ile under the Second Empire (1853-1870): stone
# facades, entresol over the shops, balconies on the noble floor and the top floor.
HAUSSMANN_STREETS = {
    "Rue de la République", "Place de la République", "Rue Président Edouard Herriot",
    "Rue de la Bourse", "Place des Jacobins", "Place des Cordeliers", "Rue Président Carnot",
    "Place de la Bourse", "Rue Jean de Tournes", "Place Bellecour",
}

TYPOLOGY = {
    # ground storey, entresol (0 = none), upper storey, bay width (metres)
    "haussmann": {"gf": 4.2, "entresol": 2.6, "upper": 3.3, "bay": 3.3},
    "ancien": {"gf": 3.8, "entresol": 0.0, "upper": 3.0, "bay": 2.8},
    "moderne": {"gf": 4.0, "entresol": 0.0, "upper": 2.9, "bay": 3.2},
    "monument": {"gf": 6.0, "entresol": 0.0, "upper": 6.0, "bay": 6.0},
}

# Lyon render colours (ochres, salmon pinks, straw yellows of the Presqu'ile).
PALETTE = {
    "haussmann": ["#D8CDB6", "#D2C5AA", "#DDD3BE", "#CBBE9F"],
    "ancien": ["#D9A55B", "#D98C6F", "#C9764F", "#E3C27A", "#D8C3A0", "#E2A98F", "#C8A27C", "#E0B48A"],
    "moderne": ["#BDBDB8", "#C9C6BF", "#A9A9A6"],
    "monument": ["#CFC6B4"],
}

PARTY_PROBE_M = 0.6      # an edge whose outside is another footprint at this distance is a party wall
STREET_PROBE_M = 4.0     # probe point in front of an edge to find the street
STREET_MARGIN_M = 7.0    # probe within half road width + this margin => street facade
MIN_FACADE_EDGE_M = 1.8  # shorter edges get a blank module
# A facade on a square or a quay (place Bellecour, place des Jacobins) has no road within the
# probe, yet it is no courtyard: three rays (normal, +-35 deg) that leave the block without
# meeting another footprint over OPEN_SPACE_M mark it as a street front (named after the nearest
# road, empty if none). Measured courtyards of the Presqu'ile are under 20 m across.
OPEN_SPACE_M = 24.0
OPEN_SPACE_STEP_M = 2.0
# Mansard roof of the Second Empire blocks (rue de la Republique): the LiDAR cannot tell the
# near-vertical zinc brisis from the wall (no slope within 3 m of the edge, 12/11), so a wall
# measured above MANSARD_MIN_WALL_M ends with a mansard level: stone storeys up to the cornice,
# then a brisis with one dormer per street bay (modules "dormer" / "mansard_blank").
MANSARD_MIN_WALL_M = 19.0
MANSARD_H_M = 3.2
MANSARD_H_RANGE = (2.8, 3.8)   # the mansard takes up what the stone storeys cannot
MANSARD_INSET_M = 0.8       # the brisis top stands this far behind the facade plane
DORMER_MIN_BAY_M = 2.2
MAX_ROOF_RISE_M = 9.0   # domes and spires are monuments, modelled by hand later
RIDGE_TOLERANCE_M = 2.5  # 2012 ridges that the LiDAR contradicts by more than this are dropped


def stable_pick(key, options):
    h = int(hashlib.sha1(key.encode("utf-8")).hexdigest()[:8], 16)
    return options[h % len(options)]


def material_class(code):
    """BD TOPO material code (two digits: main, secondary) -> main class digit as str."""
    code = (code or "").strip()
    return code[0] if code and code[0] != "0" else ""


# --- Roofs -------------------------------------------------------------------------
def interp_z(line3d, pts2d):
    """Altitude along a 3D polyline for 2D points lying on (or near) it."""
    coords = np.asarray(line3d.coords)
    seg = np.hypot(np.diff(coords[:, 0]), np.diff(coords[:, 1]))
    cum = np.concatenate([[0.0], np.cumsum(seg)])
    flat = LineString(coords[:, :2])
    return [float(np.interp(flat.project(Point(p)), cum, coords[:, 2])) for p in pts2d]


class RidgeIndex:
    def __init__(self, ridges3d):
        """ridges3d: list of 3D coordinate lists in local metres."""
        self.lines = [LineString(c) for c in ridges3d if len(c) >= 2]
        self.tree = STRtree([LineString(np.asarray(l.coords)[:, :2]) for l in self.lines])

    def clipped(self, poly):
        """Ridge pieces inside poly: list of [(x, y, z), ...]."""
        inner = poly.buffer(-0.15)
        if inner.is_empty:
            return []
        out = []
        for i in self.tree.query(inner, predicate="intersects"):
            line3d = self.lines[i]
            piece = shapely.intersection(LineString(np.asarray(line3d.coords)[:, :2]), inner)
            for part in getattr(piece, "geoms", [piece]):
                if part.geom_type != "LineString" or part.length < 0.5:
                    continue
                xy = list(part.coords)
                out.append([(x, y, z) for (x, y), z in zip(xy, interp_z(line3d, xy))])
        return out


COURT_MIN_CELLS = 24     # 6 m2 of 0.5 m LiDAR cells
COURT_MAX_HEIGHT_M = 2.5  # below this above the terrain: open courtyard
PODIUM_MIN_DROP_M = 4.0   # this far below the main roof: courtyard covered by a ground-floor roof


def carve_courtyards(poly, mns, mnt, to_world, to_local):
    """Split a footprint where the LiDAR shows courtyards.

    BD TOPO footprints of Lyon blocks usually include the inner courtyard, which
    is either open (surface at terrain level) or covered at the first floor by
    the shops' glass or zinc roofs. Connected areas of at least COURT_MIN_CELLS
    cells far below the main roof are cut out; covered ones come back as low
    "podium" parts with their own flat roof.

    Returns (parts, removed_share): parts = [(polygon, podium_height_or_None)].
    """
    from scipy import ndimage
    from shapely.geometry import box

    world_ring = [to_world(p) for p in list(poly.exterior.coords)[:-1]]
    x, y, v = mns.cells_in_ring(world_ring)
    if len(v) < COURT_MIN_CELLS * 2:
        return [(poly, None)], 0.0
    h = v - mnt.sample(x, y)
    h_main = float(np.nanpercentile(h, 75))
    thr = max(COURT_MAX_HEIGHT_M, min(h_main - PODIUM_MIN_DROP_M, 0.6 * h_main))
    low = h < thr
    if np.count_nonzero(low) < COURT_MIN_CELLS:
        return [(poly, None)], 0.0
    res = mns.res
    ci = np.round((x - x.min()) / res).astype(int)
    ri = np.round((y - y.min()) / res).astype(int)
    grid = np.zeros((ri.max() + 1, ci.max() + 1), bool)
    grid[ri[low], ci[low]] = True
    hgrid = np.full(grid.shape, np.nan)
    hgrid[ri, ci] = h
    labels, n = ndimage.label(grid)
    sizes = ndimage.sum(grid, labels, range(1, n + 1))
    courts = []
    for k in np.nonzero(sizes >= COURT_MIN_CELLS)[0]:
        rr, cc = np.nonzero(labels == k + 1)
        cells = []
        for r_, c_ in zip(rr, cc):
            lx, ly = to_local((x.min() + c_ * res, y.min() + r_ * res))
            cells.append(box(lx - res / 2, ly - res / 2, lx + res / 2, ly + res / 2))
        shape = shapely.unary_union(cells).buffer(0.3, join_style="mitre").buffer(-0.3, join_style="mitre")
        shape = shape.simplify(0.35).intersection(poly)
        hc = float(np.nanmedian(hgrid[rr, cc]))
        courts.append((shape, hc if hc > COURT_MAX_HEIGHT_M else None))
    if not courts:
        return [(poly, None)], 0.0
    rest = poly.difference(shapely.unary_union([c[0] for c in courts]))
    parts = [(p, None) for p in getattr(rest, "geoms", [rest]) if p.geom_type == "Polygon" and p.area >= 8.0]
    for shape, hc in courts:
        if hc is not None:
            parts += [(p, hc) for p in getattr(shape, "geoms", [shape]) if p.geom_type == "Polygon" and p.area >= 4.0]
    open_area = sum(c[0].area for c in courts if c[1] is None)
    return parts, open_area / poly.area


def fit_roof(poly, mns, origin_alt, ground_z, ridges, to_world, bdtopo_height=None, podium_h=None):
    """Return a roof description for one footprint.

    mns       raster.Raster in Lambert-93 with absolute altitudes
    to_world  local (x, y) -> Lambert-93 (x, y)
    """
    if podium_h is not None:
        return {"kind": "flat", "eave_z": ground_z + podium_h, "top_z": ground_z + podium_h, "ridges": [],
                "lidar_cells": 0, "podium": True}
    ring = list(poly.exterior.coords)[:-1]
    world_ring = [to_world(p) for p in ring]
    x, y, v = mns.cells_in_ring(world_ring)
    if len(v) < 6:
        return {"kind": "flat", "eave_z": ground_z + 3.0, "top_z": ground_z + 3.0, "ridges": [],
                "lidar_cells": int(len(v))}
    v = v - origin_alt
    import raster as _r  # local import keeps this module importable without the raster helpers
    keep = np.ones(len(v), bool)
    for hole in poly.interiors:  # courtyards inside the footprint are not roof
        keep &= ~_r.points_in_ring(x, y, [to_world(p) for p in list(hole.coords)[:-1]])
    x, y, v = x[keep], y[keep], v[keep]
    raised = v > ground_z + 2.0
    if np.count_nonzero(raised) < max(6, 0.2 * len(v)):
        # the LiDAR sees ground here (passage, demolition, covered courtyard): fall back to BD TOPO
        hb = bdtopo_height if bdtopo_height else 3.0
        return {"kind": "flat", "eave_z": ground_z + hb, "top_z": ground_z + hb, "ridges": [],
                "lidar_cells": int(len(v)), "lidar_missing": True}
    x, y, v = x[raised], y[raised], v[raised]
    d = _r.dist_to_ring(x, y, world_ring)
    band = v[(d >= 0.4) & (d <= 1.6)]
    if len(band) < 4:
        band = v
    eave = float(np.percentile(band, 25))
    core = v[d >= 1.0] if np.count_nonzero(d >= 1.0) >= 6 else v
    p10, p50, p90, p98 = (float(q) for q in np.percentile(core, [10, 50, 90, 98]))
    # Roof pitch at most ~45 degrees across the short side of the footprint (no Lyon roof rises more).
    rect = poly.minimum_rotated_rectangle
    sides = sorted(LineString(rect.exterior.coords[i:i + 2]).length for i in range(2))
    max_rise = min(MAX_ROOF_RISE_M, max(2.0, sides[0] / 2))
    eave = max(min(eave, p50), ground_z + 2.5, p98 - max_rise)

    kept = []
    for r in ridges:
        rz = np.array([p[2] for p in r])
        lid = mns.sample(*np.array([to_world((p[0], p[1])) for p in r]).T) - origin_alt
        if np.all(np.isnan(lid)) or np.nanmedian(np.abs(lid - rz)) <= RIDGE_TOLERANCE_M:
            kept.append([(p[0], p[1], max(p[2], eave + 0.3)) for p in r])

    if kept:
        top = max(p[2] for r in kept for p in r)
        return {"kind": "ridged", "eave_z": eave, "top_z": top, "ridges": kept, "lidar_cells": int(len(v)),
                "lidar_p50": p50, "lidar_p98": p98}
    if p90 - p10 < 1.2:
        return {"kind": "flat", "eave_z": p50, "top_z": p50, "ridges": [], "lidar_cells": int(len(v)),
                "lidar_p50": p50, "lidar_p98": p98}
    return {"kind": "lidar", "eave_z": eave, "top_z": p98, "ridges": [], "lidar_cells": int(len(v)),
            "lidar_p50": p50, "lidar_p98": p98}


def roof_triangles(poly, roof, mns=None, origin_alt=0.0, to_world=None, grid_m=3.0):
    """Triangulate a roof. Returns (vertices Nx3, triangles Mx3).

    Outline and ridge lines are noded first (split where they cross, snapped to
    1 cm) so the constrained triangulation never sees overlapping input.
    """
    import triangle as tr

    eave, top = roof["eave_z"], roof["top_z"]
    rings = [poly.exterior] + list(poly.interiors)
    holes = [Polygon(h).representative_point() for h in poly.interiors]
    ridge3d = [LineString(r) for r in roof["ridges"] if len(r) >= 2]
    lines = [LineString(r.coords) for r in rings] + [LineString(np.asarray(r.coords)[:, :2]) for r in ridge3d]
    noded = shapely.unary_union(shapely.set_precision(shapely.GeometryCollection(lines), 0.01))
    index, verts, segs = {}, [], set()

    def vid(p):
        key = (round(p[0], 2), round(p[1], 2))
        if key not in index:
            index[key] = len(verts)
            verts.append(key)
        return index[key]

    for part in getattr(noded, "geoms", [noded]):
        cs = list(part.coords)
        for a, b in zip(cs, cs[1:]):
            ia, ib = vid(a), vid(b)
            if ia != ib:
                segs.add((min(ia, ib), max(ia, ib)))
    n_lines = len(verts)
    if roof["kind"] == "lidar" and mns is not None:
        minx, miny, maxx, maxy = poly.bounds
        inner = poly.buffer(-1.0)
        for gx in np.arange(minx + grid_m / 2, maxx, grid_m):
            for gy in np.arange(miny + grid_m / 2, maxy, grid_m):
                if inner.contains(Point(gx, gy)):
                    vid((gx, gy))

    def tri(with_segments):
        data = {"vertices": np.array(verts, float)}
        if with_segments:
            data["segments"] = np.array(sorted(segs), int)
            if holes:
                data["holes"] = np.array([(h.x, h.y) for h in holes], float)
        return tr.triangulate(data, "p" if with_segments else "")

    if len(verts) < 3:
        return np.zeros((0, 3)), np.zeros((0, 3), int)
    t = tri(True)
    tv = t["vertices"]
    outline = shapely.MultiLineString([np.asarray(r.coords)[:, :2] for r in rings])
    z = np.empty(len(tv))
    for i, (vx, vy) in enumerate(tv):
        p = Point(vx, vy)
        if i < n_lines or outline.distance(p) < 0.02:
            near = [r for r in ridge3d if LineString(np.asarray(r.coords)[:, :2]).distance(p) < 0.02]
            if near and outline.distance(p) >= 0.02:
                z[i] = max(interp_z(near[0], [(vx, vy)])[0], eave)
                continue
            if outline.distance(p) < 0.02:
                z[i] = eave
                continue
        zi = eave
        if mns is not None and to_world is not None:
            smp = mns.sample(*np.array([to_world((vx, vy))]).T)[0] - origin_alt
            if not np.isnan(smp):
                zi = min(max(float(smp), eave), top)
        z[i] = zi
    tris = t.get("triangles", np.zeros((0, 3), int))
    keep = [k for k, tt in enumerate(tris) if poly.buffer(0.05).contains(Point(tv[tt].mean(0)))]
    return np.column_stack([tv, z]), tris[keep]


# --- Facades -------------------------------------------------------------------------
class Context:
    """Spatial indexes shared by every building of a run."""

    def __init__(self, footprints, roads):
        """footprints: {id: Polygon}; roads: list of (LineString, half_width_m, name)."""
        self.ids = list(footprints)
        self.polys = [footprints[i] for i in self.ids]
        self.tree = STRtree(self.polys)
        self.roads = roads
        self.road_tree = STRtree([r[0] for r in roads]) if roads else None

    def inside_other(self, pt, own_id):
        p = Point(pt)
        for i in self.tree.query(p, predicate="intersects"):
            if self.ids[i] != own_id:
                return True
        return False

    def inside_any(self, pt):
        """True when pt is inside any footprint, the building's own included (a carved courtyard
        is outside its own part, the far side of a U-shaped block is inside it)."""
        p = Point(pt)
        return len(self.tree.query(p, predicate="intersects")) > 0

    def street_at(self, pt):
        """(name, distance beyond the carriageway edge) of the road in front of pt, or (None, inf)."""
        if self.road_tree is None:
            return None, math.inf
        p = Point(pt)
        best = (None, math.inf)
        for i in self.road_tree.query(p.buffer(40.0)):
            line, half, name = self.roads[i]
            d = line.distance(p) - half
            if d < best[1]:
                best = (name, d)
        return best


def edge_frames(poly):
    """Yield (ring_index, edge_index, p0, p1, outward unit normal) for every footprint edge."""
    rings = [list(poly.exterior.coords)[:-1]] + [list(h.coords)[:-1] for h in poly.interiors]
    for ri, ring in enumerate(rings):
        for ei in range(len(ring)):
            p0, p1 = np.array(ring[ei]), np.array(ring[(ei + 1) % len(ring)])
            d = p1 - p0
            L = float(np.hypot(*d))
            if L < 1e-6:
                continue
            n = np.array([d[1], -d[0]]) / L
            mid = (p0 + p1) / 2
            if poly.contains(Point(mid + n * 0.05)):
                n = -n
            yield ri, ei, p0, p1, n


def classify_edge(ctx, bid, p0, p1, n):
    L = float(np.hypot(*(p1 - p0)))
    ts = np.linspace(0.1, 0.9, max(3, int(L)))
    party = sum(ctx.inside_other(p0 + (p1 - p0) * t + n * PARTY_PROBE_M, bid) for t in ts) / len(ts)
    if party >= 0.5:
        return "party", None
    name, d = ctx.street_at((p0 + p1) / 2 + n * STREET_PROBE_M)
    if d <= STREET_MARGIN_M:
        return "street", name
    if faces_open_space(ctx, bid, (p0 + p1) / 2, n):
        return "street", name or ""
    return "court", None


def faces_open_space(ctx, bid, mid, n):
    c, s_ = math.cos(math.radians(35)), math.sin(math.radians(35))
    for dx, dy in ((n[0], n[1]), (c * n[0] - s_ * n[1], s_ * n[0] + c * n[1]),
                   (c * n[0] + s_ * n[1], -s_ * n[0] + c * n[1])):
        for k in range(1, int(OPEN_SPACE_M / OPEN_SPACE_STEP_M) + 1):
            if ctx.inside_any(mid + np.array([dx, dy]) * (k * OPEN_SPACE_STEP_M)):
                return False
    return True


def typology_of(b, street_names, wall_h):
    if b.get("nature") == "Eglise" or b.get("usage_1") == "Religieux":
        return "monument"
    if wall_h >= 14 and any(s in HAUSSMANN_STREETS for s in street_names):
        return "haussmann"
    if material_class(b.get("wall_material")) == "3":
        return "moderne"
    return "ancien"


def storeys(typ, wall_h):
    """List of (kind, z0, height) from the ground to the eave."""
    t = TYPOLOGY[typ]
    if wall_h < t["gf"] + 1.0:
        return [("gf", 0.0, wall_h)]
    levels = [("gf", t["gf"])]
    rest = wall_h - t["gf"]
    if t["entresol"] and rest > t["entresol"] + t["upper"]:
        levels.append(("entresol", t["entresol"]))
        rest -= t["entresol"]
    n_up = max(0, int(round(rest / t["upper"])))
    if n_up == 0:
        levels[0] = ("gf", wall_h) if len(levels) == 1 else levels[0]
        if len(levels) > 1:
            levels[-1] = (levels[-1][0], levels[-1][1] + rest)
    else:
        levels += [("upper", rest / n_up)] * n_up
    out, z = [], 0.0
    for kind, h in levels:
        out.append((kind, z, h))
        z += h
    return out


def levels_for(typ, wall_h):
    """Storeys from the ground to the eave, with the mansard level of a tall Haussmann block.

    The mansard height floats between MANSARD_H_RANGE so that the stone storeys stay close to
    the nominal 3.3 m (rue de la Republique: ground 4.2, entresol 2.6, 4 x 3.3, cornice near
    20 m); a fixed 3.2 m mansard gave upper storeys from 2.9 to 3.7 m (12/11)."""
    if typ == "haussmann" and wall_h >= MANSARD_MIN_WALL_M:
        t = TYPOLOGY[typ]
        lo, hi = MANSARD_H_RANGE
        best = None
        for mh in [lo + k * 0.1 for k in range(int(round((hi - lo) / 0.1)) + 1)]:
            stone = storeys(typ, wall_h - mh)
            ups = [h for k, _, h in stone if k == "upper"]
            err = max((abs(h - t["upper"]) for h in ups), default=9.0) + 0.05 * abs(mh - MANSARD_H_M)
            if best is None or err < best[0] - 1e-9:
                best = (err, mh, stone)
        _, mh, stone = best
        return stone + [("mansard", wall_h - mh, mh)]
    return storeys(typ, wall_h)


def layout_building(ctx, b, poly, ground_z, roof, shops):
    """Module list for one building (see module fields in generate.py)."""
    wall_h = max(2.5, roof["eave_z"] - ground_z)
    edges = []
    for ri, ei, p0, p1, n in edge_frames(poly):
        kind, street = classify_edge(ctx, b["id"], p0, p1, n)
        edges.append((ri, ei, p0, p1, n, kind, street))
    typ = typology_of(b, {e[6] for e in edges if e[6]}, wall_h)
    t = TYPOLOGY[typ]
    colour = stable_pick(b["id"], PALETTE[typ])
    levels = levels_for(typ, wall_h)

    # Shops -> nearest street edge and bay.
    street_edges = [e for e in edges if e[5] == "street" and np.hypot(*(e[3] - e[2])) >= MIN_FACADE_EDGE_M]
    shop_slots = {}
    for s in shops:
        if not street_edges:
            break
        sp = np.array([s["x"], s["y"]])
        best = min(street_edges, key=lambda e: LineString([e[2], e[3]]).distance(Point(sp)))
        L = float(np.hypot(*(best[3] - best[2])))
        nb = max(1, int(round(L / t["bay"])))
        tpos = float(np.dot(sp - best[2], (best[3] - best[2]) / L))
        bay = int(min(nb - 1, max(0, tpos / (L / nb))))
        key = (best[0], best[1])
        taken = shop_slots.setdefault(key, {})
        while bay in taken and bay + 1 < nb:
            bay += 1
        if bay not in taken:
            taken[bay] = s
            if nb >= 6 and bay + 1 < nb and bay + 1 not in taken:
                taken[bay + 1] = dict(s, continued=True)

    modules = []
    entrance_done = False
    stone_top = max(si for si, lv in enumerate(levels) if lv[0] != "mansard")
    for ri, ei, p0, p1, n, kind, street in edges:
        L = float(np.hypot(*(p1 - p0)))
        if kind == "party":
            continue
        if L < MIN_FACADE_EDGE_M:
            modules.append(_module(b, typ, colour, ri, ei, "blank", 0, 0, p0, p1, n, 0.5, ground_z, wall_h, L,
                                   street, kind))
            continue
        nb = max(1, int(round(L / t["bay"])))
        bw = L / nb
        slots = shop_slots.get((ri, ei), {})
        door_bay = None
        if kind == "street" and not entrance_done:
            free = [i for i in range(nb) if i not in slots]
            if free:
                door_bay = free[len(free) // 2]
                entrance_done = True
        for si, (lk, z0, h) in enumerate(levels):
            for bi in range(nb):
                tc = (bi + 0.5) / nb
                if lk == "mansard":
                    mk = "dormer" if kind == "street" and bw >= DORMER_MIN_BAY_M else "mansard_blank"
                elif lk == "gf":
                    if kind == "court":
                        mk = "court_window"
                    elif bi in slots:
                        mk = "shop"
                    elif bi == door_bay:
                        mk = "entrance"
                    elif typ == "haussmann":
                        mk = "shopfront_closed"
                    elif typ == "monument":
                        mk = "monument_bay"
                    else:
                        mk = "gf_window"
                elif lk == "entresol":
                    mk = "entresol_window"
                elif kind == "court":
                    mk = "court_window"
                elif typ == "monument":
                    mk = "monument_bay"
                elif typ == "haussmann" and (si in (2, 5) or (si == stone_top and si < 5)):
                    # art bible: continuous balconies on the 2nd and 5th floors
                    mk = "window_balcony"
                elif typ == "ancien" and si == stone_top and len(levels) >= 4:
                    mk = "window_small"
                else:
                    mk = "window"
                m = _module(b, typ, colour, ri, ei, mk, si, bi, p0, p1, n, tc, ground_z + z0, h, bw, street, kind)
                if mk == "shop":
                    s = slots[bi]
                    m.update(sign_name=s.get("name", ""), sign_brand=s.get("brand", ""),
                             sign_wikidata=s.get("brand_wikidata", ""), shop_osm_id=s.get("osm_id", ""),
                             sign_continued=bool(s.get("continued")))
                modules.append(m)
    info = {"typology": typ, "colour": colour, "wall_h": wall_h, "levels": len(levels),
            "mansard": levels[-1][0] == "mansard",
            "edges": {k: sum(1 for e in edges if e[5] == k) for k in ("street", "court", "party")},
            "edge_list": edges}
    return modules, info


def _module(b, typ, colour, ri, ei, kind, storey, bay, p0, p1, n, tc, z, h, w, street, side):
    c = p0 + (p1 - p0) * tc
    return {"building_id": b["id"], "typology": typ, "colour": colour, "ring": ri, "edge": ei,
            "side": side, "street": street or "", "kind": kind, "storey": storey, "bay": bay,
            "x": float(c[0]), "y": float(c[1]), "z": float(z), "nx": float(n[0]), "ny": float(n[1]),
            "width_m": float(w), "height_m": float(h), "sign_name": "", "sign_brand": "",
            "sign_wikidata": "", "shop_osm_id": "", "sign_continued": False}


def roof_error(verts, tris, px, py, pz):
    """Vertical error of a triangulated roof against LiDAR samples (local metres).

    Returns the absolute differences for samples that fall on a roof triangle.
    """
    errs = []
    for t in tris:
        a, b, c = verts[t[0]], verts[t[1]], verts[t[2]]
        xmin, xmax = min(a[0], b[0], c[0]), max(a[0], b[0], c[0])
        ymin, ymax = min(a[1], b[1], c[1]), max(a[1], b[1], c[1])
        sel = (px >= xmin) & (px <= xmax) & (py >= ymin) & (py <= ymax)
        if not sel.any():
            continue
        x, y = px[sel], py[sel]
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-9:
            continue
        l0 = ((b[1] - c[1]) * (x - c[0]) + (c[0] - b[0]) * (y - c[1])) / d
        l1 = ((c[1] - a[1]) * (x - c[0]) + (a[0] - c[0]) * (y - c[1])) / d
        l2 = 1 - l0 - l1
        inside = (l0 >= 0) & (l1 >= 0) & (l2 >= 0)
        z = l0 * a[2] + l1 * b[2] + l2 * c[2]
        errs.append(np.abs(z[inside] - pz[sel][inside]))
    return np.concatenate(errs) if errs else np.zeros(0)
