"""Cut the quarter (Bellecour -> Terreaux, both quays) into streaming cells and generate
each one outside the engine: buildings and facade modules (generate.py --cell), shop
signs (signs.py) and the ground mask (ground_mask.py, carriageways fitted to the kerbs).

A cell is a square of CELL_M on a grid anchored at the zone origin, so cell names never
change when the quarter grows. Each cell is written to processed/<zone>/gen/<cell>/ in the
same layout as a street (gen/rue-de-la-republique/), so the Unreal steps can import one
cell into its own level (ADR-0010). The cell list and per-cell counts go to
processed/<zone>/cells.json.

Usage: python3 tools/city/cells.py --name presqu-ile [--only cell-p0250-m0500 ...] [--jobs 4]
"""

import argparse
import concurrent.futures as cf
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

CELL_M = 250.0
# Bellecour -> Terreaux with the Saone and Rhone quays and the Lafayette bridge head
# (local metres, x east, y north); measured on the BD TOPO roads of the zone.
QUARTER = (-350.0, -750.0, 750.0, 600.0)


def cell_name(x0, y0):
    def f(v):
        return ("p" if v >= 0 else "m") + "%04d" % abs(int(round(v)))
    return "cell-%s-%s" % (f(x0), f(y0))   # already a generate.py slug


def grid(bounds=QUARTER, size=CELL_M):
    x0, y0, x1, y1 = bounds
    out = []
    gx0 = size * (x0 // size)
    gy0 = size * (y0 // size)
    x = gx0
    while x < x1:
        y = gy0
        while y < y1:
            out.append({"name": cell_name(x, y), "bbox_m": [x, y, x + size, y + size]})
            y += size
        x += size
    return out


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("%s failed: %s" % (" ".join(cmd[:3]), (r.stderr or r.stdout)[-600:]))
    return r.stdout


def make_cell(zone, cell, py):
    t0 = time.time()
    bbox = ",".join("%g" % v for v in cell["bbox_m"])
    run([py, os.path.join(HERE, "generate.py"), "--name", zone, "--focus", cell["name"], "--cell=" + bbox])
    gen = os.path.join(REPO, "assets", "geo", "processed", zone, "gen", cell["name"])
    with open(os.path.join(gen, "report.json"), encoding="utf-8") as f:
        rep = json.load(f)
    out = dict(cell)
    out["buildings"] = rep.get("bdtopo_buildings")
    out["parts"] = rep.get("building_parts")
    out["shops_with_sign"] = rep.get("shops_with_sign")
    out["modules"] = rep.get("modules")
    if out["buildings"]:
        run([py, os.path.join(HERE, "signs.py"), "--name", zone, "--focus", cell["name"]])
        run([py, os.path.join(HERE, "ground_mask.py"), "--zone", zone, "--focus", cell["name"]])
    out["seconds"] = round(time.time() - t0, 1)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--only", action="append", default=[])
    ap.add_argument("--jobs", type=int, default=4)
    a = ap.parse_args()
    py = sys.executable
    cells = [c for c in grid() if not a.only or c["name"] in a.only]
    run([py, os.path.join(HERE, "carriageway.py"), "--name", a.name])
    done, errors = [], []
    with cf.ProcessPoolExecutor(max_workers=a.jobs) as ex:
        futs = {ex.submit(make_cell, a.name, c, py): c for c in cells}
        for fut in cf.as_completed(futs):
            try:
                done.append(fut.result())
            except Exception as e:  # noqa: BLE001  keep the other cells going
                errors.append({"cell": futs[fut]["name"], "error": str(e)})
    done.sort(key=lambda c: c["name"])
    meta = {"zone": a.name, "cell_m": CELL_M, "quarter_bbox_m": QUARTER, "cells": done, "errors": errors}
    path = os.path.join(REPO, "assets", "geo", "processed", a.name, "cells.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    print(json.dumps({"cells": len(done), "errors": errors,
                      "buildings": sum(c.get("buildings") or 0 for c in done)}, indent=2))


if __name__ == "__main__":
    main()
