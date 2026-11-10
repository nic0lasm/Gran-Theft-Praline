"""Weight report of the streaming cells for the M1 budget (ADR-0002, ADR-0010).

Per cell (processed/<zone>/cells.json from cells.py): facade instances (modules plus one
eave per street bay of the top storey), distinct module meshes (a draw call each per
material slot, with HISM), shop signs, shell triangles, street furniture and trees from
the zone's OSM and Grand Lyon tables, and an estimated city GPU cost.

The estimate is a linear calibration on the one measured map, rue de la République
extended to the Rhone (10/11, m1 profile): 21 299 facade instances and ~3 400 props and
details for the 14 ms city share of ADR-0002. It is an order of magnitude to pick the cells
to stream together, not a measure; the optimisation thread measures the packaged cells.
The report gives the worst 2 x 2 and 3 x 3 blocks of neighbouring cells.

Output: processed/<zone>/cells_budget.json and a Markdown table on stdout.
Usage: python3 tools/city/cell_budget.py --name presqu-ile
"""

import argparse
import csv
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

REF_INSTANCES = 21299 + 3400          # measured street map, facades + props and details
REF_CITY_MS = 14.0                    # ADR-0002 city share for that map
MS_PER_INSTANCE = REF_CITY_MS / REF_INSTANCES
PROPS_PER_OSM_ITEM = 3.0              # props.py + details.py place ~3 objects per mapped item
BUDGET_MS = 14.0                      # ADR-0010: the detailed cells together


def count_in(rows, bbox):
    x0, y0, x1, y1 = bbox
    return sum(1 for r in rows if x0 <= float(r["x_m"]) < x1 and y0 <= float(r["y_m"]) < y1)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    a = ap.parse_args()
    proc = os.path.join(REPO, "assets", "geo", "processed", a.name)
    with open(os.path.join(proc, "cells.json"), encoding="utf-8") as f:
        cells = json.load(f)
    tables = {}
    for t in ("furniture", "trees"):
        with open(os.path.join(proc, t + ".csv"), encoding="utf-8") as f:
            tables[t] = list(csv.DictReader(f))
    out = []
    for c in cells["cells"]:
        gen = os.path.join(proc, "gen", c["name"])
        rec = {"name": c["name"], "bbox_m": c["bbox_m"], "buildings": c.get("buildings") or 0}
        mods_csv = os.path.join(gen, "facade_modules.csv")
        rows = []
        if os.path.exists(mods_csv):
            with open(mods_csv, encoding="utf-8") as f:
                rows = list(csv.DictReader(f))
        top = {}
        for r in rows:
            top[r["building_id"]] = max(top.get(r["building_id"], 0), int(r["storey"]))
        eaves = sum(1 for r in rows if r["side"] == "street" and int(r["storey"]) == top[r["building_id"]])
        rec["facade_instances"] = len(rows) + eaves
        rec["module_kinds"] = len({(r["typology"], r["kind"]) for r in rows})
        rec["signs"] = sum(1 for r in rows if r.get("sign_name"))
        rep = {}
        if os.path.exists(os.path.join(gen, "report.json")):
            with open(os.path.join(gen, "report.json"), encoding="utf-8") as f:
                rep = json.load(f)
        rec["shell_triangles"] = rep.get("triangles_total", 0)
        rec["osm_furniture"] = count_in(tables["furniture"], c["bbox_m"])
        rec["trees"] = count_in(tables["trees"], c["bbox_m"])
        rec["props_estimate"] = round(rec["osm_furniture"] * PROPS_PER_OSM_ITEM + rec["trees"])
        rec["city_gpu_ms_estimate"] = round((rec["facade_instances"] + rec["props_estimate"]) * MS_PER_INSTANCE, 2)
        out.append(rec)
    by_xy = {(round(r["bbox_m"][0]), round(r["bbox_m"][1])): r for r in out}
    size = round(cells["cell_m"])

    def worst(n):
        best = None
        for (x, y) in by_xy:
            block = [by_xy.get((x + i * size, y + j * size)) for i in range(n) for j in range(n)]
            if None in block:
                continue
            ms = sum(b["city_gpu_ms_estimate"] for b in block)
            if best is None or ms > best["city_gpu_ms_estimate"]:
                best = {"cells": [b["name"] for b in block], "city_gpu_ms_estimate": round(ms, 1),
                        "facade_instances": sum(b["facade_instances"] for b in block)}
        return best

    meta = {"zone": a.name, "calibration": {"ref_instances": REF_INSTANCES, "ref_city_ms": REF_CITY_MS},
            "budget_ms": BUDGET_MS, "cells": out, "worst_2x2": worst(2), "worst_3x3": worst(3),
            "total_facade_instances": sum(r["facade_instances"] for r in out)}
    with open(os.path.join(proc, "cells_budget.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    print("| cell | buildings | facade inst. | signs | props (est.) | city GPU ms (est.) |")
    print("|---|---|---|---|---|---|")
    for r in sorted(out, key=lambda r: -r["city_gpu_ms_estimate"]):
        if r["buildings"]:
            print("| %s | %d | %d | %d | %d | %.1f |" % (r["name"], r["buildings"], r["facade_instances"], r["signs"],
                                                       r["props_estimate"], r["city_gpu_ms_estimate"]))
    print("\nworst 2x2:", json.dumps(meta["worst_2x2"]))
    print("worst 3x3:", json.dumps(meta["worst_3x3"]))


if __name__ == "__main__":
    main()
