"""IGN LiDAR HD elevation rasters (MNT terrain, MNS surface, MNH height).

Fetched from the Geoplateforme WMS-Raster as raw float32 grids (image/x-bil),
in Lambert-93, 0.5 m per pixel. Licence Ouverte Etalab 2.0, credit "IGN, LiDAR HD".

On disk (never committed): assets/geo/raw/<name>/lidar_<kind>.f32 + lidar.json
Row 0 is the NORTH edge, as served by WMS.
"""

import json
import os

import numpy as np

import citylib as cl

WMS_R = "https://data.geopf.fr/wms-r/wms"
LAYERS = {
    "mnt": "IGNF_LIDAR-HD_MNT_ELEVATION.ELEVATIONGRIDCOVERAGE.LAMB93",
    "mns": "IGNF_LIDAR-HD_MNS_ELEVATION.ELEVATIONGRIDCOVERAGE.LAMB93",
    "mnh": "IGNF_LIDAR-HD_MNH_ELEVATION.ELEVATIONGRIDCOVERAGE.LAMB93",
}
CHUNK_PX = 2000  # service limit is 5010 px per side
NODATA_BELOW = -1000.0


def l93_envelope(bbox_wgs84, pad_m=20.0):
    """Lambert-93 envelope (xmin, ymin, xmax, ymax) of a lat0,lon0,lat1,lon1 box, snapped to 1 m."""
    lat0, lon0, lat1, lon1 = bbox_wgs84
    pts = [cl.lonlat_to_l93(lo, la) for lo in (lon0, lon1) for la in (lat0, lat1)]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    return (float(np.floor(min(xs) - pad_m)), float(np.floor(min(ys) - pad_m)),
            float(np.ceil(max(xs) + pad_m)), float(np.ceil(max(ys) + pad_m)))


def fetch(out_dir, env, res=0.5, kinds=("mnt", "mns", "mnh")):
    xmin, ymin, xmax, ymax = env
    w, h = int(round((xmax - xmin) / res)), int(round((ymax - ymin) / res))
    for kind in kinds:
        grid = np.full((h, w), np.nan, dtype=np.float32)
        for r0 in range(0, h, CHUNK_PX):
            for c0 in range(0, w, CHUNK_PX):
                cw, ch = min(CHUNK_PX, w - c0), min(CHUNK_PX, h - r0)
                bx0, by1 = xmin + c0 * res, ymax - r0 * res
                bbox = f"{bx0},{by1 - ch * res},{bx0 + cw * res},{by1}"
                url = WMS_R + "?" + cl.qs({
                    "SERVICE": "WMS", "VERSION": "1.3.0", "REQUEST": "GetMap", "LAYERS": LAYERS[kind],
                    "STYLES": "", "CRS": "EPSG:2154", "BBOX": bbox, "WIDTH": cw, "HEIGHT": ch,
                    "FORMAT": "image/x-bil;bits=32"})
                data = cl.http_get(url)
                if len(data) != cw * ch * 4:
                    raise RuntimeError(f"unexpected LiDAR tile size {len(data)} for {kind} {bbox}")
                grid[r0:r0 + ch, c0:c0 + cw] = np.frombuffer(data, "<f4").reshape(ch, cw)
        grid[grid < NODATA_BELOW] = np.nan
        grid.tofile(os.path.join(out_dir, f"lidar_{kind}.f32"))
        print(f"lidar {kind}: {w}x{h} px, {np.nanmin(grid):.1f}..{np.nanmax(grid):.1f} m")
    meta = {"envelope_l93": env, "res_m": res, "width": w, "height": h, "row0": "north",
            "dtype": "float32 little-endian", "source": "IGN LiDAR HD (WMS-R)",
            "licence": "Licence Ouverte Etalab 2.0"}
    with open(os.path.join(out_dir, "lidar.json"), "w") as f:
        json.dump(meta, f, indent=2)
    return meta


class Raster:
    """A north-up grid with world (Lambert-93) lookups."""

    def __init__(self, grid, env, res):
        self.grid, self.env, self.res = grid, tuple(env), float(res)

    @classmethod
    def load(cls, raw_dir, kind):
        with open(os.path.join(raw_dir, "lidar.json")) as f:
            meta = json.load(f)
        g = np.fromfile(os.path.join(raw_dir, f"lidar_{kind}.f32"), "<f4").reshape(meta["height"], meta["width"])
        return cls(g, meta["envelope_l93"], meta["res_m"])

    def to_px(self, x, y):
        """World -> fractional (col, row) of pixel centres."""
        return (np.asarray(x) - self.env[0]) / self.res - 0.5, (self.env[3] - np.asarray(y)) / self.res - 0.5

    def sample(self, x, y):
        """Bilinear sample; NaN outside the grid or on nodata."""
        c, r = self.to_px(x, y)
        c, r = np.atleast_1d(c).astype(float), np.atleast_1d(r).astype(float)
        h, w = self.grid.shape
        c0, r0 = np.floor(c).astype(int), np.floor(r).astype(int)
        ok = (c0 >= 0) & (r0 >= 0) & (c0 + 1 < w) & (r0 + 1 < h)
        out = np.full(c.shape, np.nan)
        c0, r0, fc, fr = c0[ok], r0[ok], c[ok] - c0[ok], r[ok] - r0[ok]
        g = self.grid
        out[ok] = (g[r0, c0] * (1 - fc) * (1 - fr) + g[r0, c0 + 1] * fc * (1 - fr) +
                   g[r0 + 1, c0] * (1 - fc) * fr + g[r0 + 1, c0 + 1] * fc * fr)
        return out

    def cells_in_ring(self, ring, inset_m=0.0):
        """World x, y and values of the cells whose centre lies inside the ring (optionally inset)."""
        xs, ys = [p[0] for p in ring], [p[1] for p in ring]
        c0, r1 = self.to_px(min(xs), min(ys))
        c1, r0 = self.to_px(max(xs), max(ys))
        h, w = self.grid.shape
        c0, c1 = max(0, int(np.floor(c0))), min(w - 1, int(np.ceil(c1)))
        r0, r1 = max(0, int(np.floor(r0))), min(h - 1, int(np.ceil(r1)))
        if c1 < c0 or r1 < r0:
            return np.empty(0), np.empty(0), np.empty(0)
        cc, rr = np.meshgrid(np.arange(c0, c1 + 1), np.arange(r0, r1 + 1))
        px = self.env[0] + (cc + 0.5) * self.res
        py = self.env[3] - (rr + 0.5) * self.res
        inside = points_in_ring(px, py, ring)
        if inset_m > 0:
            inside &= dist_to_ring(px, py, ring) >= inset_m
        v = self.grid[rr[inside], cc[inside]]
        good = ~np.isnan(v)
        return px[inside][good], py[inside][good], v[good]


def points_in_ring(px, py, ring):
    inside = np.zeros(np.shape(px), dtype=bool)
    for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]):
        if y0 == y1:
            continue
        cond = (y0 > py) != (y1 > py)
        xint = (x1 - x0) * (py - y0) / (y1 - y0) + x0
        inside ^= cond & (px < xint)
    return inside


def dist_to_ring(px, py, ring):
    best = np.full(np.shape(px), np.inf)
    for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]):
        dx, dy = x1 - x0, y1 - y0
        L = dx * dx + dy * dy
        t = np.zeros(np.shape(px)) if L == 0 else np.clip(((px - x0) * dx + (py - y0) * dy) / L, 0, 1)
        best = np.minimum(best, np.hypot(px - (x0 + t * dx), py - (y0 + t * dy)))
    return best
