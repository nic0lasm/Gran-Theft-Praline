"""Build the whole streamed quarter in the Unreal Editor without a screen, on the Mac or on the
Windows PC (ADR-0010; 0.9 = test build of the complete quarter).

One portable driver (plain Python 3, no bash): every editor step runs headless (-nullrhi)
through Content/Python/gtp_run.py, with its log in GranTheftPraline/Saved/Logs (-abslog), a
time limit, a done marker (the step's report, or the file gtp_run writes as the script
returns) and a forced stop of the editor process tree once the work is done (the editor can
hang on exit). Time-stamped lines (Paris time) and a progress line every minute.

Steps:
  data      tools/city/cells.py: the 30 cells (buildings, facade modules, signs and atlases,
            landmarks, ground masks, street furniture), then the far proxies, the driving lanes
            of the quarter and the weight and memory budget per cell (needs numpy, scipy,
            shapely, PIL: --python); skipped with --skip-data when assets/geo is already there
  lanes     installs gen/quarter/traffic/lanes.csv as Content/City/Traffic/L_City_Quarter.lanes.csv
  quarter   persistent level L_City_Quarter (import_city.py and apply_city_materials.py,
            GTP_CITY_LEVEL=quarter)
  cells     per cell: import_city, place_facades (street kit reused), place_props,
            apply_city_materials (GTP_CITY_LEVEL=cell) -> L_City_Cell<X><Y>
Materials are created with the Nanite and instanced usage flags (apply_city_materials.py).
The facade kit and the street materials must already be in the project (one street run, as
on the Mac; the PC has the Mac's Content).

Usage (repository root):
  python3 tools/city/quarter_build.py [--cells all|street|cell-p0000-p0000,...] [--skip-data]
      [--python <venv python>] [--editor <UnrealEditor or UnrealEditor-Cmd.exe>] [--dry-run]
  --dry-run replaces the editor by a stand-in that writes the expected report, so the whole
  sequence, the time limits and the forced stop are tried in seconds without Unreal
  (GTP_DRY_SLOW=<step name, e.g. quarter_import_city> makes that step hang).
Report: GranTheftPraline/Saved/CityImport/quarter_build.json (steps, seconds, result).
Exit 0 when every step passed, 1 at the first failed step (the rest is not run), 3 on a
time limit.
"""

import argparse
import datetime
import json
import os
import shutil
import signal
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
PROJECT_DIR = os.path.join(REPO, "GranTheftPraline")
PROJECT = os.path.join(PROJECT_DIR, "GranTheftPraline.uproject")
SAVED = os.path.join(PROJECT_DIR, "Saved", "CityImport")
LOGS = os.path.join(PROJECT_DIR, "Saved", "Logs")
UNREAL_PY = os.path.join(REPO, "tools", "city", "unreal")
WINDOWS = os.name == "nt"
# (script, report it writes last, time limit in s): about twice the time measured on the M1.
CELL_STEPS = (("import_city", "report.json", 600), ("place_facades", "facades_report.json", 1200),
              ("place_props", "props_report.json", 300), ("apply_city_materials", "materials_report.json", 360))
QUARTER_STEPS = (("import_city", "report.json", 600), ("apply_city_materials", "materials_report.json", 360))
EXIT_GRACE_S = 20
BEAT_S = 60


def stamp(msg):
    try:
        from zoneinfo import ZoneInfo
        now = datetime.datetime.now(ZoneInfo("Europe/Paris"))
    except Exception:  # no tz database (bare Windows Python): UTC, said so
        now = datetime.datetime.now(datetime.timezone.utc)
    print("== %s %s" % (now.strftime("%H:%M:%S %Z"), msg), flush=True)


def default_editor():
    root = os.environ.get("UE_ROOT")
    if WINDOWS:
        root = root or r"C:\Program Files\Epic Games\UE_5.8"
        return os.path.join(root, "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe")
    root = root or "/Users/Shared/Epic Games/UE_5.8"
    return os.path.join(root, "Engine", "Binaries", "Mac", "UnrealEditor.app", "Contents", "MacOS", "UnrealEditor")


def kill_tree(proc):
    if proc.poll() is not None:
        return
    if WINDOWS:
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(proc.pid)], capture_output=True)
    else:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except OSError:
            proc.kill()
    try:
        proc.wait(10)
    except subprocess.TimeoutExpired:
        pass


def other_editors():
    if WINDOWS:
        out = subprocess.run(["tasklist", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
        return [line for line in out.splitlines() if "UnrealEditor" in line and "CrashReport" not in line]
    out = subprocess.run(["ps", "-axo", "pid=,command="], capture_output=True, text=True).stdout
    return [line for line in out.splitlines()
            if ("UnrealEditor " in line or line.rstrip().endswith("UnrealEditor")) and "CrashReport" not in line
            and "UnrealEditorServices" not in line]


def fake_editor_cmd():
    """Stand-in for --dry-run: writes the report the real script would, then hangs like the
    editor on exit, so the done marker and the forced stop are exercised."""
    code = ("import json,os,time;p=os.environ['GTP_DONE_FILE'];os.makedirs(os.path.dirname(p),exist_ok=True);"
            "time.sleep(1);json.dump({'errors':[]},open(p,'w'));"
            "open(os.environ['GTP_LOG'],'a').write('Shutdown handler: cleanup\\n');time.sleep(3600)")
    return [sys.executable, "-c", code]


def run_editor(name, script, report, limit_s, env_extra, args):
    """One headless editor launch. Returns (ok, seconds, note)."""
    os.makedirs(LOGS, exist_ok=True)
    os.makedirs(SAVED, exist_ok=True)
    log = os.path.join(LOGS, "Quarter_%s.log" % name)
    marker = os.path.join(LOGS, "Quarter_%s.done" % name)
    done_file = os.path.join(SAVED, report)
    for p in (log, marker, done_file):
        if os.path.exists(p):
            os.remove(p)
    env = dict(os.environ, GTP_PY_SCRIPT=os.path.join(UNREAL_PY, script + ".py"), GTP_PY_DONE=marker,
               GTP_DONE_FILE=done_file, GTP_LOG=log, **env_extra)
    if args.dry_run and os.environ.get("GTP_DRY_SLOW") == name:   # try the time limit
        cmd, limit_s = [sys.executable, "-c", "import time; time.sleep(3600)"], 3
    elif args.dry_run:
        cmd = fake_editor_cmd()
    else:
        cmd = [args.editor, PROJECT, "-nullrhi", "-unattended", "-nosplash", "-nosound", "-NSAppSleepDisabled", "YES",
               "-abslog=" + log, "-ExecutePythonScript=__import__('gtp_run').main()"]
    t0 = time.time()
    stamp("start %s (limit %d s, log Saved/Logs/%s)" % (name, limit_s, os.path.basename(log)))
    kw = {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP} if WINDOWS else {"start_new_session": True}
    proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kw)
    done_at, next_beat = None, t0 + BEAT_S
    grace = 1 if args.dry_run else EXIT_GRACE_S
    while True:
        rc = proc.poll()
        now = time.time()
        finished = any(os.path.exists(p) and os.path.getmtime(p) >= t0 for p in (done_file, marker))
        if finished and done_at is None:
            done_at = now
            stamp("%s: work done, editor gets %d s to exit" % (name, grace))
        if rc is not None:
            break
        if done_at is not None and now - done_at > grace:
            kill_tree(proc)
            stamp("%s: editor did not exit, stopped (counted as success)" % name)
            break
        if now - t0 > limit_s:
            kill_tree(proc)
            stamp("TIME LIMIT %s after %d s" % (name, now - t0))
            return False, round(now - t0), "time limit"
        if now >= next_beat:
            size = os.path.getsize(log) if os.path.exists(log) else 0
            stamp("running %s, %d s, log %d bytes" % (name, now - t0, size))
            next_beat = now + BEAT_S
        time.sleep(0.5 if args.dry_run else 2)
    secs = round(time.time() - t0)
    if not os.path.exists(done_file):
        return False, secs, "no %s (see the log)" % report
    with open(done_file, encoding="utf-8") as f:
        errors = json.load(f).get("errors", [])
    if errors:
        return False, secs, "errors: %s" % (errors[:3],)
    stamp("end %s, %d s" % (name, secs))
    return True, secs, ""


def camel(slug):
    return "".join(p.capitalize() for p in slug.split("-"))


def main():  # noqa: C901
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--zone", default="presqu-ile")
    ap.add_argument("--cells", default="all", help="all, street (within 60 m of the rue de la Republique) or a list")
    ap.add_argument("--skip-data", action="store_true")
    ap.add_argument("--python", default=sys.executable, help="Python with numpy, scipy, shapely, PIL for the data step")
    ap.add_argument("--editor", default=default_editor())
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    t0 = time.time()
    result = {"zone": args.zone, "dry_run": args.dry_run, "steps": [], "ok": False}

    def finish(code):
        result["seconds"] = round(time.time() - t0)
        os.makedirs(SAVED, exist_ok=True)
        with open(os.path.join(SAVED, "quarter_build.json"), "w", encoding="utf-8") as f:
            json.dump(result, f, indent=1)
        stamp("quarter build %s in %d min, report Saved/CityImport/quarter_build.json"
              % ("OK" if code == 0 else "STOPPED", result["seconds"] // 60))
        sys.exit(code)

    if not args.dry_run:
        if not os.path.exists(args.editor):
            stamp("Unreal Editor not found: %s (--editor or UE_ROOT)" % args.editor)
            sys.exit(1)
        if other_editors():
            stamp("an Unreal editor is already running: one at a time")
            sys.exit(1)
    proc_dir = os.path.join(REPO, "assets", "geo", "processed", args.zone)
    if not args.skip_data:
        stamp("data: cells, proxies, lanes, budget")
        s = time.time()
        r = subprocess.run([args.python, os.path.join(HERE, "cells.py"), "--name", args.zone, "--jobs", "6"],
                           capture_output=True, text=True, timeout=3600)
        result["steps"].append({"step": "data", "ok": r.returncode == 0, "seconds": round(time.time() - s)})
        if r.returncode != 0:
            stamp("data failed: " + (r.stderr or r.stdout)[-800:])
            finish(1)
    lanes = os.path.join(proc_dir, "gen", "quarter", "traffic", "lanes.csv")
    if os.path.exists(lanes):
        dest = os.path.join(PROJECT_DIR, "Content", "City", "Traffic", "L_City_Quarter.lanes.csv")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        shutil.copyfile(lanes, dest)
        result["lanes"] = dest
    if args.cells == "all":
        sys.path.insert(0, HERE)
        import cells as cellgrid
        names = [c["name"] for c in cellgrid.grid()]
        names = [n for n in names if os.path.exists(os.path.join(proc_dir, "gen", n, "buildings_gen.csv"))]
    elif args.cells == "street":
        names = subprocess.run([args.python, os.path.join(HERE, "cell_proxies.py"), "--name", args.zone,
                                "--near-street", "rue-de-la-republique", "--within", "60"],
                               capture_output=True, text=True, check=True).stdout.split()
    else:
        names = [n.strip() for n in args.cells.split(",") if n.strip()]
    result["cells"] = names
    stamp("%d cells: %s" % (len(names), " ".join(names)))
    common = {"GTP_CITY_ZONE": args.zone, "GTP_SHOP_GLASS": os.environ.get("GTP_SHOP_GLASS", "opaque")}
    jobs = [("quarter_" + s, s, rep, lim, dict(common, GTP_CITY_FOCUS="quarter", GTP_CITY_LEVEL="quarter"))
            for s, rep, lim in QUARTER_STEPS]
    for c in names:
        env = dict(common, GTP_CITY_FOCUS=c, GTP_CITY_LEVEL="cell", GTP_KIT_FOCUS="rue-de-la-republique",
                   GTP_KIT_REUSE="1")
        jobs += [("%s_%s" % (c, s), s, rep, lim, env) for s, rep, lim in CELL_STEPS
                 if s != "place_props" or os.path.exists(os.path.join(proc_dir, "gen", c, "props_placement.csv"))]
    for i, (name, script, rep, lim, env) in enumerate(jobs):
        stamp("step %d/%d" % (i + 1, len(jobs)))
        ok, secs, note = run_editor(name, script, rep, lim, env, args)
        result["steps"].append({"step": name, "ok": ok, "seconds": secs, "note": note})
        if not ok:
            stamp("%s failed: %s" % (name, note))
            finish(3 if note == "time limit" else 1)
    result["ok"] = True
    result["levels"] = ["/Game/Maps/L_City_Quarter"] + ["/Game/Maps/L_City_" + camel(c) for c in names]
    finish(0)


if __name__ == "__main__":
    main()
