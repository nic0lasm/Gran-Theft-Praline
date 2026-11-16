"""Take the OSM bollards that stand on a carriageway out of an existing details table.

For a machine that holds the generated tables but not the raw geo data details.py reads (LiDAR,
OSM extract, Grand Lyon kerbs): the same result as replaying details.py after the fix that leaves
these bollards out, without recomputing anything else. Only rows with source "osm:bollard" can
go; every other row is written back unchanged, in the same order.

    python3 tools/city/drop_carriageway_bollards.py --name presqu-ile --focus rue-de-la-republique
    ... --dry-run     only says what would go

Which rows: those inside a polygon of processed/<zone>/carriageway.geojson (mode "geometry",
needs shapely), or, when that file or shapely is missing, the 13 OSM nodes found on the
République focus on 2026-11-16 (mode "list"). The table before the change is kept once as
details_placement.before-bollards.csv next to it. Then replay the placement in the editor:
GTP_PROPS_SET=details with tools/city/unreal/place_props.py.
"""

import argparse
import csv
import json
import os
import shutil

GEO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "geo")
# Access bollards in the middle of a street (rue Palais Grillet, rue Thomassin, rue du Président
# Edouard Herriot...), as details.py listed them in bollards_left_out_on_carriageway.
KNOWN_REFS = ("node/13296699730", "node/1513947088", "node/1528154151", "node/6367225474", "node/6367225475",
              "node/6367225476", "node/6370605095", "node/6370605103", "node/6370605104", "node/6370605105",
              "node/6370605106", "node/6372006467", "node/6372006471")


def on_way_test(proc):
    """(mode, test(row) -> bool). Geometry when the carriageways and shapely are there."""
    path = os.path.join(proc, "carriageway.geojson")
    try:
        from shapely.geometry import Point, shape
        with open(path, encoding="utf-8") as f:
            polys = [shape(ft["geometry"]) for ft in json.load(f)["features"]]
    except (ImportError, OSError, ValueError, KeyError):
        return "list", lambda r: r["ref"] in KNOWN_REFS
    boxes = [g.bounds for g in polys]

    def test(r):
        x, y = float(r["x_m"]), float(r["y_m"])
        p = Point(x, y)
        return any(b[0] <= x <= b[2] and b[1] <= y <= b[3] and g.contains(p) for g, b in zip(polys, boxes))
    return "geometry", test


def split(rows, test):
    """(kept, removed): only OSM bollards can be removed."""
    kept, removed = [], []
    for r in rows:
        (removed if r.get("source") == "osm:bollard" and test(r) else kept).append(r)
    return kept, removed


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--geo", default=GEO)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args(argv)
    proc = os.path.join(a.geo, "processed", a.name)
    table = os.path.join(proc, "gen", a.focus, "details_placement.csv")
    with open(table, encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        fields, rows = reader.fieldnames, list(reader)
    mode, test = on_way_test(proc)
    kept, removed = split(rows, test)
    count = lambda rs: sum(r.get("source") == "osm:bollard" for r in rs)   # noqa: E731
    report = {"table": os.path.normpath(table), "mode": mode, "dry_run": a.dry_run,
              "rows_before": len(rows), "rows_after": len(kept),
              "bollards_before": count(rows), "bollards_after": count(kept),
              "removed": sorted(r["ref"] for r in removed),
              "known_refs_still_there": sorted(r["ref"] for r in kept if r["ref"] in KNOWN_REFS)}
    if removed and not a.dry_run:
        backup = table[:-4] + ".before-bollards.csv"
        if not os.path.exists(backup):
            shutil.copyfile(table, backup)
        with open(table, "w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fields)
            w.writeheader()
            w.writerows(kept)
        report["backup"] = os.path.normpath(backup)
    print("bollards off the carriageway: " + json.dumps(report))


if __name__ == "__main__":
    main()
