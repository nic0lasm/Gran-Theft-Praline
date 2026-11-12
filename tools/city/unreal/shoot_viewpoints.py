"""Shoot the five reference viewpoints of the street slice, in -game mode.

Started like tools/city/unreal/shoot_city.py: copy to a path without spaces, then
  UnrealEditor <uproject> <map> -game -ExecCmds="py <copy>" -DemoDeterministic=1
Environment:
  GTP_VIEWPOINTS  path to points-de-vue.json (required)
  GTP_VIEWS_OUT   output folder (default <project>/Saved/Viewpoints)
  GTP_VIEW_FAN=1  also shoot each view at yaw -20 and +20 (first series only, to fix headings)
For each view: sets date, hour and weather (gtp.Date / gtp.Time / gtp.Weather, no transition),
moves a camera actor to the eye point (ground found by a vertical trace, plus eye_cm), applies
the horizontal field of view, waits for exposure to settle, then HighResShot at the photo size.
Writes <out>/<id>[_l20|_r20].png and <out>/viewpoints_report.json.
"""

import json
import os

import unreal

SPEC = os.environ["GTP_VIEWPOINTS"]
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.environ.get("GTP_VIEWS_OUT", os.path.join(PROJECT, "Saved", "Viewpoints"))
FAN = os.environ.get("GTP_VIEW_FAN", "0") == "1"
WARMUP_S, SETTLE_S, AFTER_SHOT_S = 15.0, 6.0, 2.0

with open(SPEC, encoding="utf-8") as f:
    VIEWS = json.load(f)["views"]
SHOTS = []
for v in VIEWS:
    SHOTS.append((v, 0.0, v["id"]))
    if FAN:
        SHOTS += [(v, -20.0, v["id"] + "_l20"), (v, 20.0, v["id"] + "_r20")]

state = dict(t=0.0, phase="warmup", phase_t=0.0, i=0, handle=None, cam=None, report=[])


def log(msg):
    unreal.log_warning("[Viewpoints] " + msg)


def controller():
    for pc in unreal.ObjectIterator(unreal.PlayerController):
        if pc.get_world() is not None and pc.is_local_player_controller():
            return pc
    return None


def cmd(pc, text):
    unreal.SystemLibrary.execute_console_command(pc.get_world(), text)


def camera(pc):
    if state["cam"] is None:
        w = pc.get_world()
        cams = [a for a in unreal.ObjectIterator(unreal.CameraActor) if a.get_world() == w]
        if not cams:
            cmd(pc, "EnableCheats")
            cmd(pc, "summon CameraActor")
            cams = [a for a in unreal.ObjectIterator(unreal.CameraActor) if a.get_world() == w]
        if not cams:
            raise RuntimeError("no CameraActor after summon (cheats off?)")
        state["cam"] = cams[-1]
        state["cam"].get_editor_property("camera_component").set_editor_property("constrain_aspect_ratio", False)
    if pc.get_view_target() != state["cam"]:
        pc.set_view_target_with_blend(state["cam"], 0.0)
    return state["cam"]


def ground_z(pc, x, y):
    start, end = unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -5000.0)
    pawn = pc.get_controlled_pawn()
    hit = unreal.SystemLibrary.line_trace_single(
        pc.get_world(), start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False,
        [pawn] if pawn else [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    return t[4].z if t[0] else None  # (blocking_hit, initial_overlap, time, distance, location, ...)


def setup(pc, v, yaw_offset):
    cmd(pc, "gtp.TimeScale 0")
    cmd(pc, "gtp.Date %s" % v["date"])
    cmd(pc, "gtp.Time %.3f" % v["hour"])
    cmd(pc, "gtp.Weather %s 0" % v["weather"])
    # Keep the pawn out of frame: park it under the camera.
    gz = ground_z(pc, v["x_cm"], v["y_cm"])
    z = (gz if gz is not None else 0.0) + v["eye_cm"]
    cam = camera(pc)
    cam.get_editor_property("camera_component").set_editor_property("field_of_view", float(v["hfov"]))
    rot = unreal.Rotator(roll=0.0, pitch=float(v["pitch"]), yaw=float(v["yaw"]) + yaw_offset)
    cam.set_actor_location_and_rotation(unreal.Vector(v["x_cm"], v["y_cm"], z), rot, False, True)
    return gz


def tick(dt):
    state["t"] += dt
    state["phase_t"] += dt
    pc = controller()
    ph = state["phase"]
    if ph == "warmup":
        if pc and pc.get_controlled_pawn() and state["phase_t"] >= WARMUP_S:
            state["phase"], state["phase_t"] = "place", 0.0
        elif state["t"] > 240:
            state["report"].append({"error": "no pawn after 240 s"})
            state["phase"], state["phase_t"] = "quit", 0.0
    elif ph == "place":
        v, off, name = SHOTS[state["i"]]
        gz = setup(pc, v, off)
        state["report"].append({"shot": name, "ground_z_cm": gz, "yaw": v["yaw"] + off})
        state["phase"], state["phase_t"] = "settle", 0.0
    elif ph == "settle" and state["phase_t"] >= SETTLE_S:
        v, off, name = SHOTS[state["i"]]
        path = os.path.join(OUT, name + ".png")
        cmd(pc, 'HighResShot %dx%d filename="%s"' % (v["res"][0], v["res"][1], path))
        state["report"][-1]["file"] = path
        state["phase"], state["phase_t"] = "after", 0.0
    elif ph == "after" and state["phase_t"] >= AFTER_SHOT_S:
        state["i"] += 1
        state["phase"] = "place" if state["i"] < len(SHOTS) else "quit"
        state["phase_t"] = 0.0
    elif ph == "quit" and state["phase_t"] >= AFTER_SHOT_S:
        with open(os.path.join(OUT, "viewpoints_report.json"), "w") as fh:
            json.dump({"spec": SPEC, "fan": FAN, "shots": state["report"]}, fh, indent=2)
        log("done: %d shots" % len(state["report"]))
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(pc.get_world(), None, unreal.QuitPreference.QUIT, False)


os.makedirs(OUT, exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered, %d shots" % len(SHOTS))
