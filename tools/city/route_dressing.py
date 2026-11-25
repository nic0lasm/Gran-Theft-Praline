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
              the scanned stone. Map not saved (GTP_CITY_EV100=off). Before it, ground_mask.py
              --far makes the far mask and the rivers the ground reads past the focus mask
  place       place_props.py with GTP_PROPS_SET=route: folder City/Route, the street's own
              City/Props untouched; saves the map
  street      (only when named in --only) place_props.py on the street's own set, City/Props,
              from its props_placement.csv: the same breakable tags there (glass containers,
              gate 68); saves the map
  facades     fix_facade_flips.py: a facade kit module facing the other way from its neighbours
              on the same facade line is turned round on its pivot (gate 75: a shop module turned
              round stood its room on lane 13, the white block); saves the map
  lanes       clear_lane_instances.py: every street object a car can hit within 1.5 m of a lane
              of the lanes file the game reads is taken out (street sets only; the facade kit
              and the signs are only reported); saves the map
  pits        cover_pits.py: the closed pits of the LiDAR terrain (car park ramps, underpass
              mouths: gate 76, the player fell into one) get a flat lid at street height; the
              heights are read from terrain.glb or the tile meshes, never traced; fails when
              none is read; saves the map
  collision   backdrop_collision.py: complex-as-simple collision and BlockAll on the backdrop
              volumes (gate 65: the player walked into one, the camera stayed inside, black);
              saves the map. GTP_BACKDROP_COLLISION=0 leaves them as they are
  horizon     place_horizon.py: the far view of tools/city/horizon.py in the folder City/Horizon
              (a ground to the horizon, far blocks, the Part-Dieu towers, two hills, Fourviere), with the
              materials of the materials step; saves the map. GTP_HORIZON=0 takes it out again
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
STEPS = ("props", "backup", "surfaces", "materials", "place", "street", "facades", "lanes", "pits", "collision", "horizon", "lights")
OPT_IN = ("street",)


def md5(path):
    h = hashlib.md5()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def ground_masks():
    """The masks the ground material reads, made from the data of this machine before the
    materials step: the focus mask again (lane lines; the one of the PC dated from the making of
    the city), the far mask, the route mask. Returns the note of the step: each exit code, and
    what the masks held under the driven line before and after (ground_mask.py --probe). Never
    stops the step: a mask that is not made again stays as it was."""
    tool = [sys.executable, os.path.join(HERE, "ground_mask.py"), "--focus", FOCUS]
    circuit = os.path.join(qb.PROJECT_DIR, "Content", "Test", "Circuits", "L_City_RueDeLaRepublique.circuit.csv")
    notes = []

    def probe(label):
        try:
            out = subprocess.run(tool + ["--probe", circuit], capture_output=True, text=True, timeout=300).stdout
            seen = json.loads(out.strip().splitlines()[-1])
            notes.append("%s: %s" % (label, "; ".join(
                "%s %s of %s px, %d points: asphalt %d, marking %d, paving %d, kerb %d, sidewalk %d"
                % (k, v["extent_ue_cm"], v["size_px"], v["points"], v["asphalt"], v["marking"], v["paving"],
                   v["kerb"], v["sidewalk"]) for k, v in seen.items())))
        except Exception as e:  # noqa: BLE001
            notes.append("%s: %r" % (label, e))

    has_circuit = os.path.exists(circuit)
    if has_circuit:
        probe("under the driven line before")
    for label, extra in (("focus mask", []), ("far mask", ["--far"]),
                         ("route mask", ["--route", circuit] if has_circuit else None)):
        if extra is None:
            continue
        try:
            rc = subprocess.call(tool + extra, stdout=subprocess.DEVNULL, timeout=600)
            notes.append("%s: ground_mask.py %s exit %d" % (label, " ".join(extra[:1]) or "(focus)", rc))
        except Exception as e:  # noqa: BLE001
            notes.append("%s: %r" % (label, e))
    if has_circuit:
        probe("after")
    return ", ".join(notes)


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
            # the far mask and the rivers the ground material reads past the focus mask: made
            # here from the data of this machine; without it the ground is as it was
            far = "masks: dry run"
            if not args.dry_run:
                far = ground_masks()
            ok, _, note = qb.run_editor("route_materials", "apply_city_materials", "materials_report.json", 900,
                                        {"GTP_CITY_EV100": "off"}, args)
            note = (note + " " + far).strip()
        elif step == "place":
            ok, _, note = qb.run_editor("route_place", "place_props", "route_report.json", 900,
                                        {"GTP_PROPS_SET": "route"}, args)
        elif step == "street":
            ok, _, note = qb.run_editor("route_street", "place_props", "props_report.json", 900,
                                        {"GTP_PROPS_SET": "props"}, args)
        elif step == "facades":
            ok, _, note = qb.run_editor("route_facades", "fix_facade_flips", "facade_flip_report.json", 900, {}, args)
            rep = os.path.join(qb.SAVED, "facade_flip_report.json")
            if os.path.exists(rep):
                with open(rep, encoding="utf-8") as f:
                    r = json.load(f)
                note += " modules %s, flipped %s %s" % (r.get("modules"), r.get("flipped"), r.get("by_mesh"))
        elif step == "lanes":
            ok, _, note = qb.run_editor("route_lanes", "clear_lane_instances", "lane_clear_report.json", 900,
                                        {}, args)
            rep = os.path.join(qb.SAVED, "lane_clear_report.json")
            if os.path.exists(rep):
                with open(rep, encoding="utf-8") as f:
                    r = json.load(f)
                note += " removed %s %s, of which %s within %s cm of the %s driven lines, reported only %s" % (
                    r.get("removed_total"), r.get("removed"), r.get("circuit_removed"), r.get("circuit_clear_cm"),
                    r.get("circuit_lines"), r.get("reported_only_total"))
                note += ", plain actors removed %s" % [e.get("label") for e in r.get("removed_actors") or []]
        elif step == "pits":
            ok, _, note = qb.run_editor("route_pits", "cover_pits", "pit_cover_report.json", 2400, {}, args)
            rep = os.path.join(qb.SAVED, "pit_cover_report.json")
            if os.path.exists(rep):
                with open(rep, encoding="utf-8") as f:
                    r = json.load(f)
                note += " heights read %s, covered %s, lids %s" % (r.get("cells_read"), r.get("covered"), r.get("lids"))
        elif step == "collision":
            ok, _, note = qb.run_editor("route_collision", "backdrop_collision", "backdrop_collision_report.json", 900,
                                        {}, args)
            rep = os.path.join(qb.SAVED, "backdrop_collision_report.json")
            if os.path.exists(rep):
                with open(rep, encoding="utf-8") as f:
                    r = json.load(f)
                w = r.get("walls_without_collision") or {}
                note += " backdrop components made solid %s; standing without collision %s, %s near the driven line" % (
                    r.get("components"), w.get("by_folder"), w.get("near_total"))
        elif step == "horizon":
            ok, _, note = qb.run_editor("route_horizon", "place_horizon", "horizon_report.json", 900, {}, args)
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
