"""Footprints of the building table as PCG input: one closed spline per building, with MaxHeight.

Reads assets/geo/processed/<zone>/buildings_lyon/buildings_lyon.geojson (tools/city/building_table.py)
and writes what tools/city/unreal/create_footprint_splines.py places in a level, for City Sample's
PCG building primitives (Assign_CitySample_Buildings reads footprint splines and @data.MaxHeight)
and for the Lyon facade grammar of tools/city/pcg_lyon.py.

    python3 tools/city/pcg_footprints.py --name presqu-ile --block 69382AK57      # its city block
    python3 tools/city/pcg_footprints.py --name presqu-ile --block-at 120,-400    # block at x,y (m)
    python3 tools/city/pcg_footprints.py --name presqu-ile --slice                # the whole slice

A block (îlot) is a set of footprints that touch each other (party walls), as on the ground.
Output (out of Git): assets/geo/processed/<zone>/pcg/<out>.json and <out>.csv
  json  per building: id, points_cm (outer ring, closed implicitly, anticlockwise seen from above
        in x east / y north), ground_z_cm, max_height_cm (eave = wall height: @data.MaxHeight),
        top_height_cm, style fields, and the grammar of pcg_lyon.grammar_for
  csv   the same scalars, one row per building (a data table keyed by BuildingId for PCG)
Units: centimetres in the Unreal world of the street map (X = x·100, Y = -y·100, Z = z·100).
"""

import argparse
import csv
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pcg_lyon  # noqa: E402

GEO = os.path.join(HERE, "..", "..", "assets", "geo")
SIMPLIFY_M = 0.3      # spline points closer to a straight edge than this are dropped
TOUCH_M = 0.6         # footprints closer than this belong to the same block
MIN_AREA_M2 = 8.0


def load_table(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def outer_ring(geom):
    """Largest polygon's outer ring, simplified, anticlockwise (x east, y north), not repeated."""
    from shapely.geometry import Polygon, shape
    g = shape(geom)
    if g.geom_type == "MultiPolygon":
        g = max(g.geoms, key=lambda p: p.area)
    ring = Polygon(g.exterior).simplify(SIMPLIFY_M, preserve_topology=True)
    if ring.is_empty or ring.area < MIN_AREA_M2:
        return None, 0.0
    pts = list(ring.exterior.coords)[:-1]
    if not ring.exterior.is_ccw:
        pts = pts[::-1]
    return pts, g.area


def blocks(features):
    """Index of the block of each feature: connected components of touching footprints."""
    from shapely.geometry import shape
    from shapely.strtree import STRtree
    geoms = [shape(f["geometry"]).buffer(TOUCH_M / 2) for f in features]
    tree = STRtree(geoms)
    parent = list(range(len(geoms)))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for i, g in enumerate(geoms):
        for j in tree.query(g):
            if j > i and g.intersects(geoms[j]):
                parent[root(j)] = root(i)
    return [root(i) for i in range(len(geoms))]


def select(features, a):
    if a.slice:
        return [f for f in features if f["properties"].get("in_slice")], "slice"
    if a.bbox:
        x0, y0, x1, y1 = (float(v) for v in a.bbox.split(","))
        return [f for f in features
                if x0 <= f["properties"]["centroid_x_m"] <= x1 and y0 <= f["properties"]["centroid_y_m"] <= y1], "bbox"
    from shapely.geometry import Point, shape
    if a.block:
        seed = next((i for i, f in enumerate(features) if f["properties"]["id"] == a.block), None)
        if seed is None:
            raise SystemExit(f"no building {a.block} in the table")
    else:
        x, y = (float(v) for v in a.block_at.split(","))
        p = Point(x, y)
        seed = min(range(len(features)), key=lambda i: shape(features[i]["geometry"]).distance(p))
    # Only the neighbourhood of the seed: blocks of the whole table would be slow and useless.
    c = shape(features[seed]["geometry"]).centroid
    near = [f for f in features if abs(f["properties"]["centroid_x_m"] - c.x) < 400
            and abs(f["properties"]["centroid_y_m"] - c.y) < 400]
    k = near.index(features[seed])
    labels = blocks(near)
    return [f for f, lab in zip(near, labels) if lab == labels[k]], "block-" + str(features[seed]["properties"]["id"])


def building_record(f):
    p = f["properties"]
    pts, area = outer_ring(f["geometry"])
    if pts is None or p.get("height_eave_m") in (None, 0):
        return None
    ground = float(p.get("ground_z_m") or 0.0)
    eave_h = float(p["height_eave_m"])
    top_h = float(p.get("height_ridge_m") or eave_h)
    g = pcg_lyon.grammar_for(p)
    return {
        "id": p["id"], "source": p["source"], "bdtopo_id": p.get("bdtopo_id"),
        "points_cm": [[round(x * 100.0, 1), round(-y * 100.0, 1), round(ground * 100.0, 1)] for x, y in pts],
        "ground_z_cm": round(ground * 100.0, 1), "max_height_cm": int(round(eave_h * 100.0)),
        "top_height_cm": int(round(top_h * 100.0)), "footprint_m2": round(area, 1),
        "style": p.get("style"), "year_built": p.get("year_built"), "storeys": p.get("storeys"),
        "roof_type": p.get("roof_type"), "ground_floor_commercial": p.get("ground_floor_commercial"),
        "remarkable": bool(p.get("remarkable")), "in_street_map": bool(p.get("in_street_map")),
        "grammar": g,
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--geo", default=GEO)
    sel = ap.add_mutually_exclusive_group(required=True)
    sel.add_argument("--block", help="building id of the table: export its whole block")
    sel.add_argument("--block-at", help="x,y in local metres: the block of the nearest building")
    sel.add_argument("--slice", action="store_true", help="every building of the slice (in_slice)")
    sel.add_argument("--bbox", help="x0,y0,x1,y1 in local metres (centroids inside)")
    ap.add_argument("--out", help="output name (default: block-<id>, slice or bbox)")
    ap.add_argument("--with-remarkable", action="store_true",
                    help="also the remarkable buildings (left out by default: they keep their 2018 mesh)")
    a = ap.parse_args(argv)

    table = os.path.join(a.geo, "processed", a.name, "buildings_lyon", "buildings_lyon.geojson")
    data = load_table(table)
    chosen, label = select(data["features"], a)
    recs, skipped, meshes = [], [], []
    for f in chosen:
        if f["properties"].get("remarkable") and not a.with_remarkable:
            meshes.append(f["properties"]["id"])      # basilica, Hôtel-Dieu, fountains: the 2018 mesh
            continue
        r = building_record(f)
        (recs if r else skipped).append(r or f["properties"]["id"])
    name = a.out or label
    out_dir = os.path.join(a.geo, "processed", a.name, "pcg")
    os.makedirs(out_dir, exist_ok=True)
    meta = data.get("metadata", {})
    doc = {"what": "Footprints of the building table as closed splines for the PCG building primitives "
                   "(tools/city/pcg_footprints.py, selection: %s)" % name,
           "units": "centimetres, Unreal world of L_City_RueDeLaRepublique (X east, Y south, Z up); origin "
                    "Lambert-93 %s, altitude %s m" % (meta.get("origin_lambert93_m"), meta.get("origin_altitude_m")),
           "max_height_cm": "eave height above the ground (wall without the roof) = @data.MaxHeight; "
                            "top_height_cm = ridge, for comparison",
           "points": "outer ring, closed (last point not repeated), anticlockwise seen from above in x east / "
                     "y north, at ground level; courtyards dropped",
           "grammar": "Lyon facade grammar proposal of tools/city/pcg_lyon.py (kit meshes in "
                      + pcg_lyon.KIT_DIR + "), not yet checked in an editor",
           "sources": meta.get("sources"),
           "count": len(recs), "skipped": skipped, "remarkable_kept_as_mesh": meshes, "buildings": recs}
    with open(os.path.join(out_dir, name + ".json"), "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, indent=1)
    cols = ["BuildingId", "MaxHeight", "TopHeight", "GroundZ", "Family", "Style", "Storeys", "Roof",
            "Shops", "Vertical", "RowRDC", "RowENT", "RowNOB", "RowETG", "RowBAL", "RowSOM", "RowRoof"]
    with open(os.path.join(out_dir, name + ".csv"), "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(cols)
        for r in recs:
            g = r["grammar"]
            rows = g["rows"]
            w.writerow([r["id"], r["max_height_cm"], r["top_height_cm"], r["ground_z_cm"], g["family"],
                        r["style"], len(g["storeys"]), g["roof"], int(bool(r["ground_floor_commercial"])),
                        g["vertical"]] + [rows.get(k, "") for k in ("RDC", "ENT", "NOB", "ETG", "BAL", "SOM")]
                       + [rows.get("ROOF_" + g["roof"], "")])
    fams = {}
    for r in recs:
        fams[r["grammar"]["family"]] = fams.get(r["grammar"]["family"], 0) + 1
    print(json.dumps({"out": os.path.normpath(os.path.join(out_dir, name + ".json")), "buildings": len(recs),
                      "skipped": len(skipped), "remarkable_kept_as_mesh": len(meshes), "families": fams}))


if __name__ == "__main__":
    main()
