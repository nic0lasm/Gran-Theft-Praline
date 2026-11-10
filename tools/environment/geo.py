"""Small geodesy helpers for the environment tools (no external dependency).

Lambert-93 (EPSG:2154) forward projection on GRS80, and the Unreal mapping used
by the city pipeline (tools/city): local metres = Lambert-93 minus the zone
origin, Unreal X = east, Y = south, Z = up, centimetres.
"""

import math

A = 6378137.0
E = 0.0818191910428158
LON0 = math.radians(3.0)
PHI0, PHI1, PHI2 = math.radians(46.5), math.radians(44.0), math.radians(49.0)
X0, Y0 = 700000.0, 6600000.0


def _m(phi):
    return math.cos(phi) / math.sqrt(1 - (E * math.sin(phi)) ** 2)


def _t(phi):
    s = math.sin(phi)
    return math.tan(math.pi / 4 - phi / 2) / ((1 - E * s) / (1 + E * s)) ** (E / 2)


_N = (math.log(_m(PHI1)) - math.log(_m(PHI2))) / (math.log(_t(PHI1)) - math.log(_t(PHI2)))
_F = _m(PHI1) / (_N * _t(PHI1) ** _N)
_R0 = A * _F * _t(PHI0) ** _N


def lambert93(lat_deg, lon_deg):
    """WGS84 latitude/longitude (degrees) -> Lambert-93 (x, y) metres."""
    phi, lam = math.radians(lat_deg), math.radians(lon_deg)
    r = A * _F * _t(phi) ** _N
    theta = _N * (lam - LON0)
    return X0 + r * math.sin(theta), Y0 + _R0 - r * math.cos(theta)


def to_unreal(lat_deg, lon_deg, origin, z_cm=0.0):
    """WGS84 -> Unreal centimetres of a city zone whose Lambert-93 origin is `origin`."""
    x, y = lambert93(lat_deg, lon_deg)
    return round((x - origin[0]) * 100.0, 1), round(-(y - origin[1]) * 100.0, 1), z_cm
