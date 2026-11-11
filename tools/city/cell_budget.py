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

Memory (ADR-0010 condition 4: <= 1.5 GB for 4 detailed cells and every proxy) is estimated
the same way, from sizes rather than a measure (memreport on the packaged quarter replaces
it): shell triangles with their complex collision, facade and prop instances (HISM transform,
custom data and render copy), the sign atlas pages of the cell (4096 px, BC1 with mips) and
one actor per building part. The kit, the prop meshes and the proxies are shared: counted
once, in "shared_mb".

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
# Memory estimate (bytes), see the docstring.
B_PER_SHELL_TRI = 108 + 50            # 3 unshared vertices x 32 B + 12 B of indices, + cooked collision
B_PER_INSTANCE = 200                  # HISM: transform, custom data, cluster tree, render copy
B_PER_ATLAS_PAGE = 4096 * 4096 // 2 * 4 // 3   # BC1, full mip chain
B_PER_ACTOR = 2048
B_PER_PROXY_TRI = 108
BUDGET_MB = 1536.0                    # 4 detailed cells + all proxies
KIT_SHARED_MB = 120.0                 # facade kit with LODs, its materials and the street prop meshes (order of magnitude)


GROUND_KINDS = {"shop", "shopfront_closed", "entrance", "gf_window", "entresol_window", "monument_bay"}
FACADE_CULL_M = {"ground": 250.0, "upper": 250.0, "eave": 250.0, "court": 250.0}   # place_facades.py defaults
PROP_CULL_M = 60.0                    # props: mostly small (30 m) and medium (60 m), place_props.py
# Budget levers, as switches: generation (GTP_FACADE_SKIP) or run time (-CityCull, -CityCellMax).
SCENARIOS = (
    ("as generated", {}, {}, 4),
    ("-CityCull=court=60", {"court": 60.0}, {}, 4),
    ("GTP_FACADE_SKIP=court", {"court": 0.0}, {}, 4),
    ("-CityCull=court=60,upper=180,eave=180", {"court": 60.0, "upper": 180.0, "eave": 180.0}, {}, 4),
    ("-CityCull=court=60,upper=150,eave=150,ground=150", {"court": 60.0, "upper": 150.0, "eave": 150.0,
                                                          "ground": 150.0}, {}, 4),
    ("above + -CityCull=props_small=20,props_medium=40", {"court": 60.0, "upper": 150.0, "eave": 150.0,
                                                          "ground": 150.0}, {"props": 35.0}, 4),
    ("-CityCellMax=2 (2 detailed cells, the rest proxies)", {}, {}, 2),
)


def visible_share(radius_m, half_m=250.0, step=5.0):
    """Share of a 2 x 2 block (player at its centre) within radius_m: what a cull distance keeps."""
    if radius_m <= 0:
        return 0.0
    n = int(2 * half_m / step)
    inside = sum(1 for i in range(n) for j in range(n)
                 if ((i + 0.5) * step - half_m) ** 2 + ((j + 0.5) * step - half_m) ** 2 <= radius_m ** 2)
    return inside / float(n * n)


def family_counts(gen):
    """Facade instances of a cell per cull family (place_facades.py module_family)."""
    out = {"ground": 0, "upper": 0, "eave": 0, "court": 0}
    path = os.path.join(gen, "facade_modules.csv")
    if not os.path.exists(path):
        return out
    with open(path, encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    top = {}
    for r in rows:
        top[r["building_id"]] = max(top.get(r["building_id"], 0), int(r["storey"]))
    for r in rows:
        if r["side"] == "court":
            out["court"] += 1
        elif r["kind"] in GROUND_KINDS or r["storey"] == "0":
            out["ground"] += 1
        else:
            out["upper"] += 1
        if r["side"] == "street" and int(r["storey"]) == top[r["building_id"]]:
            out["eave"] += 1
    return out


def scenarios(block, gen_of):
    """Estimated city GPU ms of the block for each lever, relative to the linear estimate:
    each class of instances weighted by the share of the block its cull distance keeps."""
    fam = {"ground": 0, "upper": 0, "eave": 0, "court": 0}
    props = 0
    for b in block["records"]:
        for k, v in family_counts(gen_of(b["name"])).items():
            fam[k] += v
        props += b.get("props_placed", b["props_estimate"])
    base_vis = sum(n * visible_share(FACADE_CULL_M[k]) for k, n in fam.items()) + props * visible_share(PROP_CULL_M)
    out = []
    for label, cull, pcull, cells in SCENARIOS:
        vis = sum(n * visible_share(cull.get(k, FACADE_CULL_M[k])) for k, n in fam.items()) \
            + props * visible_share(pcull.get("props", PROP_CULL_M))
        vis *= cells / 4.0
        out.append({"lever": label, "city_gpu_ms_estimate": round(block["city_gpu_ms_estimate"] * vis / base_vis, 1)})
    return {"families": fam, "props": props, "scenarios": out}


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
        props_csv = os.path.join(gen, "props_placement.csv")
        if os.path.exists(props_csv):
            with open(props_csv, encoding="utf-8") as f:
                rec["props_placed"] = sum(1 for _ in csv.DictReader(f))
        pages = 0
        atlas = os.path.join(gen, "signs", "sign_atlas.json")
        if os.path.exists(atlas):
            with open(atlas, encoding="utf-8") as f:
                at = json.load(f)
            pages = at.get("pages", 0) + at.get("logo_pages", 0)
        rec["sign_atlas_pages"] = pages
        mem = (rec["shell_triangles"] * B_PER_SHELL_TRI
               + (rec["facade_instances"] + rec.get("props_placed", rec["props_estimate"])) * B_PER_INSTANCE
               + pages * B_PER_ATLAS_PAGE + ((c.get("parts") or 0) + 50) * B_PER_ACTOR)
        rec["memory_mb_estimate"] = round(mem / 2 ** 20, 1)
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
                best = {"cells": [b["name"] for b in block], "records": block, "city_gpu_ms_estimate": round(ms, 1),
                        "facade_instances": sum(b["facade_instances"] for b in block),
                        "memory_mb_estimate": round(sum(b["memory_mb_estimate"] for b in block), 1)}
        return best

    proxies = os.path.join(proc, "gen", "quarter", "cell_proxies.json")
    proxy_tris = 0
    if os.path.exists(proxies):
        with open(proxies, encoding="utf-8") as f:
            proxy_tris = sum(n["triangles"] for n in json.load(f)["nodes"])
    shared_mb = round(proxy_tris * B_PER_PROXY_TRI / 2 ** 20 + KIT_SHARED_MB, 1)
    w2 = worst(2)
    if w2:
        w2["levers"] = scenarios(w2, lambda name: os.path.join(proc, "gen", name))
    w3 = worst(3)
    for w in (w2, w3):
        if w:
            w.pop("records", None)
    memory = {"shared_mb": shared_mb, "proxy_triangles": proxy_tris, "budget_mb": BUDGET_MB,
              "worst_2x2_plus_shared_mb": round((w2["memory_mb_estimate"] if w2 else 0) + shared_mb, 1)}
    memory["ok"] = memory["worst_2x2_plus_shared_mb"] <= BUDGET_MB
    meta = {"zone": a.name, "calibration": {"ref_instances": REF_INSTANCES, "ref_city_ms": REF_CITY_MS},
            "memory": memory,
            "budget_ms": BUDGET_MS, "cells": out, "worst_2x2": w2, "worst_3x3": w3,
            "total_facade_instances": sum(r["facade_instances"] for r in out)}
    with open(os.path.join(proc, "cells_budget.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    print("| cell | buildings | facade inst. | signs | props | city GPU ms (est.) | memory MB (est.) |")
    print("|---|---|---|---|---|---|---|")
    for r in sorted(out, key=lambda r: -r["city_gpu_ms_estimate"]):
        if r["buildings"]:
            print("| %s | %d | %d | %d | %d | %.1f | %.0f |" % (
                r["name"], r["buildings"], r["facade_instances"], r["signs"], r.get("props_placed", r["props_estimate"]),
                r["city_gpu_ms_estimate"], r["memory_mb_estimate"]))
    print("\nworst 2x2:", json.dumps(meta["worst_2x2"]))
    print("worst 3x3:", json.dumps(meta["worst_3x3"]))
    print("memory:", json.dumps(memory))
    if w2:
        print("\nworst 2x2 by class:", json.dumps(w2["levers"]["families"]), "props", w2["levers"]["props"])
        print("| lever | city GPU ms (est.) |\n|---|---|")
        for sc in w2["levers"]["scenarios"]:
            print("| %s | %.1f |" % (sc["lever"], sc["city_gpu_ms_estimate"]))


if __name__ == "__main__":
    main()
