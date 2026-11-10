"""Landmarks of the quarter in data and faithful volumes, from OpenStreetMap and the LiDAR.

The generic pipeline (generate.py) gives every BD TOPO building a shell and the Haussmann
facade kit. That is wrong for the monuments a Lyonnais recognises at once: the Opera's
glass vault, the Hotel de Ville on place des Terreaux, the Louis XIV statue on Bellecour,
the Jacobins and Bartholdi fountains. Each one here comes from its OSM feature (footprint,
Wikidata id) and takes its volume from the LiDAR surface model (MNS, 0.5 m), so the
silhouette, the vault, the domes, the courtyards and the statue's size are measured, not
drawn.

  building  : OSM footprint minus the courtyards the LiDAR sees as ground; walls follow the
              true outline (vector, not stair-cased) from the street up to the roof edge;
              the roof is a triangulated LiDAR heightfield joined to the wall tops.
  monument  : same, over the LiDAR footprint of what stands higher than the ground
              (statue on its pedestal, fountain basin and centrepiece), 0.5 m grid.
  square    : data only (polygon, surface, monuments it holds), for the ground and props.

The BD TOPO buildings covered by a landmark are listed in landmarks.json; generate.py leaves
them out (no shell, no facade kit) and import_city.py places the landmark mesh instead.
Greybox with vertex colours like the shells: stone walls, slate and zinc roofs, bronze.

Output (not committed, OSM and LiDAR derived):
  processed/<zone>/landmarks.json             every landmark, zone-wide
  gen/<focus>/landmarks.glb + landmarks.json  with --focus: the landmarks in that area
                                              (one mesh per landmark, origin at the zone origin)

Usage:
  python3 tools/city/landmarks.py --name presqu-ile                       # zone data
  python3 tools/city/landmarks.py --name presqu-ile --focus rue-de-la-republique
  python3 tools/city/landmarks.py --name presqu-ile --focus cell-p0000-m0250 --cell=0,-250,250,0
"""

import argparse
import csv
import glob
import json
import math
import os
import xml.etree.ElementTree as ET

import numpy as np
import shapely
from scipy.spatial import Delaunay
from shapely.geometry import MultiPolygon, Point, Polygon, box
from shapely.ops import unary_union

import citylib as cl
import meshkit
import raster

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

# (key, display name, kind, OSM element, Wikidata, colours). Footprints and volumes come
# from the data; only the choice of monuments and their colours are written here.
LANDMARKS = [
    ("opera", "Opéra de Lyon", "building", ("way", "10061086"), "Q9496",
     {"wall": "#b9ad97", "roof": "#2a2c30"}),          # stone arcades, black glass vault
    ("hotel-de-ville", "Hôtel de Ville", "building", ("way", "1221535523"), "Q516",
     {"wall": "#cdbf9f", "roof": "#596069"}),          # Villebois stone, slate roofs
    ("place-bellecour", "Place Bellecour", "square", ("way", "4161405"), "Q614",
     {"surface": "red_gravel"}),
    ("statue-louis-xiv", "Statue équestre de Louis XIV", "monument", ("node", "236376643"), "Q15726574",
     {"wall": "#c9c2b2", "top": "#4b3a28", "search_m": 9.0}),   # stone pedestal, bronze
    ("place-des-jacobins", "Place des Jacobins", "square", ("relation", "4218607"), "Q10169",
     {"surface": "paving_stones"}),
    ("fontaine-des-jacobins", "Fontaine des Jacobins", "monument", ("way", "25101472"), None,
     {"wall": "#d8d2c4", "top": "#d8d2c4"}),           # white marble
    ("fontaine-bartholdi", "Fontaine Bartholdi", "monument", ("way", "55071957"), "Q10144",
     {"wall": "#8f8a80", "top": "#3f4a45"}),           # granite basin, lead group
]

ROOF_GRID_M = 1.0          # buildings: LiDAR roof sampled every metre
MONUMENT_GRID_M = 0.5      # monuments: native LiDAR resolution
EDGE_STEP_M = 1.5          # wall columns along the outline
EDGE_INSET_M = 0.8         # roof edge height read this far inside the wall
COURT_MAX_M = 3.0          # inside a footprint, LiDAR this close to the ground is a courtyard
COURT_MIN_M2 = 25.0
MONUMENT_MIN_M = 0.4       # a monument part stands at least this high above the ground
COVER_SHARE = 0.5          # a BD TOPO building half covered by a landmark is replaced
FOCUS_PAD_M = 60.0         # --focus: landmarks within this distance of the generated buildings (Opera at the end of the street)
SINK_M = 0.3               # walls start below the street so no gap shows on slopes


# --- OSM -----------------------------------------------------------------------------
def load_osm(raw):
    nodes, ways, rels = {}, {}, {}
    for f in sorted(glob.glob(os.path.join(raw, "osm_*.osm"))):
        for el in ET.parse(f).getroot():
            i = el.get("id")
            if el.tag == "node":
                nodes[i] = (float(el.get("lon")), float(el.get("lat")))
            elif el.tag == "way":
                ways[i] = [n.get("ref") for n in el.findall("nd")]
            elif el.tag == "relation":
                rels[i] = [(m.get("type"), m.get("ref"), m.get("role")) for m in el.findall("member")]
    return nodes, ways, rels


def osm_geometry(osm, ref, origin):
    nodes, ways, rels = osm

    def local(nid):
        x, y = cl.lonlat_to_l93(*nodes[nid])
        return (x - origin[0], y - origin[1])

    kind, i = ref
    if kind == "node":
        return Point(local(i))
    if kind == "way":
        pts = [local(n) for n in ways[i] if n in nodes]
        return Polygon(pts).buffer(0)
    outer = [Polygon([local(n) for n in ways[w] if n in nodes]).buffer(0)
             for t, w, role in rels[i] if t == "way" and role == "outer" and w in ways]
    inner = [Polygon([local(n) for n in ways[w] if n in nodes]).buffer(0)
             for t, w, role in rels[i] if t == "way" and role == "inner" and w in ways]
    return unary_union(outer).difference(unary_union(inner) if inner else Polygon())


def polys(g):
    return [p for p in getattr(g, "geoms", [g]) if isinstance(p, Polygon) and not p.is_empty]


# --- LiDAR footprints ----------------------------------------------------------------
def low_mask(poly, mns, mnt, origin, step, above_m):
    """Union of grid squares inside poly whose surface is within above_m of the ground."""
    x0, y0, x1, y1 = poly.bounds
    xs = np.arange(x0 + step / 2, x1, step)
    ys = np.arange(y0 + step / 2, y1, step)
    gx, gy = np.meshgrid(xs, ys)
    gx, gy = gx.ravel(), gy.ravel()
    inside = shapely.contains_xy(poly, gx, gy)
    gx, gy = gx[inside], gy[inside]
    h = mns.sample(gx + origin[0], gy + origin[1]) - mnt.sample(gx + origin[0], gy + origin[1])
    return gx, gy, h


def squares(gx, gy, step):
    return unary_union([box(x - step / 2, y - step / 2, x + step / 2, y + step / 2) for x, y in zip(gx, gy)])


def building_footprint(poly, mns, mnt, origin):
    gx, gy, h = low_mask(poly, mns, mnt, origin, ROOF_GRID_M, COURT_MAX_M)
    low = h < COURT_MAX_M
    if not low.any():
        return poly, 0.0
    courts = squares(gx[low], gy[low], ROOF_GRID_M).buffer(-1.0).buffer(1.0)   # opening: no slivers
    courts = unary_union([p for p in polys(courts) if p.area >= COURT_MIN_M2])
    if courts.is_empty:
        return poly, 0.0
    out = poly.difference(courts.buffer(0.3)).buffer(0)
    out = unary_union([p for p in polys(out) if p.area >= COURT_MIN_M2])
    return out, round(courts.intersection(poly).area, 1)


def monument_footprint(geom, mns, mnt, origin, search_m):
    area = geom.buffer(search_m) if isinstance(geom, Point) else geom.buffer(0.5)
    gx, gy, h = low_mask(area, mns, mnt, origin, MONUMENT_GRID_M, MONUMENT_MIN_M)
    up = h >= MONUMENT_MIN_M
    if not up.any():
        return Polygon()
    blob = squares(gx[up], gy[up], MONUMENT_GRID_M).buffer(-0.25).buffer(0.25)
    parts = polys(blob)
    if isinstance(geom, Point):   # the part under the node (trees and lamp posts around are left out)
        parts = sorted(parts, key=lambda p: p.distance(geom))[:1]
        return parts[0].simplify(0.15) if parts else Polygon()
    return unary_union([p for p in parts if p.area >= 0.5]).intersection(geom.buffer(0.5)).simplify(0.15)


# --- Mesh ----------------------------------------------------------------------------
def volume_mesh(fp, mns, mnt, origin, origin_alt, grid_m, col_wall, col_top, split_share=None):
    """Walls along every ring of fp from the ground to the LiDAR edge, roof as a heightfield."""
    m = meshkit.Mesh()
    wall, top = meshkit.hex_rgb(col_wall), meshkit.hex_rgb(col_top)
    W = lambda x, y: (np.asarray(x) + origin[0], np.asarray(y) + origin[1])  # noqa: E731
    stats = {"top_m": -1e9, "ground_m": 1e9}
    for p in polys(fp):
        inset = p.buffer(-EDGE_INSET_M)
        x0, y0, x1, y1 = p.bounds
        gx, gy = np.meshgrid(np.arange(x0, x1, grid_m) + grid_m / 2, np.arange(y0, y1, grid_m) + grid_m / 2)
        gx, gy = gx.ravel(), gy.ravel()
        keep = shapely.contains_xy(inset, gx, gy) if not inset.is_empty else np.zeros(len(gx), bool)
        gz = mns.sample(*W(gx[keep], gy[keep])) - origin_alt
        inner_pts = [(x, y, z) for x, y, z in zip(gx[keep], gy[keep], gz) if not math.isnan(z)]
        ring_pts = []
        ground_p = []
        for ring in [p.exterior] + list(p.interiors):
            ring = shapely.geometry.LinearRing(ring.coords)
            n = max(4, int(ring.length / EDGE_STEP_M))
            ds = np.linspace(0, ring.length, n, endpoint=False)
            pts = [ring.interpolate(d) for d in ds]
            xy = np.array([(q.x, q.y) for q in pts])
            # roof edge: highest LiDAR just inside the wall (the cornice), robust to one bad pixel
            edge = []
            for (x, y) in xy:
                cand = []
                for t in (0.5, EDGE_INSET_M, 1.5):
                    q = inside_point(p, x, y, t)
                    if q is not None:
                        cand.append(mns.sample(*W([q[0]], [q[1]]))[0])
                cand = [c for c in cand if not math.isnan(c)]
                edge.append((max(cand) if cand else np.nan) - origin_alt)
            edge = np.array(edge)
            if np.isnan(edge).all():
                continue
            edge = np.where(np.isnan(edge), np.nanmedian(edge), edge)
            edge = np.array([np.median(np.take(edge, range(k - 1, k + 2), mode="wrap")) for k in range(len(edge))])
            ground = mnt.sample(*W(xy[:, 0], xy[:, 1])) - origin_alt
            ground = np.where(np.isnan(ground), np.nanmin(ground) if not np.isnan(ground).all() else 0.0, ground)
            gmin = float(ground.min()) - SINK_M
            ground_p.append(gmin)
            for k in range(len(xy)):
                a, b = xy[k], xy[(k + 1) % len(xy)]
                za, zb = edge[k], edge[(k + 1) % len(xy)]
                if za <= gmin + 0.2 or zb <= gmin + 0.2:
                    continue
                q = [(a[0], a[1], gmin), (b[0], b[1], gmin), (b[0], b[1], zb), (a[0], a[1], za)]
                m.quad(*q, wall)    # double-sided material: winding does not matter for the look
            ring_pts += [(x, y, z) for (x, y), z in zip(xy, edge)]
        pts = ring_pts + inner_pts
        if len(pts) < 3:
            continue
        arr = np.array(pts)
        if split_share is not None:   # monuments: bronze above split_share of the height
            g0 = min(ground_p) if ground_p else float(np.nanmin(arr[:, 2]))
            z_split = g0 + split_share * (arr[:, 2].max() - g0)
        tri = Delaunay(arr[:, :2])
        for s in tri.simplices:
            c = arr[s, :2].mean(0)
            if not p.contains(Point(c)):
                continue
            col = top
            if split_share is not None and arr[s, 2].max() < z_split:
                col = wall
            m.tri(*(arr[k] for k in s), col)
        stats["top_m"] = max(stats["top_m"], float(arr[:, 2].max()))
        stats["ground_m"] = min([stats["ground_m"]] + ground_p)
    stats["height_m"] = round(stats["top_m"] - stats["ground_m"] - SINK_M, 1) if m.tris else 0.0
    return m, stats


def inside_point(p, x, y, t):
    """The point t metres from (x, y) towards the inside of polygon p, along the local normal."""
    b = p.boundary
    d = b.project(Point(x, y))
    a, c = b.interpolate(max(0.0, d - 0.5)), b.interpolate(min(b.length, d + 0.5))
    tx, ty = c.x - a.x, c.y - a.y
    L = math.hypot(tx, ty)
    if L < 1e-6:
        return None
    for sgn in (1, -1):
        q = (x - sgn * ty / L * t, y + sgn * tx / L * t)
        if p.contains(Point(q)):
            return q
    return None


# --- Main ----------------------------------------------------------------------------
def load_bdtopo(proc):
    out = []
    with open(os.path.join(proc, "buildings.csv"), encoding="utf-8") as f:
        for r in csv.DictReader(f):
            ring = [tuple(float(v) for v in pt.split()) for pt in r["ring_xy"].split(";") if pt]
            if len(ring) >= 3:
                out.append((r["id"], Polygon(ring).buffer(0)))
    return out


def build_zone(zone):
    raw = os.path.join(REPO, "assets", "geo", "raw", zone)
    proc = os.path.join(REPO, "assets", "geo", "processed", zone)
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    origin, origin_alt = man["origin_lambert93_m"], man["origin_altitude_m"]
    osm = load_osm(raw)
    mnt, mns = raster.Raster.load(raw, "mnt"), raster.Raster.load(raw, "mns")
    bd = load_bdtopo(proc)
    out, meshes = [], {}
    for key, name, kind, ref, qid, style in LANDMARKS:
        try:
            g = osm_geometry(osm, ref, origin)
        except KeyError:
            out.append({"key": key, "name": name, "error": "OSM element %s/%s not in the zone" % ref})
            continue
        rec = {"key": key, "name": name, "kind": kind, "osm": "%s/%s" % ref, "wikidata": qid}
        if kind == "square":
            rec.update(surface=style["surface"], area_m2=round(g.area), polygon_xy=ring_list(g))
            out.append(rec)
            continue
        if kind == "building":
            fp, court = building_footprint(g, mns, mnt, origin)
            mesh, st = volume_mesh(fp, mns, mnt, origin, origin_alt, ROOF_GRID_M, style["wall"], style["roof"])
            covered = [i for i, b in bd if b.area > 0 and b.intersection(g).area >= COVER_SHARE * b.area]
            rec.update(courtyards_m2=court, replaces_bdtopo=covered)
        else:
            fp = monument_footprint(g, mns, mnt, origin, style.get("search_m", 6.0))
            mesh, st = volume_mesh(fp, mns, mnt, origin, origin_alt, MONUMENT_GRID_M, style["wall"], style["top"],
                                   split_share=0.45)
            rec["replaces_bdtopo"] = []
        c = fp.centroid if not fp.is_empty else g.centroid
        rec.update(footprint_m2=round(fp.area, 1), height_m=st["height_m"], triangles=mesh.tris,
                   centre_xy=[round(c.x, 1), round(c.y, 1)], polygon_xy=ring_list(fp),
                   mesh="SM_Landmark_" + "".join(w.capitalize() for w in key.split("-")))
        if mesh.tris == 0:
            rec["error"] = "no LiDAR volume (outside the LiDAR tiles?)"
        else:
            meshes[key] = mesh
        out.append(rec)
    squares_ = [r for r in out if r.get("kind") == "square"]
    for r in out:   # which square holds which monument
        if r.get("kind") == "monument" and "centre_xy" in r:
            for s in squares_:
                if Polygon(s["polygon_xy"][0]).buffer(5).contains(Point(r["centre_xy"])):
                    s.setdefault("monuments", []).append(r["key"])
    meta = {"zone": zone, "sources": "OpenStreetMap (ODbL) footprints and Wikidata ids; LiDAR HD IGN MNS/MNT 0.5 m",
            "landmarks": out, "replaced_bdtopo": sorted({i for r in out for i in r.get("replaces_bdtopo", [])})}
    with open(os.path.join(proc, "landmarks.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1, ensure_ascii=False)
    return meta, meshes


def ring_list(g):
    return [[[round(x, 2), round(y, 2)] for x, y in p.exterior.coords] for p in polys(g)]


def write_focus(zone, focus, meta, meshes, cell=None):
    proc = os.path.join(REPO, "assets", "geo", "processed", zone)
    gen = os.path.join(proc, "gen", focus)
    if cell:
        area = box(*[float(v) for v in cell.split(",")])
        pick = lambda c: area.contains(Point(c))   # noqa: E731  one cell owns each landmark
    else:
        with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
            pts = [(float(r["cx"]), float(r["cy"])) for r in csv.DictReader(f)]
        area = MultiPolygon([Point(p).buffer(1.0) for p in pts]).convex_hull.buffer(FOCUS_PAD_M) \
            if pts else Polygon()
        pick = lambda c: area.contains(Point(c))   # noqa: E731
    chosen = [r for r in meta["landmarks"] if r.get("key") in meshes and pick(r["centre_xy"])]
    items = [(r["mesh"], meshes[r["key"]], (0.0, 0.0, 0.0)) for r in chosen]
    for name in ("landmarks.glb", "landmarks.json"):
        if os.path.exists(os.path.join(gen, name)):
            os.remove(os.path.join(gen, name))
    if items:
        meshkit.write_glb(os.path.join(gen, "landmarks.glb"), items)
        with open(os.path.join(gen, "landmarks.json"), "w", encoding="utf-8") as f:
            json.dump({"zone": zone, "focus": focus, "nodes": [
                {"name": r["mesh"], "key": r["key"], "label": r["name"], "kind": r["kind"], "collision": True}
                for r in chosen]}, f, indent=1, ensure_ascii=False)
    return [r["key"] for r in chosen]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", help="gen/<focus> to write landmarks.glb into (after generate.py)")
    ap.add_argument("--cell", help="cell mode: X0,Y0,X1,Y1; the cell holding a landmark's centre owns it")
    a = ap.parse_args()
    meta, meshes = build_zone(a.name)
    summary = [{k: r.get(k) for k in ("key", "kind", "footprint_m2", "height_m", "triangles", "error")
                if r.get(k) is not None} for r in meta["landmarks"]]
    res = {"landmarks": summary, "replaced_bdtopo": len(meta["replaced_bdtopo"])}
    if a.focus:
        res["placed_in_" + a.focus] = write_focus(a.name, a.focus, meta, meshes, a.cell)
    print(json.dumps(res, indent=1, ensure_ascii=False))


if __name__ == "__main__":
    main()
