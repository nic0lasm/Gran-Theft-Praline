"""Dress what the camera sees during the car theft and the flight, on the street map, replayable.

The flight (gate 59 video) leaves the 45 m of the rue de la Republique: quai Jean Moulin and the
streets east of it are backdrop volumes (backdrop.py) with no facade kit, no furniture and, in the
project, none of the scanned surfaces (they were exported to downloads-free, never imported).
One portable driver (plain Python 3, Windows or Mac), one headless editor at a time, like
quarter_build.py and surfaces_build.py:

  props       props.py --set route --ring 45,600 (needs assets/geo): every surveyed item between
              45 m and 600 m of the street (lamps, benches, bins, bike racks, shelters, hydrants,
              tree grates with their trees, swapped to the leafy alder at run time), written to
              gen/<focus>/route_placement.csv. --route-csv <file>: use that table instead (its
              tree rows, bare trunks in tables made before gate 65, are given the leafy alder).
  backup      copy of the map files to Saved/CityImport/route_map_backup (md5 in the report)
  surfaces    import_surfaces.py with GTP_SURFACES_DIR=<--loose>: the Megascans exports found by
              tools/city/surfaces.json, imported to /Game/Environment/Surfaces/<id>/ (2K)
  materials   apply_city_materials.py: the scanned surfaces in the ground, kit and shell materials,
              and the procedural Lyon facade (M_City_Backdrop) on the backdrop volumes, laid over
              the scanned stone. Map not saved (GTP_CITY_EV100=off).
  place       place_props.py with GTP_PROPS_SET=route: folder City/Route, the street's own
              City/Props untouched; saves the map
  street      (only when named in --only) place_props.py on the street's own set, City/Props,
              from its props_placement.csv: the same breakable tags there (glass containers,
              gate 68); saves the map
  lanes       clear_lane_instances.py: every placed object a car can hit within 1.5 m of a lane of
              the lanes file the game reads is taken out (street sets and gate 75's named
              actors; the facade kit only reported); saves the map
  collision   backdrop_collision.py: complex-as-simple collision and BlockAll on the backdrop
              volumes (gate 65: the player walked into one, the camera stayed inside, black);
              saves the map. GTP_BACKDROP_COLLISION=0 leaves them as they are
  lights      build_night_lights.py: the route lamps light at night too

Usage (repository root, no editor open):
  python tools/city/route_dressing.py --loose "<downloads-free folder>" [--route-csv <csv>]
         [--only props,surfaces,...] [--editor <exe>] [--dry-run]
Report: GranTheftPraline/Saved/CityImport/route_dressing.json. Exit 0 when every step passed,
1 at the first failed step, 3 on a time limit. Undo: GTP_BACKDROP_FACADES=0 and GTP_SURFACES=0 on
apply_city_materials, and the map back from the backup.
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import quarter_build as qb  # noqa: E402
import route_trees  # noqa: E402

FOCUS = "rue-de-la-republique"
MAP_DIR = os.path.join(qb.PROJECT_DIR, "Content", "Maps")
MAP_FILES = [os.path.join(MAP_DIR, "L_City_RueDeLaRepublique" + ext) for ext in (".umap", "_BuiltData.uasset")]
BACKUP = os.path.join(qb.SAVED, "route_map_backup")
STEPS = ("props", "backup", "surfaces", "materials", "place", "street", "lanes", "collision", "lights")
OPT_IN = ("street",)


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def main():  # noqa: C901
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--loose", help="folder of the Fab/Megascans exports (downloads-free)")
    ap.add_argument("--route-csv", help="a ready route_placement.csv (made elsewhere)")
    ap.add_argument("--ring", default="45,600")
    ap.add_argument("--only", help="comma list of steps (default: all)")
    ap.add_argument("--editor", default=qb.default_editor())
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    only = args.only.split(",") if args.only else None
    steps = [s for s in STEPS if (s in only if only else s not in OPT_IN)]
    gen = os.path.join(qb.REPO, "assets", "geo", "processed", "presqu-ile", "gen", FOCUS)
    report = {"steps": [], "started": time.strftime("%Y-%m-%d %H:%M:%S")}
    out = os.path.join(qb.SAVED, "route_dressing.json")
    os.makedirs(qb.SAVED, exist_ok=True)

    def finish(code):
        report["result"] = code
        with open(out, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=1)
        qb.stamp("report %s, exit %d" % (out, code))
        sys.exit(code)

    if "surfaces" in steps and not args.loose:
        qb.stamp("no --loose folder: the surfaces step is skipped")
        steps.remove("surfaces")
    if not args.dry_run and qb.other_editors():
        qb.stamp("an Unreal editor is already running: one at a time, stop")
        finish(1)
    for step in steps:
        t0 = time.time()
        ok, note = True, ""
        if step == "props":
            dest = os.path.join(gen, "route_placement.csv")
            if args.route_csv:
                os.makedirs(gen, exist_ok=True)
                if os.path.abspath(args.route_csv) != os.path.abspath(dest):
                    shutil.copyfile(args.route_csv, dest)
                note = "from " + args.route_csv
                # a table made before gate 65's fix still plants bare trunks: leafy alder instead
                note += ", %d tree rows made leafy" % route_trees.retree_csv(dest)
            else:
                rc = subprocess.call([sys.executable, os.path.join(HERE, "props.py"), "--name", "presqu-ile",
                                      "--focus", FOCUS, "--set", "route", "--ring", args.ring])
                ok, note = rc == 0, "props.py exit %d" % rc
            if ok:
                with open(dest, encoding="utf-8") as f:
                    note += ", %d rows" % (sum(1 for _ in f) - 1)
        elif step == "backup":
            os.makedirs(BACKUP, exist_ok=True)
            sums = {}
            for p in MAP_FILES:
                if os.path.exists(p):
                    shutil.copy2(p, os.path.join(BACKUP, os.path.basename(p)))
                    sums[os.path.basename(p)] = md5(p)
            ok, note = bool(sums) or args.dry_run, json.dumps(sums)
        elif step == "surfaces":
            ok, _, note = qb.run_editor("route_surfaces", "import_surfaces", "surfaces.json", 1800,
                                        {"GTP_SURFACES_DIR": os.path.abspath(args.loose)}, args)
        elif step == "materials":
            ok, _, note = qb.run_editor("route_materials", "apply_city_materials", "materials_report.json", 900,
                                        {"GTP_CITY_EV100": "off"}, args)
        elif step == "place":
            ok, _, note = qb.run_editor("route_place", "place_props", "route_report.json", 900,
                                        {"GTP_PROPS_SET": "route"}, args)
        elif step == "street":
            ok, _, note = qb.run_editor("route_street", "place_props", "props_report.json", 900,
                                        {"GTP_PROPS_SET": "props"}, args)
        elif step == "lanes":
            ok, _, note = qb.run_editor("route_lanes", "clear_lane_instances", "lane_clear_report.json", 900,
                                        {}, args)
            rep = os.path.join(qb.SAVED, "lane_clear_report.json")
            if os.path.exists(rep):
                with open(rep, encoding="utf-8") as f:
                    r = json.load(f)
                note += " removed %s %s, reported only %s" % (r.get("removed_total"), r.get("removed"),
                                                              r.get("reported_only_total"))
        elif step == "collision":
            ok, _, note = qb.run_editor("route_collision", "backdrop_collision", "backdrop_collision_report.json", 900,
                                        {}, args)
        elif step == "lights":
            # every generated focus with facade modules, as setup-environment-win.ps1 does
            root = os.path.dirname(gen)
            gens = sorted(os.path.join(root, d) for d in os.listdir(root)
                          if os.path.exists(os.path.join(root, d, "facade_modules.csv")))
            rc = subprocess.call([sys.executable, os.path.join(qb.REPO, "tools", "environment", "build_night_lights.py")]
                                 + gens)
            ok, note = rc == 0, "build_night_lights exit %d" % rc
        report["steps"].append({"step": step, "ok": ok, "seconds": round(time.time() - t0), "note": note})
        qb.stamp("%s: %s %s" % (step, "ok" if ok else "FAILED", note))
        if not ok:
            finish(3 if note == "time limit" else 1)
    finish(0)


if __name__ == "__main__":
    main()
