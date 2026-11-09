"""Play the generated city map in -game mode: screenshots along the street and frame times.

Run by tools/city/unreal/run_city_import.sh (copied to a path without spaces,
started with -ExecCmds="py <copy>"). Writes Saved/CityImport/shots/*.png and
Saved/CityImport/play_report.json, then quits.

Shots: at the PlayerStart looking north, then at 1/4 and 3/4 of the focus
street axis looking along it, each after the frame has settled; frame times
are the engine tick deltas over FRAMES_S seconds at the PlayerStart.
"""

import json
import math
import os

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
OUT = os.path.join(REPO, "GranTheftPraline", "Saved", "CityImport")
WARMUP_S, SETTLE_S, FRAMES_S = 10.0, 3.0, 20.0

with open(os.path.join(REPO, "assets", "geo", "processed", ZONE, "gen", FOCUS, "report.json"), encoding="utf-8") as f:
    AXIS = json.load(f)["focus_axis_ue_cm"]

state = dict(t=0.0, phase="warmup", phase_t=0.0, frames=[], shots=[], queue=[], handle=None, result={})


def log(msg):
    unreal.log_warning("[CityPlay] " + msg)


def controller():
    for pc in unreal.ObjectIterator(unreal.PlayerController):
        if pc.get_world() is not None and pc.is_local_player_controller():
            return pc
    return None


def world():
    pc = controller()
    return pc.get_world() if pc else None


def shot(name):
    path = os.path.join(OUT, "shots", name + ".png")
    unreal.SystemLibrary.execute_console_command(world(), 'HighResShot 1920x1080 filename="%s"' % path)
    state["shots"].append(path)


def look(yaw, pitch=-4.0):
    pc = controller()
    pc.set_ignore_look_input(True)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))


def axis_pose(frac):
    i = min(len(AXIS) - 2, int(frac * (len(AXIS) - 1)))
    a, b = AXIS[i], AXIS[i + 1]
    return unreal.Vector(a[0], a[1], a[2] + 150.0), math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def go(phase):
    state["phase"], state["phase_t"] = phase, 0.0


def percentile(values, p):
    v = sorted(values)
    return v[min(len(v) - 1, int(round(p / 100.0 * (len(v) - 1))))] if v else None


def tick(dt):
    state["t"] += dt
    state["phase_t"] += dt
    pc = controller()
    pawn = pc.get_controlled_pawn() if pc else None
    ph = state["phase"]
    if ph == "warmup":
        if pawn and state["phase_t"] >= WARMUP_S:
            look(-90.0)
            go("frames")
        elif state["t"] > 180:
            state["result"]["error"] = "no pawn after 180 s"
            go("quit")
    elif ph == "frames":
        state["frames"].append(dt * 1000.0)
        if state["phase_t"] >= FRAMES_S:
            shot("spawn_north")
            state["queue"] = [("quarter", 0.25), ("three_quarters", 0.75)]
            go("next")
    elif ph == "next" and state["phase_t"] >= SETTLE_S:
        if not state["queue"]:
            go("quit")
            return
        name, frac = state["queue"].pop(0)
        loc, yaw = axis_pose(frac)
        pawn.teleport(loc, unreal.Rotator(0, 0, yaw))
        look(yaw)
        state["pending"] = name
        go("settle")
    elif ph == "settle" and state["phase_t"] >= SETTLE_S:
        shot(state["pending"])
        go("next")
    elif ph == "quit" and state["phase_t"] >= SETTLE_S:
        f = state["frames"]
        state["result"].update({"frames": len(f), "frame_ms_p50": percentile(f, 50), "frame_ms_p95": percentile(f, 95),
                                "shots": state["shots"],
                                "note": "frame times include the editor-in-game overhead; Shipping build is the reference"})
        with open(os.path.join(OUT, "play_report.json"), "w") as fh:
            json.dump(state["result"], fh, indent=2)
        log("report: " + json.dumps(state["result"]))
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(world(), None, unreal.QuitPreference.QUIT, False)


os.makedirs(os.path.join(OUT, "shots"), exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered")
