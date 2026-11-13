#!/usr/bin/env python3
"""Far decor: the real Lyon up to the horizon, from the Metropole's textured 3D model (2018).

The street map (generate.py) and its grey backdrop (backdrop.py) stop at a few hundred metres;
from the République slice the wide views must reach Fourvière, the Croix-Rousse slopes, the
Saône and Rhône quays and the roofs of the Presqu'île. This script turns the Metropole de Lyon's
"Maquettes 3D texturées 2018" (CityGML LOD2, one photo texture per building, Licence Ouverte v2,
credit "Métropole de Lyon") and an IGN terrain with its orthophoto (Licence Ouverte Etalab 2.0,
credit "IGN") into a light horizon decor:

- every CityGML building, bridge and remarkable building within --radius of the street axis
  that the street map does not generate (its centroid is not in a generated BD TOPO footprint
  or a landmark; the bridge riverside.py models is left to it), merged per --tile square and
  per texture atlas: one mesh, one material, one draw each;
- the building photos at three sizes by distance to the axis (--close-tex within --close-m,
  --near-tex within --near-m, --far-tex beyond), packed in atlases of --atlas px cut to the
  power-of-two size they use (JPEG inside the glb);
- a terrain around the zone with the orthophoto (1 m/px) per 1 km tile, sampled every 10, 20 or
  40 m by distance to the axis (--terrain-steps) with skirts on the tile edges, a hole over the
  zone's own LiDAR terrain (terrain.py) and 0.5 m lower, so the seam never shows.

import_city.py places it with no collision and no distance field; backdrop.py then
leaves out the BD TOPO buildings the far decor covers (far_decor.json "covered_bdtopo").
Nothing here goes in Git: the zips, the cache, the glb and the textures stay out (.gitignore).

Steps (each one cached, rerun freely):
  fetch   download the arrondissement zips (--arr, default the seven around the Presqu'ile) into
          assets/geo/raw/<name>/citygml/, and the far terrain and orthophoto into .../far/
  prep    each zip -> cache/<stem>.npz (local metres) + cache/<stem>/<sheet>.jpg at --prep-tex px;
          the zip can be deleted afterwards (2-3 GB each, the cache about 60-110 MB)
  build   -> gen/<focus>/far_decor.glb + far_decor.json

Usage:
  python3 tools/city/far_decor.py fetch --name presqu-ile
  python3 tools/city/far_decor.py prep  --name presqu-ile
  python3 tools/city/far_decor.py build --name presqu-ile --focus rue-de-la-republique

Needs numpy and pillow (tools/city/requirements.txt); no pyproj, no lxml: the CC46 -> Lambert-93
conversion and the XML parsing are done here.
"""

import argparse
import csv
import io
import json
import math
import os
import posixpath
import struct
import sys
import time
import zipfile
import xml.etree.ElementTree as ET

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import citylib as cl  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
CITYGML_URL = "https://download.data.grandlyon.com/files/grandlyon/imagerie/2018/maquette/LYON_{arr}_2018.zip"
WMS_R = "https://data.geopf.fr/wms-r/wms"
MNT_LAYER = "ELEVATION.ELEVATIONGRIDCOVERAGE.HIGHRES"      # RGE ALTI 1 m, served at any resolution
ORTHO_LAYER = "HR.ORTHOIMAGERY.ORTHOPHOTOS"
# Terreaux and the slopes, Presqu'ile, Part-Dieu and the Rhone's left bank, plateau, Fourviere and
# Vieux Lyon, Brotteaux (opposite pont Lafayette), Guillotiere (opposite Bellecour)
DEFAULT_ARR = ["1ER", "2EME", "3EME", "4EME", "5EME", "6EME", "7EME"]
SOURCE = {"buildings": "Métropole de Lyon, Maquettes 3D texturées 2018 (CityGML LOD2), Licence Ouverte v2",
          "terrain": "IGN, RGE ALTI and BD ORTHO (Géoplateforme), Licence Ouverte Etalab 2.0"}

NS_GML = "{http://www.opengis.net/gml}"
# CityGML 2.0 in the BATI files, 1.0 in most *_REMARQUABLE ones (Fourviere basilica, Saint-Jean...):
# objects and textures are matched on their local names, whatever the version of their namespace.
OBJECT_TAGS = {"Building", "Bridge", "CityFurniture", "GenericCityObject"}

GREY = (150, 146, 140)       # untextured polygons (rare): a patch of this colour in every atlas


# --- CC46 (RGF93, LCC 2SP, GRS80) -> Lambert-93 -----------------------------------------------
def _lcc(lat1, lat2, lat0, lon0, x0, y0):
    a, f = 6378137.0, 1 / 298.257222101
    e = math.sqrt(2 * f - f * f)
    m = lambda p: math.cos(p) / math.sqrt(1 - (e * math.sin(p)) ** 2)  # noqa: E731
    t = lambda p: math.tan(math.pi / 4 - p / 2) / ((1 - e * math.sin(p)) / (1 + e * math.sin(p))) ** (e / 2)  # noqa: E731
    p1, p2, p0 = math.radians(lat1), math.radians(lat2), math.radians(lat0)
    n = (math.log(m(p1)) - math.log(m(p2))) / (math.log(t(p1)) - math.log(t(p2)))
    g = m(p1) / (n * t(p1) ** n)
    return {"a": a, "e": e, "n": n, "g": g, "rho0": a * g * t(p0) ** n, "lon0": math.radians(lon0), "x0": x0, "y0": y0}


_CC46 = _lcc(45.25, 46.75, 46.0, 3.0, 1700000.0, 5200000.0)
_L93 = _lcc(44.0, 49.0, 46.5, 3.0, 700000.0, 6600000.0)


def _inverse(p, x, y):
    dx, dy = x - p["x0"], p["rho0"] - (y - p["y0"])
    rho = np.sign(p["n"]) * np.hypot(dx, dy)
    theta = np.arctan2(dx, dy)
    t = (rho / (p["a"] * p["g"])) ** (1 / p["n"])
    lon = theta / p["n"] + p["lon0"]
    lat = np.pi / 2 - 2 * np.arctan(t)
    for _ in range(6):
        es = p["e"] * np.sin(lat)
        lat = np.pi / 2 - 2 * np.arctan(t * ((1 - es) / (1 + es)) ** (p["e"] / 2))
    return lon, lat


def _forward(p, lon, lat):
    es = p["e"] * np.sin(lat)
    t = np.tan(np.pi / 4 - lat / 2) / ((1 - es) / (1 + es)) ** (p["e"] / 2)
    rho = p["a"] * p["g"] * t ** p["n"]
    th = p["n"] * (lon - p["lon0"])
    return p["x0"] + rho * np.sin(th), p["y0"] + p["rho0"] - rho * np.cos(th)


def cc46_to_l93(x, y):
    lon, lat = _inverse(_CC46, np.asarray(x, np.float64), np.asarray(y, np.float64))
    return _forward(_L93, lon, lat)


# --- small geometry helpers ------------------------------------------------------------------
def triangulate(p2):
    """Ear clipping of a simple polygon (n x 2), any winding. Returns a list of index triples."""
    n = len(p2)
    if n < 3:
        return []
    if n == 3:
        return [(0, 1, 2)]
    area = 0.5 * np.sum(p2[:, 0] * np.roll(p2[:, 1], -1) - np.roll(p2[:, 0], -1) * p2[:, 1])
    idx = list(range(n)) if area > 0 else list(range(n))[::-1]
    out, guard = [], 0
    while len(idx) > 3 and guard < 4 * n:
        guard += 1
        m = len(idx)
        for k in range(m):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % m]
            a, b, c = p2[i0], p2[i1], p2[i2]
            cross = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
            if cross <= 1e-12:
                continue
            ok = True
            for j in idx:
                if j in (i0, i1, i2):
                    continue
                q = p2[j]
                d1 = (b[0] - a[0]) * (q[1] - a[1]) - (b[1] - a[1]) * (q[0] - a[0])
                d2 = (c[0] - b[0]) * (q[1] - b[1]) - (c[1] - b[1]) * (q[0] - b[0])
                d3 = (a[0] - c[0]) * (q[1] - c[1]) - (a[1] - c[1]) * (q[0] - c[0])
                if d1 >= 0 and d2 >= 0 and d3 >= 0:
                    ok = False
                    break
            if ok:
                out.append((i0, i1, i2))
                idx.pop(k)
                break
        else:
            break
    if len(idx) == 3:
        out.append(tuple(idx))
    elif len(idx) > 3:                       # degenerate leftovers: a fan keeps the surface closed
        out += [(idx[0], idx[k], idx[k + 1]) for k in range(1, len(idx) - 1)]
    return out


# --- fetch -----------------------------------------------------------------------------------
def raw_dirs(geo, name):
    raw = os.path.join(geo, "raw", name)
    return raw, os.path.join(raw, "citygml"), os.path.join(raw, "far")


def zone_envelope(proc):
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    return man, man["origin_lambert93_m"], man["origin_altitude_m"]


def far_envelope(origin, radius):
    ox, oy = origin
    return (math.floor(ox - radius), math.floor(oy - radius), math.ceil(ox + radius), math.ceil(oy + radius))


def wms(layer, bbox, w, h, fmt):
    url = WMS_R + "?" + cl.qs({"SERVICE": "WMS", "VERSION": "1.3.0", "REQUEST": "GetMap", "LAYERS": layer,
                                "STYLES": "", "CRS": "EPSG:2154", "BBOX": ",".join(str(v) for v in bbox),
                                "WIDTH": w, "HEIGHT": h, "FORMAT": fmt})
    return cl.http_get(url)


def download(url, path, chunk=1 << 22):
    """Stream to path.part, resume a partial file, rename when complete."""
    import urllib.request
    part = path + ".part"
    for attempt in range(6):
        have = os.path.getsize(part) if os.path.exists(part) else 0
        req = urllib.request.Request(url, headers={"User-Agent": cl.USER_AGENT, "Range": f"bytes={have}-"})
        try:
            with urllib.request.urlopen(req, timeout=300) as resp, open(part, "ab" if have else "wb") as out:
                if have and resp.status != 206:
                    out.truncate(0)
                expected, got = int(resp.headers.get("Content-Length") or -1), 0
                while True:
                    b = resp.read(chunk)
                    if not b:
                        break
                    out.write(b)
                    got += len(b)
                if 0 <= expected != got:          # the server closed early without an error
                    raise IOError(f"short read: {got} of {expected} bytes")
            os.replace(part, path)
            return
        except Exception as exc:  # noqa: BLE001
            print(f"download interrupted ({exc}), retry {attempt + 1}")
            time.sleep(5 * (attempt + 1))
    raise SystemExit("download failed: " + url)


def cmd_fetch(a):
    raw, cg_dir, far_dir = raw_dirs(a.geo, a.name)
    os.makedirs(cg_dir, exist_ok=True)
    os.makedirs(far_dir, exist_ok=True)
    for arr in a.arr:
        path = os.path.join(cg_dir, f"LYON_{arr}_2018.zip")
        if os.path.exists(path) or os.path.exists(os.path.join(cg_dir, "cache", f"LYON_{arr}_2018.npz")):
            print(f"{arr}: already here")
            continue
        print(f"{arr}: downloading {CITYGML_URL.format(arr=arr)} (about 3 GB)")
        download(CITYGML_URL.format(arr=arr), path)
    proc = os.path.join(a.geo, "processed", a.name)
    _, origin, _ = zone_envelope(proc)
    env = far_envelope(origin, a.radius + 200)
    res = a.terrain_res
    w, h = int((env[2] - env[0]) / res), int((env[3] - env[1]) / res)
    mnt_path = os.path.join(far_dir, "mnt.f32")
    if not os.path.exists(mnt_path):
        data = wms(MNT_LAYER, env, w, h, "image/x-bil;bits=32")
        if len(data) != w * h * 4:
            raise SystemExit(f"far terrain: unexpected size {len(data)}")
        open(mnt_path, "wb").write(data)
    ortho_px = 1000                                   # one request per km: 1 m/px
    tiles = []
    for x0 in range(int(env[0]), int(env[2]), 1000):
        for y0 in range(int(env[1]), int(env[3]), 1000):
            p = os.path.join(far_dir, f"ortho_{x0}_{y0}.jpg")
            if not os.path.exists(p):
                open(p, "wb").write(wms(ORTHO_LAYER, (x0, y0, x0 + 1000, y0 + 1000), ortho_px, ortho_px, "image/jpeg"))
            tiles.append(os.path.basename(p))
    meta = {"envelope_l93": env, "res_m": res, "width": w, "height": h, "row0": "north",
            "mnt": "mnt.f32", "ortho_tiles_1km": tiles, "source": SOURCE["terrain"]}
    with open(os.path.join(far_dir, "far.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
    print(f"far terrain {w}x{h} at {res} m, {len(tiles)} ortho tiles")


# --- prep ------------------------------------------------------------------------------------
def _local(tag):
    return tag.rsplit("}", 1)[-1]


def parse_citygml(gml_stream):
    """Yield ('tex', ring_id, image, uv) then ('bldg', id, [(ring_id, coords n x 3)], kind) for every
    building and bridge (the arrondissement zips also hold PONT and *_REMARQUABLE files)."""
    for ev, el in ET.iterparse(gml_stream, events=("end",)):
        tag = _local(el.tag)
        if tag == "ParameterizedTexture":
            img = next((c.text for c in el if _local(c.tag) == "imageURI"), None)
            for tc in el.iter():
                if _local(tc.tag) == "textureCoordinates":
                    yield "tex", tc.get("ring", "").lstrip("#"), img, tc.text
            el.clear()
        elif tag in OBJECT_TAGS:
            rings = []
            for lr in el.iter(NS_GML + "LinearRing"):
                pl = lr.findtext(NS_GML + "posList")
                if pl:
                    rings.append((lr.get(NS_GML + "id"), pl))
            yield "bldg", el.get(NS_GML + "id"), rings, tag
            el.clear()


class Source:
    """A CityGML zip, or the folder it was extracted to (same member names)."""

    def __init__(self, path):
        self.zip = zipfile.ZipFile(path) if path.endswith(".zip") else None
        self.root = path

    def namelist(self):
        if self.zip:
            return self.zip.namelist()
        out = []
        for d, _, files in os.walk(self.root):
            out += [os.path.relpath(os.path.join(d, f), self.root).replace(os.sep, "/") for f in files]
        return out

    def open(self, name):
        return self.zip.open(name) if self.zip else open(os.path.join(self.root, name), "rb")


def cmd_prep(a):
    from PIL import Image
    raw, cg_dir, _ = raw_dirs(a.geo, a.name)
    proc = os.path.join(a.geo, "processed", a.name)
    _, origin, origin_alt = zone_envelope(proc)
    cache = os.path.join(cg_dir, "cache")
    os.makedirs(cache, exist_ok=True)
    zips = sorted(p for p in os.listdir(cg_dir) if p.endswith(".zip") or
                  (os.path.isdir(os.path.join(cg_dir, p)) and p.startswith("LYON_")))
    if not zips:
        print("no zip in " + cg_dir)
    for zname in zips:
        stem = zname[:-4] if zname.endswith(".zip") else zname
        out = os.path.join(cache, stem + ".npz")
        if os.path.exists(out):
            print(f"{stem}: cached")
            continue
        t0 = time.time()
        tdir = os.path.join(cache, stem)
        os.makedirs(tdir, exist_ok=True)
        zf = Source(os.path.join(cg_dir, zname))
        gmls = [n for n in zf.namelist() if n.lower().endswith(".gml") and "_tin_" not in n.lower()]
        if not gmls:
            print(f"{stem}: no gml")
            continue
        verts, uvs, polys, bids, kinds, images, nv = [], [], [], [], [], {}, 0
        stats = {"gml": len(gmls), "buildings": 0, "polygons": 0, "untextured": 0, "images": 0}
        for gml in gmls:
            base = posixpath.dirname(gml)
            tex = {}
            with zf.open(gml) as fh:
                for item in parse_citygml(fh):
                    if item[0] == "tex":
                        _, rid, img, txt = item
                        tex[rid] = (posixpath.normpath(posixpath.join(base, img)) if img else None, txt)
                        continue
                    _, bid, rings, kind = item
                    if not rings:
                        continue
                    stats["buildings"] += 1
                    bi = len(bids)
                    bids.append(bid)
                    kinds.append("bridge" if kind == "Bridge" or "_PONT" in gml.upper() else
                                 "remarkable" if "REMARQUABLE" in gml.upper() else "building")
                    for rid, pl in rings:
                        c = np.array(pl.split(), np.float64).reshape(-1, 3)
                        if len(c) < 4:
                            continue
                        c = c[:-1] if np.allclose(c[0], c[-1]) else c
                        x, y = cc46_to_l93(c[:, 0], c[:, 1])
                        loc = np.column_stack([x - origin[0], y - origin[1], c[:, 2] - origin_alt]).astype(np.float32)
                        img, txt = tex.get(rid, (None, None))
                        uv = None
                        if txt and img:
                            try:
                                uv = np.array(txt.split(), np.float32).reshape(-1, 2)
                            except ValueError:           # '-1.#IND00' and the like in a few remarkable files
                                uv = None
                            if uv is not None:
                                uv = uv[:len(loc)] if len(uv) >= len(loc) and np.isfinite(uv).all() else None
                        ii = -1
                        if uv is None:
                            stats["untextured"] += 1
                            uv = np.full((len(loc), 2), -1.0, np.float32)     # -1: grey patch
                        else:
                            ii = images.setdefault(img, len(images))
                        polys.append((bi, nv, len(loc), ii))
                        nv += len(loc)
                        verts.append(loc)
                        uvs.append(uv)
                        stats["polygons"] += 1
        # textures: one photo sheet per building, reduced to --near-tex px
        names = set(zf.namelist())
        for member in sorted(images):
            dst = os.path.join(tdir, member.replace("/", "__"))
            if os.path.exists(dst) or member not in names:
                continue
            with zf.open(member) as fh:
                im = Image.open(io.BytesIO(fh.read())).convert("RGB")
            im.thumbnail((a.prep_tex, a.prep_tex), Image.LANCZOS)
            im.save(dst, quality=88)
            stats["images"] += 1
        imgs = [""] * len(images)
        for member, k in images.items():
            imgs[k] = member.replace("/", "__")
        pi = np.array(polys, np.int64).reshape(-1, 4)
        np.savez_compressed(out, v=np.vstack(verts) if verts else np.zeros((0, 3), np.float32),
                            uv=np.vstack(uvs) if uvs else np.zeros((0, 2), np.float32),
                            polys=pi, bids=np.array([str(b or "") for b in bids], dtype=str),
                            images=np.array(imgs, dtype=str), kinds=np.array(kinds, dtype=str))    # str dtype: np.load refuses object arrays
        stats["seconds"] = round(time.time() - t0, 1)
        print(stem, json.dumps(stats))


# --- build -----------------------------------------------------------------------------------
class Atlas:
    """Shelf packer of square building sheets in one A x A image; slot 0 is the grey patch."""

    def __init__(self, size):
        from PIL import Image
        self.size, self.x, self.y, self.row = size, 0, 0, 0
        self.img = Image.new("RGB", (size, size), GREY)
        self.place = {}
        self.grey = self.add_patch(None, 16)

    def add_patch(self, im, s):
        if self.x + s > self.size:
            self.x, self.y, self.row = 0, self.y + self.row, 0
        if self.y + s > self.size:
            return None
        r = (self.x, self.y, s)
        if im is not None:
            self.img.paste(im.resize((s, s)), (self.x, self.y))
        self.x += s
        self.row = max(self.row, s)
        return r

    def cropped(self):
        """The atlas cut to the power-of-two size it uses, and the (u, v) factors for its UVs."""
        p2 = lambda n: max(64, 1 << (max(n, 1) - 1).bit_length())  # noqa: E731
        hgt = p2(self.y + self.row)
        wid = p2(self.x) if self.y == 0 else self.size
        if (wid, hgt) == (self.size, self.size):
            return self.img, (1.0, 1.0)
        return self.img.crop((0, 0, wid, hgt)), (self.size / wid, self.size / hgt)

    def uv(self, rect, uv):
        x0, y0, s = rect
        if rect is self.grey or uv[0, 0] < 0:
            x0, y0, s = self.grey
            return np.full((len(uv), 2), [(x0 + s / 2) / self.size, (y0 + s / 2) / self.size], np.float32)
        u = np.clip(uv[:, 0], 0, 1)
        v = 1.0 - np.clip(uv[:, 1], 0, 1)
        return np.column_stack([(x0 + 1 + u * (s - 2)) / self.size, (y0 + 1 + v * (s - 2)) / self.size]).astype(np.float32)


def load_street_footprints(proc, focus):
    """Shapely footprints of the buildings the street map generates (kit) and of the landmarks."""
    from shapely.strtree import STRtree
    gen = os.path.join(proc, "gen", focus)
    ids = set()
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        ids |= {r["bdtopo_id"] for r in csv.DictReader(f)}
    lm = os.path.join(proc, "landmarks.json")
    if os.path.exists(lm):
        with open(lm, encoding="utf-8") as f:
            ids |= set(json.load(f).get("replaced_bdtopo", []))
    import generate  # noqa: PLC0415  (shapely, numpy: the city venv)
    street, every = [], []
    for b in generate.load_buildings(proc):
        every.append((b["id"], b["poly"]))
        if b["id"] in ids:
            street.append(b["poly"])
    return STRtree(street) if street else None, street, every


def terrain_steps(spec):
    """'300:1,1200:2,inf:4' -> [(300.0, 1), (1200.0, 2), (inf, 4)]: sample stride per tile distance."""
    out = []
    for part in spec.split(","):
        d, k = part.split(":")
        out.append((float(d), int(k)))
    return sorted(out)


def terrain_mesh(far_dir, origin, origin_alt, zone_env, axis, steps, cut_down=0.5, skirt=6.0):
    """Yield (tile key, verts n x 3 local, uv, tris, ortho path, stride) per 1 km orthophoto tile.

    Neighbouring tiles share their edge samples. The stride (1 = the MNT resolution) grows with the
    tile's distance to the street axis; a skirt hangs from every tile edge, so the small steps
    where a fine tile meets a coarser one never open on the sky. Cells whose four corners are all
    inside the zone's LiDAR envelope are left out (the zone terrain is there); the far terrain is
    cut_down lower so that it stays under the zone terrain where they overlap by one cell."""
    from shapely.geometry import box
    with open(os.path.join(far_dir, "far.json"), encoding="utf-8") as f:
        meta = json.load(f)
    env, res, w, h = meta["envelope_l93"], meta["res_m"], meta["width"], meta["height"]
    g = np.fromfile(os.path.join(far_dir, meta["mnt"]), "<f4").reshape(h, w).astype(np.float64)
    g[g < -1000] = np.nan
    if np.isnan(g).any():
        g[np.isnan(g)] = np.nanmin(g)
    g = g[::-1]                                           # row 0 = south: rows grow with y
    xs = env[0] + (np.arange(w) + 0.5) * res
    ys = env[1] + (np.arange(h) + 0.5) * res
    inside = lambda x, y: (zone_env[0] < x) & (x < zone_env[2]) & (zone_env[1] < y) & (y < zone_env[3])  # noqa: E731

    def ladder(lo, hi, k):
        idx = list(range(lo, hi, k))
        return idx + [hi] if idx[-1] != hi else idx

    for x0 in range(int(env[0]), int(env[2]), 1000):
        for y0 in range(int(env[1]), int(env[3]), 1000):
            c0, c1 = int(np.searchsorted(xs, x0)), min(int(np.searchsorted(xs, x0 + 1000)), w - 1)
            r0, r1 = int(np.searchsorted(ys, y0)), min(int(np.searchsorted(ys, y0 + 1000)), h - 1)
            if c1 <= c0 or r1 <= r0:
                continue
            dist = axis.distance(box(x0 - origin[0], y0 - origin[1], x0 + 1000 - origin[0], y0 + 1000 - origin[1]))
            k = next(k for d, k in steps if dist <= d)
            ci, ri = ladder(c0, c1, k), ladder(r0, r1, k)
            X, Y = np.meshgrid(xs[ci], ys[ri])
            Z = g[np.ix_(ri, ci)] - origin_alt - cut_down
            nr, nc = X.shape
            keep = ~(inside(X[:-1, :-1], Y[:-1, :-1]) & inside(X[:-1, 1:], Y[:-1, 1:]) &
                     inside(X[1:, :-1], Y[1:, :-1]) & inside(X[1:, 1:], Y[1:, 1:]))
            if not keep.any():
                continue
            v = np.column_stack([X.ravel() - origin[0], Y.ravel() - origin[1], Z.ravel()])
            uv = np.column_stack([np.clip((X.ravel() - x0) / 1000.0, 0, 1),
                                  np.clip(1.0 - (Y.ravel() - y0) / 1000.0, 0, 1)])
            r, c = np.nonzero(keep)
            a_, b_, c_, d_ = r * nc + c, r * nc + c + 1, (r + 1) * nc + c, (r + 1) * nc + c + 1
            tris = [np.column_stack([a_, b_, d_]), np.column_stack([a_, d_, c_])]  # counter-clockwise from above
            # skirts: one quad down per kept border cell edge
            sv, suv, n0 = [], [], len(v)
            borders = [(keep[0, :], [(0, j) for j in range(nc)]),                 # south edge
                       (keep[-1, :], [(nr - 1, j) for j in range(nc)]),           # north edge
                       (keep[:, 0], [(i, 0) for i in range(nr)]),                 # west edge
                       (keep[:, -1], [(i, nc - 1) for i in range(nr)])]           # east edge
            for cells, line in borders:
                for e, kept in enumerate(cells):
                    if not kept:
                        continue
                    p, q = line[e][0] * nc + line[e][1], line[e + 1][0] * nc + line[e + 1][1]
                    base = n0 + len(sv)
                    sv += [v[p] - [0, 0, skirt], v[q] - [0, 0, skirt]]
                    suv += [uv[p], uv[q]]
                    tris.append(np.array([[p, q, base + 1], [p, base + 1, base]]))
            if sv:
                v, uv = np.vstack([v, sv]), np.vstack([uv, suv])
            yield ((x0, y0), v.astype(np.float32), uv.astype(np.float32), np.vstack(tris).astype(np.uint32),
                   os.path.join(far_dir, f"ortho_{x0}_{y0}.jpg"), k)


class GlbWriter:
    """Meshes with one baseColor texture each (embedded JPEG), node translation 0 (vertices in place)."""

    def __init__(self):
        self.bin, self.views, self.acc, self.meshes, self.nodes = [], [], [], [], []
        self.images, self.textures, self.materials, self.off = [], [], [], 0

    def view(self, data, target=None):
        pad = (-len(data)) % 4
        self.bin.append(data + b"\0" * pad)
        v = {"buffer": 0, "byteOffset": self.off, "byteLength": len(data)}
        if target:
            v["target"] = target
        self.views.append(v)
        self.off += len(data) + pad
        return len(self.views) - 1

    def material(self, name, jpeg_bytes, rough=0.95):
        self.images.append({"bufferView": self.view(jpeg_bytes), "mimeType": "image/jpeg", "name": name})
        self.textures.append({"source": len(self.images) - 1})
        self.materials.append({"name": "M_" + name[2:] if name.startswith("T_") else "M_" + name, "pbrMetallicRoughness": {
            "baseColorTexture": {"index": len(self.textures) - 1}, "metallicFactor": 0.0, "roughnessFactor": rough},
            "doubleSided": True})
        return len(self.materials) - 1

    def mesh(self, name, v, n, uv, idx, mat):
        to_gltf = lambda a: np.ascontiguousarray(np.column_stack([a[:, 0], a[:, 2], -a[:, 1]]), np.float32)  # noqa: E731
        gv, gn = to_gltf(v), to_gltf(n)
        attrs = {}
        for key, arr, typ, extra in (("POSITION", gv, "VEC3", {"min": gv.min(0).tolist(), "max": gv.max(0).tolist()}),
                                     ("NORMAL", gn, "VEC3", {}),
                                     ("TEXCOORD_0", np.ascontiguousarray(uv, np.float32), "VEC2", {})):
            self.acc.append({"bufferView": self.view(arr.tobytes(), 34962), "componentType": 5126,
                             "count": len(arr), "type": typ, **extra})
            attrs[key] = len(self.acc) - 1
        ii = np.ascontiguousarray(idx, np.uint32).ravel()
        self.acc.append({"bufferView": self.view(ii.tobytes(), 34963), "componentType": 5125,
                         "count": len(ii), "type": "SCALAR"})
        self.meshes.append({"name": name, "primitives": [{"attributes": attrs, "indices": len(self.acc) - 1,
                                                           "material": mat}]})
        self.nodes.append({"name": name, "mesh": len(self.meshes) - 1})

    def write(self, path, extras):
        g = {"asset": {"version": "2.0", "generator": "gran-theft-praline far_decor", "extras": extras},
             "scene": 0, "scenes": [{"nodes": list(range(len(self.nodes)))}], "nodes": self.nodes,
             "meshes": self.meshes, "materials": self.materials, "textures": self.textures,
             "images": self.images, "samplers": [], "accessors": self.acc, "bufferViews": self.views,
             "buffers": [{"byteLength": self.off}]}
        del g["samplers"]
        js = json.dumps(g, separators=(",", ":")).encode("utf-8")
        js += b" " * ((-len(js)) % 4)
        binary = b"".join(self.bin)
        with open(path, "wb") as f:
            f.write(struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(binary)))
            f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
            f.write(struct.pack("<II", len(binary), 0x004E4942) + binary)


def jpeg(im, q=85):
    b = io.BytesIO()
    im.save(b, "JPEG", quality=q)
    return b.getvalue()


def load_cache(cache, stem):
    """One prepped arrondissement: arrays of the npz, polygons sorted by object ("first" gives
    each object's slice), centroid "cen" and vertex count "cnt" per object, "kinds" (building,
    bridge, remarkable) and the "remarkable" mask."""
    with np.load(os.path.join(cache, stem + ".npz")) as z:      # load once: NpzFile decompresses per access
        d = {k: z[k] for k in z.files}
    order = np.argsort(d["polys"][:, 0], kind="stable")
    d["polys"] = d["polys"][order]
    nb = len(d["bids"])
    d["first"] = np.searchsorted(d["polys"][:, 0], np.arange(nb + 1))
    sums, cnt = np.zeros((nb, 3)), np.zeros(nb)       # object centroid = mean of its vertices
    for bi, s, n, _ in d["polys"]:
        sums[bi] += d["v"][s:s + n].sum(0)
        cnt[bi] += n
    d["cen"], d["cnt"] = sums / np.maximum(cnt, 1)[:, None], cnt
    if "kinds" not in d:                              # cache made before 13/11 19:30
        d["kinds"] = np.array(["building"] * nb)
    # remarkable objects: by file (REMARQUABLE), or for older caches by id (the BATI ids are
    # cadastral, 6938x...; the remarkable ones are UUIDs or empty)
    d["remarkable"] = (d["kinds"] == "remarkable") | \
        np.array([k == "building" and not str(b).startswith("6938") for k, b in zip(d["kinds"], d["bids"])],
                 dtype=bool)
    return d


def footprint(d, bi):
    """2D footprint of a CityGML object: union of its roof and ground faces seen from above
    (walls project to lines), or the convex hull of its vertices when it has none."""
    from shapely import MultiPoint
    from shapely.geometry import Polygon
    from shapely.ops import unary_union
    parts, pts = [], []
    for _, s, n, _ in d["polys"][d["first"][bi]:d["first"][bi + 1]]:
        q = d["v"][s:s + n].astype(np.float64)
        pts.append(q[:, :2])
        a2 = 0.5 * abs(np.dot(q[:, 0], np.roll(q[:, 1], -1)) - np.dot(np.roll(q[:, 0], -1), q[:, 1]))
        a3 = 0.5 * np.linalg.norm(np.cross(q - q[0], np.roll(q, -1, 0) - q[0]).sum(0))
        if a3 > 0.05 and a2 > 0.3 * a3:
            parts.append(Polygon(q[:, :2]).buffer(0))
    fp = unary_union(parts) if parts else None
    if fp is None or fp.is_empty or fp.area < 1.0:
        fp = MultiPoint(np.vstack(pts)).convex_hull if pts else None
    return fp


def cmd_build(a):
    from PIL import Image
    from shapely.geometry import LineString, Point
    t0 = time.time()
    raw, cg_dir, far_dir = raw_dirs(a.geo, a.name)
    proc = os.path.join(a.geo, "processed", a.name)
    man, origin, origin_alt = zone_envelope(proc)
    gen = os.path.join(proc, "gen", a.focus)
    with open(os.path.join(gen, "report.json"), encoding="utf-8") as f:
        axis = LineString([(p[0] / 100.0, -p[1] / 100.0) for p in json.load(f)["focus_axis_ue_cm"]])
    tree, street, every = load_street_footprints(proc, a.focus)
    # shrunk 1 m: the 2018 model and BD TOPO disagree by a metre or so along party walls
    street_core = [g.buffer(-1.0) for g in street]
    street_core = [g for g in street_core if not g.is_empty]
    river_box = None                              # riverside.py models the bridge there (deck with collision)
    if os.path.exists(os.path.join(gen, "riverside.json")):
        with open(os.path.join(gen, "riverside.json"), encoding="utf-8") as f:
            river_box = [float(t) for t in str(json.load(f)["bbox_m"]).split(",")]
    cache = os.path.join(cg_dir, "cache")
    stems = sorted(p[:-4] for p in os.listdir(cache) if p.endswith(".npz")) if os.path.isdir(cache) else []
    if not stems:
        raise SystemExit(f"no CityGML cache in {cache}: run fetch and prep first")
    # 1. choose the buildings and bin them per tile
    tiles = {}                                    # (tx, ty) -> list of (stem, building index, tex px)
    stats = {"citygml_buildings": 0, "kept": 0, "in_street_map": 0, "beyond_radius": 0, "riverside_bridges": 0,
             "copies_of_remarkable": 0, "remarkable": 0, "in_two_zips": 0, "small_faces_dropped": 0,
             "overlaps_street_map": 0, "objects": 0}
    data = {}
    for stem in stems:
        data[stem] = load_cache(cache, stem)
    # the BATI files also hold a plain LOD2 copy of some remarkable buildings: drop it
    from shapely import MultiPoint
    from shapely.strtree import STRtree as _Tree
    hulls = []
    for d in data.values():
        for bi in np.nonzero(d["remarkable"])[0]:
            sel = d["polys"][d["first"][bi]:d["first"][bi + 1]]
            if len(sel):
                pts = np.vstack([d["v"][s:s + n, :2] for _, s, n, _ in sel])
                h = MultiPoint(pts).convex_hull.buffer(-1.0)
                if not h.is_empty:
                    hulls.append(h)
    hull_tree = _Tree(hulls) if hulls else None
    core_tree = _Tree(street_core) if street_core else None
    kept_fp = []                                          # footprints near the street, for the backdrop
    seen = set()                                          # bridges on a boundary are in both zips
    for stem, d in data.items():
        stats["citygml_buildings"] += len(d["bids"])
        for bi in range(len(d["bids"])):
            if d["cnt"][bi] == 0:
                continue
            bid = str(d["bids"][bi])
            if bid and bid in seen:
                stats["in_two_zips"] += 1
                continue
            seen.add(bid)
            cx, cy = d["cen"][bi, 0], d["cen"][bi, 1]
            p = Point(cx, cy)
            dist = axis.distance(p)
            if dist > a.radius:
                stats["beyond_radius"] += 1
                continue
            if tree is not None and any(street[k].contains(p) for k in tree.query(p)):
                stats["in_street_map"] += 1
                continue
            fp = footprint(d, bi) if dist <= a.footprint_m else None
            if fp is not None and core_tree is not None:
                inter = sum(fp.intersection(street_core[k]).area for k in core_tree.query(fp))
                if inter > min(40.0, 0.25 * fp.area):   # a deep parcel reaching the street front
                    stats["overlaps_street_map"] += 1
                    continue
            if river_box and d["kinds"][bi] == "bridge" and \
                    river_box[0] <= cx <= river_box[2] and river_box[1] <= cy <= river_box[3]:
                stats["riverside_bridges"] += 1
                continue
            if hull_tree is not None and not d["remarkable"][bi] and \
                    any(hulls[k].contains(p) for k in hull_tree.query(p)):
                stats["copies_of_remarkable"] += 1
                continue
            key = (int(math.floor(cx / a.tile)), int(math.floor(cy / a.tile)))
            px = a.close_tex if dist <= a.close_m else a.near_tex if dist <= a.near_m else a.far_tex
            if d["remarkable"][bi] and dist <= a.object_m and fp is not None and fp.area <= a.object_max_m2:
                # a statue or a fountain where the player walks (Jacobins, Bellecour, Terreaux): its own
                # mesh, imported with collision, sheets at full size
                key, px = ("obj", stats["objects"]), a.close_tex
                stats["objects"] += 1
            tiles.setdefault(key, []).append((stem, bi, px, (cx, cy), dist))
            if fp is not None:
                kept_fp.append(fp)
            stats["kept"] += 1
            stats["remarkable"] += int(d["remarkable"][bi])
    # 2. per tile: atlases and merged meshes
    w = GlbWriter()
    nodes, covered_pts, tri_total, atlases = [], [], 0, 0
    for key in sorted(tiles, key=lambda k: (isinstance(k[0], str), str(k))):
        items = sorted(tiles[key], key=lambda t: -t[2])
        atlas, groups = Atlas(a.atlas), []
        cur = {"v": [], "n": [], "uv": [], "i": [], "nv": 0, "atlas": atlas, "count": 0}

        def flush(cur):
            if not cur["v"]:
                return
            if key[0] == "obj":
                name = "SM_FarObject_%02d_%d" % (key[1], len(groups))
            else:
                name = "SM_FarDecor_%s%04d_%s%04d_%d" % ("p" if key[0] >= 0 else "m", abs(key[0]),
                                                        "p" if key[1] >= 0 else "m", abs(key[1]), len(groups))
            img, (su, sv) = cur["atlas"].cropped()
            mat = w.material("T_" + name[3:], jpeg(img))
            V, N, UV = np.vstack(cur["v"]), np.vstack(cur["n"]), np.vstack(cur["uv"]) * [su, sv]
            I = np.vstack(cur["i"])
            w.mesh(name, V, N, UV, I, mat)
            groups.append({"name": name, "kind": "object" if key[0] == "obj" else "buildings", "tile": list(key),
                           "buildings": cur["count"],
                           "triangles": int(len(I)), "atlas_px": list(img.size)})

        for stem, bi, px, cen, dist in items:
            min_area = a.far_min_area if dist > a.near_m else 0.0
            d = data[stem]
            sel = d["polys"][d["first"][bi]:d["first"][bi + 1]]
            rects = {}
            for ii in sorted(set(int(k) for k in sel[:, 3]) - {-1}):
                p = os.path.join(cache, stem, str(d["images"][ii]))
                if not os.path.exists(p):
                    continue
                im = Image.open(p).convert("RGB")
                r = atlas.add_patch(im, px)
                if r is None:                     # atlas full: close this mesh, open a new atlas
                    flush(cur)
                    atlas = Atlas(a.atlas)
                    cur = {"v": [], "n": [], "uv": [], "i": [], "nv": 0, "atlas": atlas, "count": 0}
                    rects = {}                    # earlier patches of this building were in the old atlas
                    for jj in sorted(set(int(k) for k in sel[:, 3]) - {-1}):
                        pj = os.path.join(cache, stem, str(d["images"][jj]))
                        if os.path.exists(pj):
                            rects[jj] = atlas.add_patch(Image.open(pj).convert("RGB"), px) or atlas.grey
                    break
                rects[ii] = r
            for _, s, n, ii in sel:
                rect = rects.get(int(ii), atlas.grey)
                loc = d["v"][s:s + n].astype(np.float64)
                uv = d["uv"][s:s + n]
                nrm = np.zeros(3)
                for k in range(n):                      # Newell normal
                    p0, p1 = loc[k], loc[(k + 1) % n]
                    nrm += [(p0[1] - p1[1]) * (p0[2] + p1[2]), (p0[2] - p1[2]) * (p0[0] + p1[0]),
                            (p0[0] - p1[0]) * (p0[1] + p1[1])]
                ln = np.linalg.norm(nrm)
                if ln < 1e-9 or ln / 2 < min_area:             # |Newell| = twice the area
                    stats["small_faces_dropped"] += int(ln >= 1e-9)
                    continue
                nrm /= ln
                ax = int(np.argmax(np.abs(nrm)))
                p2 = np.delete(loc, ax, axis=1)
                if (ax == 1) ^ (nrm[ax] < 0):           # keep the projected winding = front face
                    p2 = p2[:, ::-1]
                tri = triangulate(p2)
                if not tri:
                    continue
                cur["v"].append(loc.astype(np.float32))
                cur["n"].append(np.tile(nrm.astype(np.float32), (n, 1)))
                cur["uv"].append(atlas.uv(rect, uv))
                cur["i"].append(np.array(tri, np.uint32) + cur["nv"])
                cur["nv"] += n
            cur["count"] += 1
            covered_pts.append(cen)
        flush(cur)
        atlases += len(groups)
        tri_total += sum(g["triangles"] for g in groups)
        nodes += groups
    # 3. terrain with orthophoto
    terrain_tris = 0
    if not a.no_terrain and os.path.exists(os.path.join(far_dir, "far.json")):
        with open(os.path.join(raw, "lidar.json"), encoding="utf-8") as f:
            zone_env = json.load(f)["envelope_l93"]
        with open(os.path.join(far_dir, "far.json"), encoding="utf-8") as f:
            res_m = json.load(f)["res_m"]
        for (x0, y0), v, uv, tris, ortho, k in terrain_mesh(far_dir, origin, origin_alt, zone_env, axis,
                                                            terrain_steps(a.terrain_steps)):
            name = "SM_FarTerrain_%d_%d" % (x0, y0)
            im = Image.open(ortho).convert("RGB") if os.path.exists(ortho) else Image.new("RGB", (8, 8), (120, 120, 110))
            im.thumbnail((a.ortho_px, a.ortho_px), Image.LANCZOS)
            mat = w.material("T_" + name[3:], jpeg(im, 82))
            nrm = np.tile(np.array([0, 0, 1], np.float32), (len(v), 1))
            w.mesh(name, v, nrm, uv, tris, mat)
            nodes.append({"name": name, "kind": "terrain", "tile_l93": [x0, y0], "triangles": int(len(tris)),
                          "ortho_px": a.ortho_px, "sample_m": k * res_m})
            terrain_tris += len(tris)
    # 4. which BD TOPO buildings the far decor covers (backdrop.py leaves them out): half of the
    # footprint under kept CityGML footprints near the street, or its centroid farther out
    from shapely.strtree import STRtree
    covered = set()
    if every:
        kt = STRtree(kept_fp) if kept_fp else None
        cen_tree = STRtree([Point(x, y) for x, y in covered_pts]) if covered_pts else None
        for bid, poly in every:
            if kt is not None and axis.distance(poly.centroid) <= a.footprint_m - 50:
                if sum(poly.intersection(kept_fp[k]).area for k in kt.query(poly)) >= 0.5 * poly.area:
                    covered.add(bid)
            elif cen_tree is not None and len(cen_tree.query(poly, predicate="contains")):
                covered.add(bid)
    out_glb = os.path.join(gen, "far_decor.glb")
    report = {"source": SOURCE, "arrondissements": stems, "radius_m": a.radius, "tile_m": a.tile,
              "close_m": a.close_m, "close_tex_px": a.close_tex, "near_m": a.near_m, "near_tex_px": a.near_tex,
              "far_tex_px": a.far_tex, "far_min_area_m2": a.far_min_area, "atlas_px": a.atlas, "terrain_steps": a.terrain_steps,
              **stats, "meshes": len(nodes), "building_meshes": atlases, "building_triangles": tri_total,
              "terrain_triangles": terrain_tris, "covered_bdtopo_count": len(covered)}
    px = sum(n["atlas_px"][0] * n["atlas_px"][1] for n in nodes if n["kind"] in ("buildings", "object")) + \
        sum(n["ortho_px"] ** 2 for n in nodes if n["kind"] == "terrain")
    report["texture_mpx"] = round(px / 1e6, 1)
    report["gpu_texture_mb_dxt1"] = round(px * 0.5 * 4 / 3 / 2 ** 20)   # BC1 with mips, before streaming
    w.write(out_glb, {"source": SOURCE})
    report["glb_mb"] = round(os.path.getsize(out_glb) / 1e6, 1)
    report["seconds"] = round(time.time() - t0, 1)
    with open(os.path.join(gen, "far_decor.json"), "w", encoding="utf-8") as f:
        json.dump({"nodes": nodes, "covered_bdtopo": sorted(covered), "report": report}, f, indent=1)
    print(json.dumps(report))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", choices=["fetch", "prep", "build"])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique", help="generated street folder under gen/")
    ap.add_argument("--arr", nargs="+", default=DEFAULT_ARR, help="arrondissements to fetch (LYON_<ARR>_2018.zip)")
    ap.add_argument("--radius", type=float, default=2500.0, help="metres around the street axis")
    ap.add_argument("--tile", type=float, default=500.0, help="merge square, metres")
    ap.add_argument("--prep-tex", type=int, default=512, help="building sheet kept by prep, px (largest tier)")
    ap.add_argument("--close-m", type=float, default=150.0, help="buildings this close to the axis get --close-tex")
    ap.add_argument("--close-tex", type=int, default=512)
    ap.add_argument("--near-m", type=float, default=600.0)
    ap.add_argument("--near-tex", type=int, default=256)
    ap.add_argument("--footprint-m", type=float, default=700.0,
                    help="within this distance, CityGML footprints decide the overlap with the street "
                         "map and which BD TOPO buildings the backdrop leaves out (beyond it: centroids)")
    ap.add_argument("--object-m", type=float, default=400.0,
                    help="remarkable objects (statues, fountains) this close to the axis get their own mesh, "
                         "with collision")
    ap.add_argument("--object-max-m2", type=float, default=600.0, help="footprint above which it is a building")
    ap.add_argument("--far-tex", type=int, default=128)
    ap.add_argument("--far-min-area", type=float, default=1.0,
                    help="faces smaller than this (m2) are dropped beyond --near-m (sculpture, railings)")
    ap.add_argument("--atlas", type=int, default=4096)
    ap.add_argument("--terrain-res", type=float, default=10.0)
    ap.add_argument("--terrain-steps", default="300:1,1200:2,inf:4",
                    help="sample stride per tile distance to the axis, 'metres:stride,...' (1 = --terrain-res)")
    ap.add_argument("--ortho-px", type=int, default=1024, help="orthophoto texture per km tile")
    ap.add_argument("--no-terrain", action="store_true")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    a = ap.parse_args()
    {"fetch": cmd_fetch, "prep": cmd_prep, "build": cmd_build}[a.step](a)


if __name__ == "__main__":
    main()
