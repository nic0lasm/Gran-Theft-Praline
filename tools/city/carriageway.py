"""Carriageways fitted to the real kerbs: one polygon and one corrected centre line per road.

The BD TOPO road centre lines sit 0.8 m off the middle of the carriageway in the
Presqu'ile (median, 2.2 m at the 90th percentile) and their widths are often the
traffic lanes only. The LiDAR terrain the cars drive on has the real kerbs, so a car
on a lane drawn from those centre lines rode with two wheels on the kerb. The
Grand Lyon kerb lines (grandlyon_kerbs) give the real edges: every SAMPLE_M along
each car road, a ray each side finds the nearest kerb within MAX_HALF_M; where one
side has none, the other side's distance (or half of the ground_mask width) is used.
Distances are smoothed along the road, then the two edges make the polygon.

Output: processed/<zone>/carriageway.geojson (local metres), one feature per car road
piece: Polygon geometry, properties road_id (cleabs), centre (list of [x, y]), half
widths left/right per centre point, kerb_share (share of samples with a real kerb).
Read by ground_mask.py (asphalt and kerbs) and tools/city/traffic.py (lanes).

Usage: python3 tools/city/carriageway.py --name presqu-ile
"""

import argparse
import json
import os

import numpy as np
import shapely
from shapely.geometry import LineString, Point, Polygon, mapping, shape
from shapely.strtree import STRtree

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

SAMPLE_M = 2.0
MAX_HALF_M = 12.0
MIN_HALF_M = 2.2          # a car fits; below this the kerb found is a traffic island
SMOOTH = 5                # samples in the running median


def road_width(p):
    """Same rule as ground_mask.py when no kerb is found."""
    w = float(p.get("largeur_de_chaussee") or 0)
    return max(w, 3.2 * (p.get("nombre_de_voies") or 1), 6.0)


def is_car_road(p):
    if p.get("nature") in ("Sentier", "Escalier", "Piste cyclable", "Chemin", "Bac ou liaison maritime"):
        return False
    if p.get("vitesse_moyenne_vl") == 0:
        return False
    return (p.get("position_par_rapport_au_sol") or "0") in ("0", 0)


def running_median(v, k):
    v = np.asarray(v, float)
    out = v.copy()
    h = k // 2
    for i in range(len(v)):
        w = v[max(0, i - h):i + h + 1]
        w = w[~np.isnan(w)]
        out[i] = np.median(w) if len(w) else np.nan
    return out


def fit(line, kerbs, tree, fallback_half):
    n_s = max(2, int(line.length // SAMPLE_M) + 1)
    ds = np.linspace(0.0, line.length, n_s)
    pts, nrm, left, right = [], [], [], []
    for d in ds:
        a = line.interpolate(d)
        b = line.interpolate(min(d + 0.5, line.length)) if d + 0.5 <= line.length else line.interpolate(d - 0.5)
        t = np.array([b.x - a.x, b.y - a.y]) * (1 if d + 0.5 <= line.length else -1)
        t /= max(np.linalg.norm(t), 1e-9)
        n = np.array([-t[1], t[0]])                       # left of the drawing direction
        hit = []
        for s in (1.0, -1.0):
            ray = LineString([(a.x, a.y), (a.x + s * n[0] * MAX_HALF_M, a.y + s * n[1] * MAX_HALF_M)])
            dist = [Point(a.x, a.y).distance(ray.intersection(kerbs[i])) for i in tree.query(ray)
                    if ray.intersects(kerbs[i])]
            dist = [v for v in dist if v >= MIN_HALF_M]
            hit.append(min(dist) if dist else np.nan)
        pts.append((a.x, a.y))
        nrm.append(n)
        left.append(hit[0])
        right.append(hit[1])
    left, right = running_median(left, SMOOTH), running_median(right, SMOOTH)
    real = float(np.mean(~np.isnan(left) | ~np.isnan(right)))
    # One side missing: mirror the other; both missing: the generic width, centred.
    l2 = np.where(np.isnan(left), np.where(np.isnan(right), fallback_half, right), left)
    r2 = np.where(np.isnan(right), np.where(np.isnan(left), fallback_half, left), right)
    pts, nrm = np.array(pts), np.array(nrm)
    le = pts + nrm * l2[:, None]
    ri = pts - nrm * r2[:, None]
    centre = (le + ri) / 2.0
    half = (l2 + r2) / 2.0
    poly = Polygon(np.vstack([le, ri[::-1]])).buffer(0)
    return poly, centre, half, real


def build(zone):
    raw = os.path.join(REPO, "assets", "geo", "raw", zone)
    proc = os.path.join(REPO, "assets", "geo", "processed", zone)
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        o = np.array(json.load(f)["origin_lambert93_m"][:2])
    with open(os.path.join(raw, "grandlyon_kerbs.geojson"), encoding="utf-8") as f:
        ks = [shapely.transform(shape(ft["geometry"]), lambda xy: xy - o) for ft in json.load(f)["features"]]
    kerbs = [g for m in ks for g in getattr(m, "geoms", [m])]
    tree = STRtree(kerbs)
    with open(os.path.join(proc, "roads.geojson"), encoding="utf-8") as f:
        roads = json.load(f)["features"]
    feats, shares, offs = [], [], []
    for ft in roads:
        p = ft["properties"]
        if not is_car_road(p):
            continue
        for coords in ft["geometry"]["coordinates"]:
            line = LineString([c[:2] for c in coords])
            if line.length < 1.0:
                continue
            poly, centre, half, real = fit(line, kerbs, tree, road_width(p) / 2.0)
            if poly.is_empty:
                continue
            shares.append(real)
            offs += list(np.hypot(*(centre - np.array([line.interpolate(line.project(Point(*c))).coords[0]
                                                      for c in centre])).T))
            feats.append({"type": "Feature", "geometry": mapping(poly),
                          "properties": {"road_id": p.get("cleabs"), "kerb_share": round(real, 2),
                                         "centre": [[round(x, 2), round(y, 2)] for x, y in centre],
                                         "half_width_m": [round(float(h), 2) for h in half]}})
    with open(os.path.join(proc, "carriageway.geojson"), "w", encoding="utf-8") as f:
        json.dump({"type": "FeatureCollection", "features": feats}, f)
    return {"roads": len(feats), "kerb_share_mean": round(float(np.mean(shares)), 2) if shares else 0,
            "centre_shift_m_p50": round(float(np.median(offs)), 2) if offs else 0,
            "centre_shift_m_p90": round(float(np.percentile(offs, 90)), 2) if offs else 0}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    print(json.dumps(build(ap.parse_args().name), indent=2))


if __name__ == "__main__":
    main()
