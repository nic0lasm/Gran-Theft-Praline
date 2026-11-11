"""Put the scanned street surfaces into the city materials and shoot the rue de la Republique
before and after, on the Windows PC or on the Mac (portable driver, plain Python 3, no bash).

Steps (one Unreal launch each, one at a time, log in GranTheftPraline/Saved/Logs/Surfaces_<step>.log
through -abslog, a time limit, the step's report as done marker, forced stop of an editor that
hangs on exit; time-stamped lines and a progress line every minute, like quarter_build.py):
  import      tools/city/unreal/import_surfaces.py, headless: finds the surfaces of
              tools/city/surfaces.json in the project, caps them at 2K, writes
              Saved/CityImport/surfaces.json
  off         apply_city_materials.py with GTP_SURFACES=0 (procedural look, the reference), headless
  shoot_off   shoot_city.py in -game (needs a screen): stills to Saved/CityImport/surfaces_ab/off/
  on          apply_city_materials.py with the surfaces, headless
  shoot_on    stills to Saved/CityImport/surfaces_ab/on/, and side-by-side pairs in
              surfaces_ab/pairs/ when Pillow is installed
Without --ab only import and on run (the surfaces stay applied). The map is not rebuilt; the
street map, ground mask and facade kit must already be in the project.

Usage (repository root):
  python tools/city/surfaces_build.py [--ab] [--lite] [--loose <folder>] [--res 2560x1440] [--editor <exe>] [--dry-run]
  --loose: Fab exports outside the project (e.g. Claude Work/GTAL-assets/downloads-free), found by
  their listing name (match / match_any in surfaces.json) and imported to /Game/Environment/Surfaces/<id>/.
  --lite: colour maps only (GTP_SURFACES_LITE=1). --dry-run: a stand-in for Unreal writes each
  report and hangs, to try the sequence, the limits and the forced stop in seconds.
Report: Saved/CityImport/surfaces_build.json. Exit 0 when every step passed, 1 on a failed step,
3 on a time limit.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import quarter_build as qb  # noqa: E402  stamp, kill_tree, other_editors, default_editor, paths

SAVED, LOGS, PROJECT, UNREAL_PY = qb.SAVED, qb.LOGS, qb.PROJECT, qb.UNREAL_PY
AB = os.path.join(SAVED, "surfaces_ab")
FOCUS = "rue-de-la-republique"
MAP = "/Game/Maps/L_City_RueDeLaRepublique"
GRACE_S, BEAT_S = 20, 60


def fake_cmd():
    code = ("import json,os,time;p=os.environ['GTP_DONE_FILE'];os.makedirs(os.path.dirname(p),exist_ok=True);"
            "time.sleep(1);d={'errors':[]};"
            "s=os.environ.get('GTP_FAKE_SHOTS');"
            "s and [os.makedirs(os.path.join(s,'shots'),exist_ok=True) or open(os.path.join(s,'shots','0%d.png'%i),'wb').write(b'x') for i in (1,2)];"
            "json.dump(d,open(p,'w'));open(os.environ['GTP_LOG'],'a').write('Shutdown handler: cleanup\\n');time.sleep(3600)")
    return [sys.executable, "-c", code]


def launch(name, script, report, limit_s, env_extra, game, args):
    """One editor launch: headless, or -game on the street map. Returns (ok, seconds, note)."""
    os.makedirs(LOGS, exist_ok=True)
    os.makedirs(SAVED, exist_ok=True)
    log = os.path.join(LOGS, "Surfaces_%s.log" % name)
    marker = os.path.join(LOGS, "Surfaces_%s.done" % name)
    done_file = os.path.join(SAVED, report)
    for p in (log, marker, done_file):
        if os.path.exists(p):
            os.remove(p)
    env = dict(os.environ, GTP_PY_SCRIPT=os.path.join(UNREAL_PY, script + ".py"), GTP_PY_DONE=marker,
               GTP_DONE_FILE=done_file, GTP_LOG=log, GTP_CITY_FOCUS=FOCUS, **env_extra)
    if args.dry_run:
        if game:
            env["GTP_FAKE_SHOTS"] = SAVED
        cmd = fake_cmd()
    elif game:
        w, h = args.res.split("x")
        cmd = [args.editor, PROJECT, MAP, "-game", "-windowed", "-ResX=" + w, "-ResY=" + h, "-nosound",
               "-NoScreenMessages", "-unattended", "-NSAppSleepDisabled", "YES", "-abslog=" + log,
               "-ExecCmds=r.SetRes %sw, py __import__('gtp_run').main()" % args.res]
    else:
        env["GTP_NULLRHI"] = "1"
        cmd = [args.editor, PROJECT, "-nullrhi", "-unattended", "-nosplash", "-nosound", "-NSAppSleepDisabled", "YES",
               "-abslog=" + log, "-ExecutePythonScript=__import__('gtp_run').main()"]
    t0 = time.time()
    qb.stamp("start %s (limit %d s, log Saved/Logs/%s)" % (name, limit_s, os.path.basename(log)))
    kw = {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP} if qb.WINDOWS else {"start_new_session": True}
    proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kw)
    done_at, next_beat = None, t0 + BEAT_S
    grace = 1 if args.dry_run else GRACE_S
    while True:
        rc = proc.poll()
        now = time.time()
        # -game scripts return at once and work on ticks: only their report says they are done.
        markers = (done_file,) if game else (done_file, marker)
        if done_at is None and any(os.path.exists(p) and os.path.getmtime(p) >= t0 for p in markers):
            done_at = now
            qb.stamp("%s: work done, editor gets %d s to exit" % (name, grace))
        if rc is not None:
            break
        if done_at is not None and now - done_at > grace:
            qb.kill_tree(proc)
            qb.stamp("%s: editor did not exit, stopped (counted as success)" % name)
            break
        if now - t0 > limit_s:
            qb.kill_tree(proc)
            qb.stamp("TIME LIMIT %s after %d s" % (name, now - t0))
            return False, round(now - t0), "time limit"
        if now >= next_beat:
            size = os.path.getsize(log) if os.path.exists(log) else 0
            qb.stamp("running %s, %d s, log %d bytes" % (name, now - t0, size))
            next_beat = now + BEAT_S
        time.sleep(0.5 if args.dry_run else 2)
    secs = round(time.time() - t0)
    if not os.path.exists(done_file):
        return False, secs, "no %s (see the log)" % report
    with open(done_file, encoding="utf-8") as f:
        errors = json.load(f).get("errors", [])
    if errors:
        return False, secs, "errors: %s" % (errors[:3],)
    qb.stamp("end %s, %d s" % (name, secs))
    return True, secs, ""


def keep_shots(tag):
    src, dest = os.path.join(SAVED, "shots"), os.path.join(AB, tag)
    if os.path.isdir(dest):
        shutil.rmtree(dest)
    os.makedirs(dest)
    names = sorted(n for n in os.listdir(src) if n.endswith(".png")) if os.path.isdir(src) else []
    for n in names:
        shutil.copyfile(os.path.join(src, n), os.path.join(dest, n))
    return names


def pairs(names):
    """Side-by-side before | after, when Pillow is there (not needed otherwise)."""
    try:
        from PIL import Image
    except ImportError:
        return "Pillow missing: compare surfaces_ab/off and surfaces_ab/on"
    out = os.path.join(AB, "pairs")
    os.makedirs(out, exist_ok=True)
    done = 0
    for n in names:
        a, b = os.path.join(AB, "off", n), os.path.join(AB, "on", n)
        if not os.path.exists(b):
            continue
        try:
            ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
        except Exception:  # noqa: BLE001  dry-run stand-ins are not images
            continue
        im = Image.new("RGB", (ia.width + ib.width, max(ia.height, ib.height)))
        im.paste(ia, (0, 0))
        im.paste(ib, (ia.width, 0))
        im.save(os.path.join(out, n.replace(".png", ".jpg")), quality=88)
        done += 1
    return "%d pairs" % done


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--ab", action="store_true", help="shoot without, then with the surfaces")
    ap.add_argument("--lite", action="store_true")
    ap.add_argument("--loose", default=os.environ.get("GTP_SURFACES_DIR", ""),
                    help="folder of loose Fab exports (one folder per listing), imported first")
    ap.add_argument("--res", default="2560x1440")
    ap.add_argument("--editor", default=qb.default_editor())
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    t0 = time.time()
    result = {"ab": args.ab, "lite": args.lite, "dry_run": args.dry_run, "steps": [], "ok": False}

    def finish(code):
        result["seconds"] = round(time.time() - t0)
        with open(os.path.join(SAVED, "surfaces_build.json"), "w", encoding="utf-8") as f:
            json.dump(result, f, indent=1)
        qb.stamp("surfaces %s in %d min, report Saved/CityImport/surfaces_build.json"
                 % ("OK" if code == 0 else "STOPPED", result["seconds"] // 60))
        sys.exit(code)

    os.makedirs(SAVED, exist_ok=True)
    if not args.dry_run:
        if not os.path.exists(args.editor):
            qb.stamp("Unreal Editor not found: %s (--editor or UE_ROOT)" % args.editor)
            sys.exit(1)
        if qb.other_editors():
            qb.stamp("an Unreal editor is already running: one at a time")
            sys.exit(1)
    on = {"GTP_SURFACES": "1", "GTP_SURFACES_LITE": "1" if args.lite else "0"}
    off = {"GTP_SURFACES": "0"}
    steps = [("import", "import_surfaces", "surfaces.json", 480,
              {"GTP_SURFACES_DIR": os.path.abspath(args.loose)} if args.loose else {}, False)]
    if args.ab:
        steps += [("off", "apply_city_materials", "materials_report.json", 480, off, False),
                  ("shoot_off", "shoot_city", "play_report.json", 600, {}, True)]
    steps += [("on", "apply_city_materials", "materials_report.json", 480, on, False)]
    if args.ab:
        steps += [("shoot_on", "shoot_city", "play_report.json", 600, {}, True)]
    names = []
    for i, (name, script, rep, lim, env, game) in enumerate(steps):
        qb.stamp("step %d/%d" % (i + 1, len(steps)))
        if game and os.path.isdir(os.path.join(SAVED, "shots")):
            shutil.rmtree(os.path.join(SAVED, "shots"))      # no still of an earlier run in the A/B
        ok, secs, note = launch(name, script, rep, lim, env, game, args)
        result["steps"].append({"step": name, "ok": ok, "seconds": secs, "note": note})
        if not ok:
            qb.stamp("%s failed: %s" % (name, note))
            finish(3 if note == "time limit" else 1)
        if name == "import":
            with open(os.path.join(SAVED, "surfaces.json"), encoding="utf-8") as f:
                m = json.load(f)
            result["found"] = sorted(m.get("surfaces", {}))
            result["missing"] = m.get("missing", [])
            qb.stamp("surfaces found: %s; missing: %s" % (" ".join(result["found"]) or "none",
                                                          " ".join(result["missing"])))
        if name.startswith("shoot_"):
            names = keep_shots(name[len("shoot_"):])
            result[name + "_shots"] = names
            with open(os.path.join(SAVED, "play_report.json"), encoding="utf-8") as f:
                play = json.load(f)
            result[name + "_frames"] = {k: play.get(k) for k in ("frame_ms_p50", "frame_ms_p95", "frames", "hitches_over_100ms") if k in play}
    if args.ab:
        result["pairs"] = pairs(names)
        qb.stamp("before/after: Saved/CityImport/surfaces_ab/ (%s)" % result["pairs"])
    result["ok"] = True
    finish(0)


if __name__ == "__main__":
    main()
