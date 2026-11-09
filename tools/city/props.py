"""Street furniture placement for a generated street, from real positions.

Every prop comes from a surveyed object, never from a random scatter:
  Grand Lyon (Licence Ouverte)   litter bins (S_SERI_Sac = transparent-bag hoop), bike-rack
                                 hoops (one per arceau, in a row), bus shelters, tree grates
  OpenStreetMap (ODbL)           benches, street lamps, fire hydrants, post boxes, drinking
                                 fountains, bike-share stations, glass recycling bells
  shops (OSM, facade_modules)    café and restaurant terraces on pedestrian streets: tables,
                                 chairs, a parasol and a chalkboard easel by the door
Assets are the wave-1 catalogue items (assets/manifests/wave-01.json). Each row gets
the Unreal asset path that tools/assets/unreal/import_props.py creates.

Rules: a prop inside a building footprint or within 0.6 m of an earlier prop is
dropped; the yaw turns the prop's front (+X, import convention) away from the
nearest facade, i.e. towards the street; z is the LiDAR ground (0.5 m DTM),
refined by a downward trace in the editor.

  python3 tools/city/props.py --name presqu-ile --focus rue-de-la-republique [--buffer 45]
Output (regenerated, not committed, ODbL-derived): gen/<focus>/props_placement.csv
"""

import argparse
import csv
import json
import math
import os
import sys
import time

import numpy as np
from shapely.geometry import Point
from shapely.ops import unary_union
from shapely.strtree import STRtree

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import citylib as cl  # noqa: E402
import generate  # noqa: E402
import grandlyon  # noqa: E402
import raster  # noqa: E402

PROPS_ROOT = "/Game/Environment/Props"
OSM_TYPES = {
    "amenity=bench": "W1-01", "highway=street_lamp": "W1-04", "emergency=fire_hydrant": "W1-27",
    "amenity=post_box": "W1-24", "amenity=drinking_water": "W1-18", "amenity=bicycle_rental": "W1-13",
    "amenity=recycling": "W1-31",
}
TERRACE_TRADES = {"restaurant", "cafe", "bar", "pub", "fast_food", "ice_cream"}
MIN_GAP_M = 0.6
TERRACE_OUT_M = 2.6        # table centres in front of the facade
TERRACE_STEP_M = 1.9


def manifest_assets(path):
    with open(path, encoding="utf-8") as f:
        items = json.load(f)["items"]
    out = {}
    for it in items:
        a = it["asset"]
        out[it["id"]] = {"asset": a, "family": it["family"],
                         "path": "%s/%s/%s/SM_%s_A" % (PROPS_ROOT, it["family"], a, a)}
    return out


def main():  # noqa: C901, PLR0915
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--street", default="Rue de la République")
    ap.add_argument("--buffer", type=float, default=45.0)
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    ap.add_argument("--manifest", default=os.path.join(REPO, "assets", "manifests", "wave-01.json"))
    args = ap.parse_args()
    t0 = time.time()
    raw = os.path.join(args.geo, "raw", args.name)
    proc = os.path.join(args.geo, "processed", args.name)
    gen = os.path.join(proc, "gen", args.focus)
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        man = json.load(f)
    origin, origin_alt = man["origin_lambert93_m"], man["origin_altitude_m"]
    assets = manifest_assets(args.manifest)
    mnt = raster.Raster.load(raw, "mnt")

    roads = generate.load_roads(raw, origin)
    street = [r[0] for r in roads if r[2] == args.street]
    if not street:
        sys.exit("no road named %r" % args.street)
    zone = unary_union(street).buffer(args.buffer)
    footprints = [b["poly"] for b in generate.load_buildings(proc) if b["poly"].intersects(zone)]
    fp_tree = STRtree(footprints)
    with open(os.path.join(gen, "facade_modules.csv"), encoding="utf-8") as f:
        modules = [r for r in csv.DictReader(f) if r["side"] == "street" and r["storey"] == "0"]
    mod_xy = np.array([[float(r["x_m"]), float(r["y_m"])] for r in modules])

    def facing(x, y):
        """(ue_yaw of the nearest street facade, distance) -> prop front turned to the street."""
        d = np.hypot(mod_xy[:, 0] - x, mod_xy[:, 1] - y)
        k = int(np.argmin(d))
        return float(modules[k]["ue_yaw"]), float(d[k]), modules[k]

    rows, placed = [], []

    def add(cid, x, y, yaw, source, ref=""):
        if cid not in assets or not zone.contains(Point(x, y)):
            return False
        p = Point(x, y)
        if any(footprints[i].contains(p) for i in fp_tree.query(p)):
            return False
        if any(math.hypot(x - a, y - b) < MIN_GAP_M for a, b in placed):
            return False
        gz = mnt.sample(np.array([x + origin[0]]), np.array([y + origin[1]]))[0]
        z = 0.0 if np.isnan(gz) else float(gz) - origin_alt
        ux, uy, uz = cl.to_unreal(x, y, z)
        a = assets[cid]
        rows.append({"catalog_id": cid, "asset": a["asset"], "family": a["family"], "asset_path": a["path"],
                     "x_m": round(x, 2), "y_m": round(y, 2), "z_m": round(z, 2), "ue_x": ux, "ue_y": uy, "ue_z": uz,
                     "ue_yaw": round(yaw % 360.0, 1), "source": source, "ref": ref})
        placed.append((x, y))
        return True

    def local(ft):
        c = ft["geometry"]["coordinates"]
        return c[0] - origin[0], c[1] - origin[1]

    # Grand Lyon: bins, bike racks, shelters, tree grates
    for ft in grandlyon.load(raw, "bins"):
        x, y = local(ft)
        yaw = facing(x, y)[0]
        sac = "Sac" in (ft["properties"].get("referencemobilier") or "")
        add("W1-07" if sac else "W1-08", x, y, yaw, "grandlyon:bins", ft["properties"].get("identifiant", ""))
    for ft in grandlyon.load(raw, "bike_racks"):
        p = ft["properties"]
        if "projet" in (p.get("validite") or "").lower():
            continue  # planned, not built yet
        x, y = local(ft)
        yaw, _, _ = facing(x, y)
        n = int(p.get("nbarceaux") or max(1, (p.get("capacite") or 2) // 2))
        # hoops in a row parallel to the facade, 0.9 m apart, each turned across the pavement
        a = math.radians(yaw + 90.0)
        for k in range(min(n, 12)):
            off = (k - (n - 1) / 2) * 0.9
            add("W1-12", x + math.cos(a) * off, y - math.sin(a) * off, yaw, "grandlyon:bike_racks", p.get("nom", ""))
    for ft in grandlyon.load(raw, "shelters"):
        x, y = local(ft)
        add("W1-21", x, y, facing(x, y)[0] + 180.0, "grandlyon:shelters", ft["properties"].get("identifiant", ""))
    for ft in grandlyon.load(raw, "trees"):
        x, y = local(ft)
        add("W1-17", x, y, facing(x, y)[0], "grandlyon:trees", str(ft["properties"].get("identifiant", "")))

    # OSM street furniture
    with open(os.path.join(proc, "furniture.csv"), encoding="utf-8") as f:
        for r in csv.DictReader(f):
            cid = OSM_TYPES.get(r["type"])
            if cid:
                x, y = float(r["x_m"]), float(r["y_m"])
                add(cid, x, y, facing(x, y)[0], "osm:" + r["type"], r["osm_id"])

    # Terraces in front of cafés and restaurants on the focus street (pedestrian)
    with open(os.path.join(proc, "shops.csv"), encoding="utf-8") as f:
        shops = {r["osm_id"]: r for r in csv.DictReader(f)}
    terraces = 0
    for m in modules:
        if m["kind"] != "shop" or m["street"] != args.street or m["sign_continued"] != "False":
            continue
        if shops.get(m["shop_osm_id"], {}).get("type") not in TERRACE_TRADES:
            continue
        yaw = float(m["ue_yaw"])
        nx, ny = math.cos(math.radians(yaw)), -math.sin(math.radians(yaw))  # outward, local axes
        tx, ty = ny, -nx                                                     # along the facade
        x0, y0 = float(m["x_m"]), float(m["y_m"])
        add("W1-40", x0 + nx * 0.9 + tx * 1.0, y0 + ny * 0.9 + ty * 1.0, yaw, "terrace", m["shop_osm_id"])
        for k in (-1, 0, 1):
            cx, cy = x0 + nx * TERRACE_OUT_M + tx * k * TERRACE_STEP_M, y0 + ny * TERRACE_OUT_M + ty * k * TERRACE_STEP_M
            if not add("W1-34", cx, cy, yaw, "terrace", m["shop_osm_id"]):
                continue
            chair = "W1-35" if terraces % 2 else "W1-36"
            add(chair, cx + nx * 0.7, cy + ny * 0.7, yaw + 180.0, "terrace", m["shop_osm_id"])
            add(chair, cx - nx * 0.7, cy - ny * 0.7, yaw, "terrace", m["shop_osm_id"])
            if k == 0:
                add("W1-38", cx + tx * 0.95, cy + ty * 0.95, yaw, "terrace", m["shop_osm_id"])
        terraces += 1

    fields = ["catalog_id", "asset", "family", "asset_path", "x_m", "y_m", "z_m", "ue_x", "ue_y", "ue_z", "ue_yaw",
              "source", "ref"]
    with open(os.path.join(gen, "props_placement.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fields)
        w.writeheader()
        w.writerows(rows)
    by = {}
    for r in rows:
        by[r["catalog_id"] + " " + r["asset"]] = by.get(r["catalog_id"] + " " + r["asset"], 0) + 1
    report = {"props": len(rows), "terraces": terraces, "by_asset": dict(sorted(by.items())),
              "seconds": round(time.time() - t0, 1)}
    with open(os.path.join(gen, "props_report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main()
