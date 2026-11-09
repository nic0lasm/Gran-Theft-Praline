#!/usr/bin/env python3
"""Extract a greybox city block from IGN BD TOPO for a bounding box.

Downloads buildings (footprint, ground altitude, height) and road segments
from the IGN Geoplateforme WFS (BD TOPO, Etalab Open Licence 2.0), then writes:

  assets/geo/raw/<name>/batiment.geojson            raw download (not versioned)
  assets/geo/raw/<name>/troncon_de_route.geojson    raw download (not versioned)
  assets/geo/processed/<name>/<name>.glb            extruded buildings, roads, ground
  assets/geo/processed/<name>/<name>.meta.json      origin, axes, named street polylines

Coordinates: Lambert-93 (EPSG:2154) metres, recentred on the bbox centre.
glTF axes follow the spec (right-handed, +Y up): +X = east, +Y = up,
+Z = south. Street polylines in the meta file are given both in metres
(east, north) and in Unreal units (cm) assuming the importer maps glTF
(x, y, z) to Unreal (x, z, y): Unreal X = east, Y = south, Z = up.

Requires: numpy, mapbox-earcut.  Usage:
  python3 extract_bbox.py --name rue-republique \
      --bbox 45.7605,4.8335,45.7640,4.8380 --street "R DE LA REPUBLIQUE"
"""

import argparse
import json
import math
import os
import statistics
import struct
import sys
import urllib.parse
import urllib.request

import mapbox_earcut
import numpy as np

WFS = "https://data.geopf.fr/wfs/ows"
LAYERS = {"batiment": "BDTOPO_V3:batiment", "troncon_de_route": "BDTOPO_V3:troncon_de_route"}
DEFAULT_HEIGHT_M = 15.0
DEFAULT_ROAD_WIDTH_M = 6.0
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def fetch_layer(layer, bbox):
    lat0, lon0, lat1, lon1 = bbox
    params = {
        "SERVICE": "WFS", "VERSION": "2.0.0", "REQUEST": "GetFeature",
        "TYPENAMES": layer, "OUTPUTFORMAT": "application/json",
        "SRSNAME": "EPSG:2154",
        "BBOX": f"{lat0},{lon0},{lat1},{lon1},urn:ogc:def:crs:EPSG::4326",
        "COUNT": "5000",
    }
    url = WFS + "?" + urllib.parse.urlencode(params)
    with urllib.request.urlopen(url, timeout=120) as resp:
        return json.load(resp)


def polygons_of(geom):
    if geom["type"] == "Polygon":
        return [geom["coordinates"]]
    if geom["type"] == "MultiPolygon":
        return geom["coordinates"]
    return []


def lines_of(geom):
    if geom["type"] == "LineString":
        return [geom["coordinates"]]
    if geom["type"] == "MultiLineString":
        return geom["coordinates"]
    return []


class MeshBuilder:
    """Flat-shaded triangle soup with normals and UVs (metres)."""

    def __init__(self):
        self.pos, self.nrm, self.uv, self.idx = [], [], [], []

    def _add(self, p, n, uv):
        self.pos.append(p)
        self.nrm.append(n)
        self.uv.append(uv)
        return len(self.pos) - 1

    def tri(self, a, b, c, uva, uvb, uvc):
        n = np.cross(np.subtract(b, a), np.subtract(c, a))
        length = np.linalg.norm(n)
        if length < 1e-9:
            return
        n = tuple(n / length)
        self.idx += [self._add(a, n, uva), self._add(b, n, uvb), self._add(c, n, uvc)]

    def quad(self, a, b, c, d, uvs):
        self.tri(a, b, c, uvs[0], uvs[1], uvs[2])
        self.tri(a, c, d, uvs[0], uvs[2], uvs[3])


def ring_area(ring):
    return 0.5 * sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]))


def to_gltf(e, n, up):
    """(east, north, up) metres -> glTF (x=east, y=up, z=south)."""
    return (e, up, -n)


def extrude_building(mb, rings, base, top):
    """rings: list of [(e, n)] in metres, first outer, others holes."""
    clean = []
    for i, ring in enumerate(rings):
        r = [tuple(p[:2]) for p in ring]
        if len(r) > 1 and r[0] == r[-1]:
            r = r[:-1]
        if len(r) < 3:
            continue
        ccw = ring_area(r) > 0
        # outer ring counter-clockwise, holes clockwise (seen from above)
        if (i == 0) != ccw:
            r = r[::-1]
        clean.append(r)
    if not clean:
        return
    # walls: outward normals for CCW outer rings
    for r in clean:
        u = 0.0
        for (e0, n0), (e1, n1) in zip(r, r[1:] + r[:1]):
            seg = math.hypot(e1 - e0, n1 - n0)
            a, b = to_gltf(e0, n0, base), to_gltf(e1, n1, base)
            c, d = to_gltf(e1, n1, top), to_gltf(e0, n0, top)
            h = top - base
            # glTF is right-handed: CCW front faces seen from outside
            mb.quad(a, b, c, d, [(u, h), (u + seg, h), (u + seg, 0.0), (u, 0.0)])
            u += seg
    # roof: earcut over outer ring + holes
    flat = [p for r in clean for p in r]
    ends, total = [], 0
    for r in clean:
        total += len(r)
        ends.append(total)
    tris = mapbox_earcut.triangulate_float64(np.array(flat, dtype=np.float64), np.array(ends, dtype=np.uint32))
    for i in range(0, len(tris), 3):
        pa, pb, pc = (flat[tris[i + k]] for k in range(3))
        # earcut keeps input winding; seen from above CCW => normal +Y in glTF
        a, b, c = (to_gltf(p[0], p[1], top) for p in (pa, pb, pc))
        if np.cross(np.subtract(b, a), np.subtract(c, a))[1] < 0:
            b, c, pb, pc = c, b, pc, pb
        mb.tri(a, b, c, pa, pb, pc)


def road_ribbon(mb, line, width, z, extent):
    half = width / 2.0
    for (e0, n0, *_), (e1, n1, *_) in zip(line, line[1:]):
        if max(abs(e0), abs(e1)) > extent[0] or max(abs(n0), abs(n1)) > extent[1]:
            continue
        de, dn = e1 - e0, n1 - n0
        length = math.hypot(de, dn)
        if length < 1e-6:
            continue
        oe, on = -dn / length * half, de / length * half
        a = to_gltf(e0 - oe, n0 - on, z)
        b = to_gltf(e1 - oe, n1 - on, z)
        c = to_gltf(e1 + oe, n1 + on, z)
        d = to_gltf(e0 + oe, n0 + on, z)
        mb.quad(a, b, c, d, [(0, 0), (length, 0), (length, width), (0, width)])
        if np.cross(np.subtract(b, a), np.subtract(c, a))[1] < 0:
            # keep road faces pointing up
            for _ in range(6):
                mb.idx.pop()
            mb.quad(a, d, c, b, [(0, 0), (0, width), (length, width), (length, 0)])


def chain(segments, tol=0.5):
    """Join polylines that share endpoints; return the longest chain."""
    lines = [list(map(tuple, seg)) for seg in segments if len(seg) > 1]
    chains = []
    while lines:
        cur = lines.pop(0)
        grown = True
        while grown:
            grown = False
            for i, ln in enumerate(lines):
                for cand in (ln, ln[::-1]):
                    if math.dist(cur[-1], cand[0]) < tol:
                        cur = cur + cand[1:]
                    elif math.dist(cur[0], cand[-1]) < tol:
                        cur = cand[:-1] + cur
                    else:
                        continue
                    lines.pop(i)
                    grown = True
                    break
                if grown:
                    break
        chains.append(cur)

    def length(c):
        return sum(math.dist(a, b) for a, b in zip(c, c[1:]))

    return max(chains, key=length) if chains else []


def write_glb(path, meshes):
    """meshes: list of (name, MeshBuilder). One node and one mesh each."""
    bin_parts, views, accessors, gl_meshes, nodes = [], [], [], [], []
    offset = 0

    def add_view(data, target):
        nonlocal offset
        pad = (-len(data)) % 4
        bin_parts.append(data + b"\0" * pad)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target})
        offset += len(data) + pad
        return len(views) - 1

    for name, mb in meshes:
        if not mb.idx:
            continue
        pos = np.array(mb.pos, dtype=np.float32)
        nrm = np.array(mb.nrm, dtype=np.float32)
        uv = np.array(mb.uv, dtype=np.float32)
        idx = np.array(mb.idx, dtype=np.uint32)
        base = len(accessors)
        accessors.append({"bufferView": add_view(pos.tobytes(), 34962), "componentType": 5126,
                          "count": len(pos), "type": "VEC3",
                          "min": pos.min(axis=0).tolist(), "max": pos.max(axis=0).tolist()})
        accessors.append({"bufferView": add_view(nrm.tobytes(), 34962), "componentType": 5126,
                          "count": len(nrm), "type": "VEC3"})
        accessors.append({"bufferView": add_view(uv.tobytes(), 34962), "componentType": 5126,
                          "count": len(uv), "type": "VEC2"})
        accessors.append({"bufferView": add_view(idx.tobytes(), 34963), "componentType": 5125,
                          "count": len(idx), "type": "SCALAR"})
        gl_meshes.append({"name": name, "primitives": [{
            "attributes": {"POSITION": base, "NORMAL": base + 1, "TEXCOORD_0": base + 2},
            "indices": base + 3, "material": 0}]})
        nodes.append({"name": name, "mesh": len(gl_meshes) - 1})

    gltf = {
        "asset": {"version": "2.0", "generator": "gtal geo-pipeline extract_bbox"},
        "scene": 0, "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes, "meshes": gl_meshes, "accessors": accessors, "bufferViews": views,
        "materials": [{"name": "M_Greybox", "pbrMetallicRoughness": {
            "baseColorFactor": [0.35, 0.35, 0.35, 1.0], "metallicFactor": 0.0, "roughnessFactor": 0.8}}],
        "buffers": [{"byteLength": offset}],
    }
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * ((-len(js)) % 4)
    blob = b"".join(bin_parts)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob)))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(blob), 0x004E4942) + blob)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--bbox", required=True, help="lat_min,lon_min,lat_max,lon_max (WGS84)")
    ap.add_argument("--street", action="append", default=[], help="BAN street name to export as a polyline")
    ap.add_argument("--offline", action="store_true", help="reuse raw files instead of downloading")
    args = ap.parse_args()

    bbox = [float(v) for v in args.bbox.split(",")]
    raw_dir = os.path.join(REPO, "assets", "geo", "raw", args.name)
    out_dir = os.path.join(REPO, "assets", "geo", "processed", args.name)
    os.makedirs(raw_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    data = {}
    for key, layer in LAYERS.items():
        raw_path = os.path.join(raw_dir, key + ".geojson")
        if args.offline:
            with open(raw_path) as f:
                data[key] = json.load(f)
        else:
            data[key] = fetch_layer(layer, bbox)
            with open(raw_path, "w") as f:
                json.dump(data[key], f)
        print(f"{key}: {len(data[key]['features'])} features", file=sys.stderr)

    # origin: centre of all building coordinates, reference altitude: median ground
    xs, ys, grounds = [], [], []
    for feat in data["batiment"]["features"]:
        for poly in polygons_of(feat["geometry"]):
            for x, y, *_ in poly[0]:
                xs.append(x)
                ys.append(y)
        g = feat["properties"].get("altitude_minimale_sol")
        if g is not None:
            grounds.append(g)
    ox, oy = (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0
    ref_alt = statistics.median(grounds) if grounds else 0.0

    def local(pt):
        return (pt[0] - ox, pt[1] - oy)

    meshes, heights = [], []
    for feat in data["batiment"]["features"]:
        p = feat["properties"]
        ground = p.get("altitude_minimale_sol")
        base = (ground - ref_alt) if ground is not None else 0.0
        height = p.get("hauteur")
        if not height and p.get("altitude_maximale_toit") and ground is not None:
            height = p["altitude_maximale_toit"] - ground
        height = float(height or DEFAULT_HEIGHT_M)
        heights.append(height)
        mb = MeshBuilder()
        for poly in polygons_of(feat["geometry"]):
            extrude_building(mb, [[local(pt) for pt in ring] for ring in poly], base - 0.5, base + height)
        meshes.append(("B_" + str(p.get("cleabs", len(meshes))), mb))

    half_e = (max(xs) - min(xs)) / 2.0 + 20.0
    half_n = (max(ys) - min(ys)) / 2.0 + 20.0
    roads = MeshBuilder()
    streets = {name: [] for name in args.street}
    for feat in data["troncon_de_route"]["features"]:
        p = feat["properties"]
        if p.get("position_par_rapport_au_sol") not in (None, "0", 0):
            continue
        width = float(p.get("largeur_de_chaussee") or DEFAULT_ROAD_WIDTH_M)
        names = {p.get("nom_voie_ban_gauche"), p.get("nom_collaboratif_gauche")}
        name = next((n for n in args.street if n in names), None)
        for line in lines_of(feat["geometry"]):
            pts = [local(pt) for pt in line]
            road_ribbon(roads, pts, width, 0.02, (half_e, half_n))
            if name:
                streets[name].append({"width_m": width, "points_m": [[round(e, 2), round(n, 2)] for e, n in pts]})
    meshes.append(("Roads", roads))

    ground = MeshBuilder()
    corners = [to_gltf(-half_e, -half_n, 0.0), to_gltf(half_e, -half_n, 0.0),
               to_gltf(half_e, half_n, 0.0), to_gltf(-half_e, half_n, 0.0)]
    ground.quad(*corners, [(-half_e, -half_n), (half_e, -half_n), (half_e, half_n), (-half_e, half_n)])
    if ground.nrm[0][1] < 0:
        ground = MeshBuilder()
        ground.quad(corners[0], corners[3], corners[2], corners[1],
                    [(-half_e, -half_n), (-half_e, half_n), (half_e, half_n), (half_e, -half_n)])
    meshes.append(("Ground", ground))

    glb_path = os.path.join(out_dir, args.name + ".glb")
    write_glb(glb_path, meshes)

    def to_ue(e, n):
        return [round(e * 100.0, 1), round(-n * 100.0, 1)]

    meta = {
        "name": args.name,
        "source": "IGN BD TOPO V3 via data.geopf.fr WFS (Licence Ouverte Etalab 2.0)",
        "bbox_wgs84": bbox,
        "origin_lambert93_m": [round(ox, 2), round(oy, 2)],
        "reference_altitude_m": ref_alt,
        "latitude_deg": (bbox[0] + bbox[2]) / 2.0,
        "longitude_deg": (bbox[1] + bbox[3]) / 2.0,
        "axes": {"gltf": "+X east, +Y up, +Z south (metres)",
                 "unreal_assumed": "+X east, +Y south, +Z up (cm)"},
        "buildings": len(meshes) - 2,
        "height_m": {"min": min(heights), "median": statistics.median(heights), "max": max(heights)},
        "streets": {},

    }
    for name, segs in streets.items():
        axis = chain([seg["points_m"] for seg in segs])
        meta["streets"][name] = {
            "axis_length_m": round(sum(math.dist(a, b) for a, b in zip(axis, axis[1:])), 1),
            "axis_m": [list(pt) for pt in axis],
            "axis_ue_cm": [to_ue(e, n) for e, n in axis],
            "segments": len(segs),
        }
    with open(os.path.join(out_dir, args.name + ".meta.json"), "w") as f:
        json.dump(meta, f, indent=1, ensure_ascii=False)
    print(f"wrote {glb_path} ({os.path.getsize(glb_path) // 1024} KiB), {meta['buildings']} buildings", file=sys.stderr)


if __name__ == "__main__":
    main()
