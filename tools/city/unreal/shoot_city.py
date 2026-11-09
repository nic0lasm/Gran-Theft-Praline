"""Walk the generated city street in -game mode: screenshots and frame times.

Run by tools/city/unreal/run_city_import.sh (copied to a path without spaces,
started with -ExecCmds="py <copy>", rendering profile m1 applied by the caller).
The pawn starts at the PlayerStart (Bellecour end), waits for shaders and
exposure to settle, then is carried up the street axis at walking-jog speed
(WALK_CM_S) to the far end, the camera looking ahead. Every frame time of the
walk is recorded; the walk pauses for a screenshot at the start, at 1/4, 1/2
and 3/4 of the street (one of them turned towards the shopfronts) and at the
end. Writes Saved/CityImport/shots/*.png and Saved/CityImport/play_report.json.
"""

import json
import math
import os

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
OUT = os.path.join(REPO, "GranTheftPraline", "Saved", "CityImport")
WARMUP_S, SETTLE_S = 12.0, 2.5
WALK_CM_S = 500.0
SHOTS = [(0.0, "01_bellecour", 0.0), (0.25, "02_quarter", 0.0), (0.5, "03_middle_shops", 65.0),
         (0.75, "04_three_quarters", 0.0), (1.0, "05_end", 0.0)]

with open(os.path.join(REPO, "assets", "geo", "processed", ZONE, "gen", FOCUS, "report.json"), encoding="utf-8") as f:
    AXIS = json.load(f)["focus_axis_ue_cm"]
if AXIS[0][1] < AXIS[-1][1]:
    AXIS = list(reversed(AXIS))  # walk from the Bellecour (south, larger Y) end
SEG = [math.dist(a[:2], b[:2]) for a, b in zip(AXIS, AXIS[1:])]
LENGTH = sum(SEG)

state = dict(t=0.0, phase="warmup", phase_t=0.0, dist=0.0, frames=[], shots=[], next_shot=0, handle=None,
             result={}, hitches=0)


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


def pose(dist):
    """Point and heading (deg) at a distance along the axis."""
    d = max(0.0, min(dist, LENGTH - 1.0))
    for (a, b), L in zip(zip(AXIS, AXIS[1:]), SEG):
        if d <= L:
            t = d / L if L else 0.0
            x, y, z = (a[k] + (b[k] - a[k]) * t for k in range(3))
            return unreal.Vector(x, y, z + 100.0), math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        d -= L
    a, b = AXIS[-2], AXIS[-1]
    return unreal.Vector(b[0], b[1], b[2] + 100.0), math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def place(dist, look_offset=0.0):
    pc = controller()
    pawn = pc.get_controlled_pawn()
    loc, yaw = pose(dist)
    pawn.set_actor_location_and_rotation(loc, unreal.Rotator(0.0, 0.0, yaw), False, True)
    pc.set_ignore_look_input(True)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=-6.0, yaw=yaw + look_offset))


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
            go("settle")
        elif state["t"] > 240:
            state["result"]["error"] = "no pawn after 240 s"
            go("quit")
        elif pawn:
            place(0.0)
    elif ph == "settle":  # stopped at a shot point
        frac, name, look = SHOTS[state["next_shot"]]
        place(state["dist"], look)
        if state["phase_t"] >= SETTLE_S:
            shot(name)
            state["next_shot"] += 1
            go("walk" if state["next_shot"] < len(SHOTS) else "quit")
    elif ph == "walk":
        state["frames"].append(dt * 1000.0)
        if dt > 0.1:
            state["hitches"] += 1
        state["dist"] += WALK_CM_S * dt
        target = SHOTS[state["next_shot"]][0] * LENGTH
        if state["dist"] >= target:
            state["dist"] = min(target, LENGTH - 1.0)
            go("settle")
        place(state["dist"])
    elif ph == "quit" and state["phase_t"] >= SETTLE_S:
        f = state["frames"]
        state["result"].update({
            "street_length_m": round(LENGTH / 100.0), "walk_speed_m_s": WALK_CM_S / 100.0, "frames": len(f),
            "frame_ms_p50": percentile(f, 50), "frame_ms_p95": percentile(f, 95), "frame_ms_p99": percentile(f, 99),
            "frame_ms_max": max(f) if f else None, "hitches_over_100ms": state["hitches"],
            "share_under_33ms": round(sum(1 for v in f if v <= 33.3) / len(f), 3) if f else None,
            "shots": state["shots"],
            "note": "editor -game run (not a Shipping build); profile given by the caller"})
        with open(os.path.join(OUT, "play_report.json"), "w") as fh:
            json.dump(state["result"], fh, indent=2)
        log("report: " + json.dumps(state["result"]))
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(world(), None, unreal.QuitPreference.QUIT, False)


os.makedirs(os.path.join(OUT, "shots"), exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered, street %.0f m" % (LENGTH / 100.0))
