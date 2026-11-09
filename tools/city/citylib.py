"""Shared helpers for the city pipeline (stdlib only).

Coordinates
-----------
* WGS84 (lon, lat) for anything coming from OpenStreetMap.
* Lambert-93 (EPSG:2154) metres for IGN data and all processed output,
  recentred on the dataset origin ("local metres": x = east, y = north).
* Unreal units (cm): UE X = east, UE Y = south (= -north), UE Z = up.
"""

import json
import math
import time
import urllib.error
import urllib.parse
import urllib.request

USER_AGENT = "gran-theft-praline-city-pipeline/0.1 (https://github.com/nic0lasm/gran-theft-praline)"

# --- Lambert-93 (LCC 2SP on GRS80) -------------------------------------------
_A = 6378137.0
_F = 1 / 298.257222101
_E = math.sqrt(2 * _F - _F * _F)
_LAT1, _LAT2, _LAT0, _LON0 = (math.radians(v) for v in (44.0, 49.0, 46.5, 3.0))
_X0, _Y0 = 700000.0, 6600000.0


def _m(lat):
    s = math.sin(lat)
    return math.cos(lat) / math.sqrt(1 - _E * _E * s * s)


def _t(lat):
    s = math.sin(lat)
    return math.tan(math.pi / 4 - lat / 2) / ((1 - _E * s) / (1 + _E * s)) ** (_E / 2)


_N = (math.log(_m(_LAT1)) - math.log(_m(_LAT2))) / (math.log(_t(_LAT1)) - math.log(_t(_LAT2)))
_G = _m(_LAT1) / (_N * _t(_LAT1) ** _N)
_RHO0 = _A * _G * _t(_LAT0) ** _N


def lonlat_to_l93(lon, lat):
    """WGS84 degrees -> Lambert-93 metres (error well below 1 m for GRS80 ~ WGS84)."""
    lon, lat = math.radians(lon), math.radians(lat)
    rho = _A * _G * _t(lat) ** _N
    theta = _N * (lon - _LON0)
    return _X0 + rho * math.sin(theta), _Y0 + _RHO0 - rho * math.cos(theta)


# --- Geometry ------------------------------------------------------------------
def ring_area(ring):
    """Signed shoelace area (positive for counter-clockwise rings)."""
    s = 0.0
    for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]):
        s += x0 * y1 - x1 * y0
    return s / 2


def ring_centroid(ring):
    a = ring_area(ring)
    if abs(a) < 1e-9:
        n = len(ring)
        return sum(p[0] for p in ring) / n, sum(p[1] for p in ring) / n
    cx = cy = 0.0
    for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]):
        k = x0 * y1 - x1 * y0
        cx += (x0 + x1) * k
        cy += (y0 + y1) * k
    return cx / (6 * a), cy / (6 * a)


def point_in_ring(pt, ring):
    x, y = pt
    inside = False
    for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]):
        if (y0 > y) != (y1 > y) and x < (x1 - x0) * (y - y0) / (y1 - y0) + x0:
            inside = not inside
    return inside


def dist_point_ring(pt, ring):
    """Distance from a point to the boundary of a ring."""
    px, py = pt
    best = float("inf")
    for (x0, y0), (x1, y1) in zip(ring, ring[1:] + ring[:1]):
        dx, dy = x1 - x0, y1 - y0
        L = dx * dx + dy * dy
        t = 0.0 if L == 0 else max(0.0, min(1.0, ((px - x0) * dx + (py - y0) * dy) / L))
        best = min(best, math.hypot(px - (x0 + t * dx), py - (y0 + t * dy)))
    return best


def to_unreal(x, y, z=0.0):
    """Local metres (east, north, up) -> Unreal centimetres (X east, Y south, Z up)."""
    return round(x * 100, 1), round(-y * 100, 1), round(z * 100, 1)


# --- HTTP ----------------------------------------------------------------------
def http_get(url, timeout=180, retries=4):
    last = None
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return resp.read()
        except (urllib.error.URLError, TimeoutError, ConnectionError) as exc:
            last = exc
            time.sleep(2 ** (attempt + 1))
    raise RuntimeError(f"GET failed after {retries} attempts: {url}: {last}")


def http_json(url, **kw):
    return json.loads(http_get(url, **kw))


def qs(params):
    return urllib.parse.urlencode(params)
