"""In-game check of the player character, run by playtest_character.sh in -game mode.

Drives the pawn through the sample's own input actions (injected into Enhanced
Input, so the character Blueprint reacts as to real keys): run, walk, sprint,
jump, crouch, then the camera toggle to first person and back, and a respawn
while in first person. Measures speeds, jump height and states, takes
1920x1080 screenshots and writes <out>/playtest.json, then quits.

On-foot feel (docs/gameplay/on-foot-feel.md): checks the follow camera
framing (distance, field of view at rest and sprinting), that the camera is
never behind a wall (a line from the shoulders to the camera on every frame),
the stopping distance after a sprint, and the automatic recentering while
running diagonally.

The output folder comes from the GTP_PLAYTEST_OUT environment variable
(default: <repo>/.scratch/character).
"""

import json
import math
import os

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
OUT = os.environ.get("GTP_PLAYTEST_OUT") or os.path.join(REPO, ".scratch", "character")
WARMUP_S = 10.0
INPUT = "/Game/Input/"

state = dict(t=0.0, step=0, step_t=0.0, results={}, handle=None, samples=[], yaw=0.0, data={})


def log(msg):
    unreal.log_warning("[CharacterPlaytest] " + msg)


def player_controller():
    for pc in unreal.ObjectIterator(unreal.PlayerController):
        if pc.get_world() is not None and pc.is_local_player_controller():
            return pc
    return None


def pawn():
    pc = player_controller()
    return pc.get_controlled_pawn() if pc else None


def world():
    pc = player_controller()
    return pc.get_world() if pc else None


def input_subsystem():
    # Python has no subsystem getter for local players: there is a single
    # local player in -game mode, take its Enhanced Input subsystem.
    for sub in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem):
        if not sub.get_name().startswith("Default__"):
            return sub
    raise RuntimeError("no EnhancedInputLocalPlayerSubsystem")


_actions = {}


def act(name):
    if name not in _actions:
        path = INPUT + ("GTP/" if name == "IA_ToggleCameraMode" else "") + name
        _actions[name] = unreal.load_asset(path)
    return _actions[name]


def inject(name, x=1.0, y=0.0):
    input_subsystem().inject_input_vector_for_action(act(name), unreal.Vector(x, y, 0.0), [], [])


def move_forward():
    inject("IA_Move", 0.0, 1.0)  # sample convention: Y forward, X right


def face(yaw, pitch=-10.0):
    pc = player_controller()
    pc.set_ignore_look_input(True)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))


def screenshot(name):
    path = os.path.join(OUT, name + ".png")
    unreal.SystemLibrary.execute_console_command(world(), 'HighResShot 1920x1080 filename="%s"' % path)
    log("screenshot " + path)


def hspeed(p):
    v = p.get_velocity()
    return math.hypot(v.x, v.y)


def mesh(p):
    return p.get_editor_property("mesh")


def camera_info():
    pc = player_controller()
    target = pc.get_view_target()
    p = pawn()
    head_hidden = bool(mesh(p).is_bone_hidden_by_name("head")) if p else None
    manager = pc.player_camera_manager
    fov = manager.get_fov_angle()
    distance = None
    if p:
        cam = manager.get_camera_location()
        loc = p.get_actor_location()
        distance = round(math.sqrt((cam.x - loc.x) ** 2 + (cam.y - loc.y) ** 2 + (cam.z - loc.z - 62.0) ** 2))
    return dict(view_target=target.get_class().get_name() if target else None, head_hidden=head_hidden,
                fov=round(fov, 1), distance_cm=distance)


def sample_camera_clear(p):
    """Counts frames where something solid sits between the shoulders and the third person camera."""
    pc = player_controller()
    if not p or not pc or state["data"].get("first_person"):
        return
    c = state["data"].setdefault("clear", dict(frames=0, blocked=0, error=None))
    if c["error"]:
        return
    try:
        loc = p.get_actor_location()
        start = unreal.Vector(loc.x, loc.y, loc.z + 62.0)
        hit = unreal.SystemLibrary.line_trace_single(
            world(), start, pc.player_camera_manager.get_camera_location(),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY2, True, [p], unreal.DrawDebugTrace.NONE, True)
        c["frames"] += 1
        if hit is not None and hit.to_tuple()[0]:
            c["blocked"] += 1
    except Exception as exc:  # keep the playtest going, report the probe failure
        c["error"] = repr(exc)


# Each step: (name, duration_s, per-tick action, on-end check)

def track(key):
    def f(p):
        d = state["data"].setdefault(key, dict(max_speed=0.0, start=None, max_z=-1e9, falling=False, crouched=False))
        if d["start"] is None:
            d["start"] = p.get_actor_location()
        d["max_speed"] = max(d["max_speed"], hspeed(p))
        d["max_z"] = max(d["max_z"], p.get_actor_location().z)
        mc = p.get_movement_component()
        d["falling"] = d["falling"] or mc.is_falling()
        d["crouched"] = d["crouched"] or mc.is_crouching()
    return f


def run_step(key, extra=None):
    def f(p):
        move_forward()
        if extra:
            inject(extra)
        track(key)(p)
    return f


def record(key, **checks):
    def f(p):
        d = state["data"].get(key, {})
        start = d.get("start")
        moved = 0.0
        if start is not None:
            loc = p.get_actor_location()
            moved = math.hypot(loc.x - start.x, loc.y - start.y)
        r = dict(max_speed_cm_s=round(d.get("max_speed", 0.0)), moved_cm=round(moved),
                 jump_rise_cm=round(d.get("max_z", 0.0) - start.z) if start is not None else 0,
                 fell=d.get("falling", False), crouched=d.get("crouched", False))
        ok = True
        for name, test in checks.items():
            ok = ok and test(r)
        r["ok"] = ok
        state["results"][key] = r
        log("%s %s" % (key, json.dumps(r)))
    return f


def press(name):
    return lambda p: inject(name)


def press_once(name):
    """Inject on the first tick of the step only (a single press)."""
    def f(p):
        if not state.get("fired"):
            state["fired"] = True
            inject(name)
    return f


def turn_around(p):
    state["yaw"] += 180.0
    face(state["yaw"])


def check_camera(key, first_person):
    def f(p):
        info = camera_info()
        state["data"]["first_person"] = first_person
        if first_person:
            ok = info["view_target"] == "CameraActor" and info["head_hidden"] is True and 85.0 <= info["fov"] <= 95.0 \
                and info["distance_cm"] is not None and info["distance_cm"] < 60
        else:
            # Follow camera: behind the shoulder at walking framing (a wall may pull it in).
            ok = info["head_hidden"] is False and info["distance_cm"] is not None and info["distance_cm"] > 120
        info["ok"] = ok
        state["results"][key] = info
        log("%s %s" % (key, json.dumps(info)))
    return f


def respawn(p):
    pc = player_controller()
    old = p.get_name()
    p.destroy_actor()
    unreal.GameplayStatics.get_game_mode(world()).restart_player(pc)
    state["data"]["respawn_old"] = old


def check_respawn(p):
    info = camera_info()
    info["new_pawn"] = p.get_name() if p else None
    info["ok"] = bool(p) and p.get_name() != state["data"].get("respawn_old") and info["view_target"] == "CameraActor" \
        and info["head_hidden"] is True
    state["results"]["first_person_after_respawn"] = info
    log("respawn %s" % json.dumps(info))


def nothing(p):
    pass


def check_framing(key, fov_range, min_distance):
    def f(p):
        info = camera_info()
        info["speed_cm_s"] = round(hspeed(p))
        info["ok"] = fov_range[0] <= info["fov"] <= fov_range[1] and (info["distance_cm"] or 0) >= min_distance
        state["results"][key] = info
        state["data"][key] = info
        log("%s %s" % (key, json.dumps(info)))
    return f


def track_stop(p):
    d = state["data"].setdefault("stop", dict(start=p.get_actor_location(), time_to_stop=None))
    if d["time_to_stop"] is None and hspeed(p) < 10.0:
        d["time_to_stop"] = round(state["step_t"], 2)


def record_stop(p):
    d = state["data"].get("stop", {})
    start = d.get("start")
    loc = p.get_actor_location()
    moved = math.hypot(loc.x - start.x, loc.y - start.y) if start else 0.0
    r = dict(stop_distance_cm=round(moved), time_to_stop_s=d.get("time_to_stop"))
    # Weight without skating: a sprint stops within about 0.3 to 3 m and a second.
    r["ok"] = 20 <= moved <= 300 and r["time_to_stop_s"] is not None and r["time_to_stop_s"] <= 1.2
    state["results"]["sprint_stop"] = r
    log("sprint_stop %s" % json.dumps(r))


def diagonal_run(p):
    if "diag_yaw0" not in state["data"]:
        state["data"]["diag_yaw0"] = player_controller().get_control_rotation().yaw
    inject("IA_Move", 0.7, 0.7)


def record_recenter(p):
    yaw0 = state["data"].get("diag_yaw0", 0.0)
    yaw1 = player_controller().get_control_rotation().yaw
    turned = abs((yaw1 - yaw0 + 180.0) % 360.0 - 180.0)
    r = dict(view_turned_deg=round(turned, 1))
    # The view drifts towards the direction of travel, without spinning.
    r["ok"] = 15.0 <= turned <= 200.0
    state["results"]["auto_recenter"] = r
    log("auto_recenter %s" % json.dumps(r))


def record_clear(p):
    c = state["data"].get("clear", dict(frames=0, blocked=0, error="no sample"))
    ratio = 1.0 - c["blocked"] / c["frames"] if c["frames"] else 0.0
    r = dict(frames=c["frames"], blocked=c["blocked"], clear_ratio=round(ratio, 4), error=c["error"])
    r["ok"] = c["error"] is None and c["frames"] > 100 and ratio >= 0.99
    state["results"]["camera_never_behind_wall"] = r
    log("camera_clear %s" % json.dumps(r))


STEPS = [
    ("tp_idle", 2.0, nothing, lambda p: (screenshot("third_person_idle"),
                                         check_framing("framing_idle", (74.0, 82.0), 200)(p))),
    ("run", 5.0, run_step("run"), record("run", ran=lambda r: r["max_speed_cm_s"] > 300)),
    ("walk_on", 0.2, press_once("IA_Walk"), nothing),
    ("walk", 4.0, run_step("walk"), record("walk", slower=lambda r: 50 < r["max_speed_cm_s"] < 300)),
    ("walk_off", 0.2, press_once("IA_Walk"), turn_around),
    ("sprint", 5.0, run_step("sprint", "IA_Sprint"), record("sprint", fast=lambda r: r["max_speed_cm_s"] > 500)),
    ("sprint_shot", 0.6, run_step("sprint_shot", "IA_Sprint"),
     lambda p: (screenshot("third_person_sprint"), check_framing("framing_sprint", (82.0, 90.0), 240)(p))),
    ("sprint_stop", 2.0, track_stop, record_stop),
    ("jump", 1.6, lambda p: (press_once("IA_Jump")(p), track("jump")(p)),
     record("jump", airborne=lambda r: r["fell"] and r["jump_rise_cm"] > 20)),
    ("crouch_on", 0.4, press_once("IA_Crouch"), turn_around),
    ("crouch", 4.0, run_step("crouch"), record("crouch", crouched=lambda r: r["crouched"] and r["max_speed_cm_s"] < 350)),
    ("crouch_shot", 0.5, nothing, lambda p: screenshot("third_person_crouch")),
    ("crouch_off", 0.6, press_once("IA_Crouch"), check_camera("third_person_camera", False)),
    ("diagonal", 6.0, diagonal_run, record_recenter),
    ("diagonal_shot", 0.3, nothing, lambda p: (screenshot("third_person_recenter"), record_clear(p))),
    ("to_first", 1.0, press_once("IA_ToggleCameraMode"),
     check_camera("first_person_camera", True)),
    ("fp_idle", 1.0, nothing, lambda p: screenshot("first_person_idle")),
    ("fp_look_down", 1.0, lambda p: face(state["yaw"], -70.0),
     lambda p: screenshot("first_person_look_down")),
    ("fp_look_up", 0.5, nothing, lambda p: face(state["yaw"])),
    ("fp_run", 4.0, run_step("first_person_run"), record("first_person_run", ran=lambda r: r["max_speed_cm_s"] > 300)),
    ("fp_shot", 0.6, run_step("fp_shot"), lambda p: screenshot("first_person_run")),
    ("respawn", 0.1, respawn, nothing),
    ("after_respawn", 2.0, nothing, check_respawn),
    ("fp_respawn_shot", 0.5, nothing, lambda p: screenshot("first_person_after_respawn")),
    ("to_third", 1.0, press_once("IA_ToggleCameraMode"),
     check_camera("third_person_camera_back", False)),
    ("tp_shot", 1.0, nothing, lambda p: screenshot("third_person_back")),
]


def finish():
    os.makedirs(OUT, exist_ok=True)
    ok = bool(state["results"]) and all(r.get("ok") for r in state["results"].values())
    report = dict(result="PASS" if ok else "FAIL", checks=state["results"], duration_s=round(state["t"], 1))
    with open(os.path.join(OUT, "playtest.json"), "w") as f:
        json.dump(report, f, indent=1)
    log("RESULT " + report["result"])


def quit_game():
    unreal.unregister_slate_post_tick_callback(state["handle"])
    unreal.SystemLibrary.quit_game(world(), None, unreal.QuitPreference.QUIT, False)


def tick(dt):
    state["t"] += dt
    state["step_t"] += dt
    p = pawn()
    try:
        if state["step"] == 0:  # warm-up
            if p and state["step_t"] >= WARMUP_S:
                loc = p.get_actor_location()
                state["yaw"] = p.get_actor_rotation().yaw
                face(state["yaw"])
                state["results"]["spawned"] = dict(ok=True, pawn=p.get_class().get_name(),
                                                   location=[round(loc.x), round(loc.y), round(loc.z)])
                state["step"], state["step_t"] = 1, 0.0
            elif state["t"] > 150.0:
                state["results"]["spawned"] = dict(ok=False, reason="no pawn after 150 s")
                finish()
                quit_game()
            return
        index = state["step"] - 1
        if index >= len(STEPS):
            if state["step_t"] > 1.0:
                finish()
                quit_game()
            return
        name, duration, during, at_end = STEPS[index]
        if p is None:
            if name != "after_respawn":
                return
        elif state["step_t"] < duration:
            during(p)
            sample_camera_clear(p)
            return
        if p is not None:
            at_end(p)
        state["step"], state["step_t"], state["fired"] = state["step"] + 1, 0.0, False
    except Exception as exc:  # report instead of hanging the game
        state["results"]["script_error"] = dict(ok=False, error=repr(exc), step=state["step"])
        log("error " + repr(exc))
        finish()
        quit_game()


os.makedirs(OUT, exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered, output " + OUT)
