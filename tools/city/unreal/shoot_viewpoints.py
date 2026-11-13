"""Shoot the five reference viewpoints of the street slice, in -game mode.

Started like tools/city/unreal/shoot_city.py: copy to a path without spaces, then
  UnrealEditor <uproject> <map> -game -SkipTitle -ExecCmds="py <copy>" -DemoDeterministic=1
Without -SkipTitle the title screen pauses the world (13/11): HighResShot does not show the menu,
but nothing ticks behind it, so the time of day never reaches the exposure or the night lights and
the crowd never spawns. The script unpauses the world anyway and reports it.
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
import math
import os

import unreal

SPEC = os.environ["GTP_VIEWPOINTS"]
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.environ.get("GTP_VIEWS_OUT", os.path.join(PROJECT, "Saved", "Viewpoints"))
FAN = os.environ.get("GTP_VIEW_FAN", "0") == "1"
WARMUP_S, SETTLE_S, AFTER_SHOT_S = 15.0, 12.0, 2.0  # settle covers crowd spawn around the moved pawn

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
    gz = ground_z(pc, v["x_cm"], v["y_cm"])
    z = (gz if gz is not None else 0.0) + v["eye_cm"]
    cam = camera(pc)
    cam.get_editor_property("camera_component").set_editor_property("field_of_view", float(v["hfov"]))
    yaw = float(v["yaw"]) + yaw_offset
    rot = unreal.Rotator(roll=0.0, pitch=float(v["pitch"]), yaw=yaw)
    cam.set_actor_location_and_rotation(unreal.Vector(v["x_cm"], v["y_cm"], z), rot, False, True)
    # Pedestrians and traffic stream around the player: bring the (hidden) pawn 3 m behind the
    # camera, otherwise the crowd stays at the PlayerStart and every view is empty.
    pawn = pc.get_controlled_pawn()
    if pawn and v.get("move_pawn", True):
        bx, by = -300.0 * math.cos(math.radians(yaw)), -300.0 * math.sin(math.radians(yaw))
        pz = (gz if gz is not None else 0.0) + 100.0
        pawn.set_actor_location(unreal.Vector(v["x_cm"] + bx, v["y_cm"] + by, pz), False, True)
        pawn.set_actor_hidden_in_game(True)
    return gz


def hide_ui(pc):
    """No HUD, minimap or notifications on the shots: with the world unpaused (no title screen)
    the gameplay widgets are on screen and HighResShot can catch them."""
    try:
        hud = pc.get_hud()
        if hud:
            hud.set_editor_property("show_hud", False)
    except Exception as e:  # noqa: BLE001
        state.setdefault("ui_errors", []).append(repr(e))
    try:
        for w in unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(pc.get_world(), unreal.UserWidget, True):
            if w.get_visibility() != unreal.SlateVisibility.HIDDEN:
                w.set_visibility(unreal.SlateVisibility.HIDDEN)
    except Exception as e:  # noqa: BLE001
        state.setdefault("ui_errors", []).append(repr(e))


def tick(dt):
    state["t"] += dt
    state["phase_t"] += dt
    pc = controller()
    ph = state["phase"]
    if pc and unreal.GameplayStatics.is_game_paused(pc.get_world()):
        unreal.GameplayStatics.set_game_paused(pc.get_world(), False)
        if not state.get("unpaused"):
            state["unpaused"] = True
            state["report"].append({"warning": "world was paused (title screen?): unpaused; launch with -SkipTitle"})
            log("world was paused (title screen?): unpaused; launch with -SkipTitle")
    if ph == "warmup":
        if pc and pc.get_controlled_pawn() and state["phase_t"] >= WARMUP_S:
            state["phase"], state["phase_t"] = "place", 0.0
        elif state["t"] > 240:
            state["report"].append({"error": "no pawn after 240 s"})
            state["phase"], state["phase_t"] = "quit", 0.0
    elif ph == "place":
        v, off, name = SHOTS[state["i"]]
        hide_ui(pc)
        gz = setup(pc, v, off)
        state["report"].append({"shot": name, "ground_z_cm": gz, "yaw": v["yaw"] + off})
        state["phase"], state["phase_t"] = "settle", 0.0
    elif ph == "settle" and state["phase_t"] >= SETTLE_S:
        v, off, name = SHOTS[state["i"]]
        # The game may take the view back (first shot, V0): set the camera again, hide the UI again.
        camera(pc)
        hide_ui(pc)
        state["report"][-1]["view_target"] = pc.get_view_target().get_name() if pc.get_view_target() else None
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
            json.dump({"spec": SPEC, "fan": FAN, "shots": state["report"], "ui_errors": state.get("ui_errors", [])[:5]},
                      fh, indent=2)
        log("done: %d shots" % len(state["report"]))
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(pc.get_world(), None, unreal.QuitPreference.QUIT, False)


os.makedirs(OUT, exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered, %d shots" % len(SHOTS))
