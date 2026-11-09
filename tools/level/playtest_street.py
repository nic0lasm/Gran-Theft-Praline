"""In-game check of L_RueRepublique, run by playtest_street.sh in -game mode.

Waits for the game world, then for each test:
  - points the camera along a fixed world direction (mouse input is ignored),
  - measures by a line trace the distance to the first obstacle in that
    direction, with simple collision (what the pawn really collides with),
  - pushes the pawn that way long enough to cross that distance twice,
  - passes if the pawn stopped before the obstacle.
Tests: "facade" at street level towards the nearest building side, "bounds"
above the roofs towards the north wall. One screenshot per test.
Writes Saved/PlayableStreet/playtest.json and quits.

The pawn is whatever the game mode spawns (DefaultPawn until the playable
character lands); only movement input and the pawn location are used.
"""

import json
import math
import os

import unreal

# From the project, not from __file__: the script runs from a copy (see playtest_street.sh).
REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
OUT = os.path.join(REPO, "GranTheftPraline", "Saved", "PlayableStreet")
WARMUP_S = 8.0       # shaders, Lumen and auto exposure settle
SETTLE_S = 2.0       # after a teleport or a camera change, before the screenshot
MIN_DRIVE_S = 4.0
MAX_TRACE_CM = 20000.0
ROOF_CLEARANCE_Z = 4500.0  # above the 35 m roofs, below the 60 m walls
DIRECTIONS = {"east": 0.0, "south": 90.0, "west": 180.0, "north": 270.0}  # +X east, +Y south

state = dict(t=0.0, phase="warmup", phase_t=0.0, results={}, handle=None, test=None, queue=[])


def log(msg):
    unreal.log_warning("[Playtest] " + msg)


def player_controller():
    # In -game mode there is no editor subsystem: find the local controller directly.
    for pc in unreal.ObjectIterator(unreal.PlayerController):
        if pc.get_world() is not None and pc.is_local_player_controller():
            return pc
    return None


def game_world():
    pc = player_controller()
    return pc.get_world() if pc else None


def pawn():
    pc = player_controller()
    return pc.get_controlled_pawn() if pc else None


def screenshot(name):
    path = os.path.join(OUT, name + ".png")
    unreal.SystemLibrary.execute_console_command(game_world(), 'HighResShot 1920x1080 filename="%s"' % path)
    log("screenshot " + path)


def vec(yaw_deg):
    r = math.radians(yaw_deg)
    return unreal.Vector(math.cos(r), math.sin(r), 0.0)


def obstacle_distance(p, yaw_deg):
    """Distance to the first blocking hit along yaw, simple collision only; None if nothing."""
    start = p.get_actor_location()
    d = vec(yaw_deg)
    end = unreal.Vector(start.x + d.x * MAX_TRACE_CM, start.y + d.y * MAX_TRACE_CM, start.z)
    res = unreal.SystemLibrary.line_trace_single(
        game_world(), start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [p],
        unreal.DrawDebugTrace.NONE, True)
    hit = res[1] if isinstance(res, tuple) and res[0] else (None if isinstance(res, tuple) else res)
    if not hit:
        return None, None
    t = hit.to_tuple()  # (blocking_hit, initial_overlap, time, distance, location, impact_point, ...)
    actor = None
    for v in t:  # the hit actor or component, depending on the engine version
        if isinstance(v, unreal.ActorComponent):
            v = v.get_owner()
        if isinstance(v, unreal.Actor):
            actor = v.get_actor_label() if hasattr(v, "get_actor_label") else v.get_name()
            break
    return t[3], actor


def face(yaw_deg, pitch_deg=-8.0):
    pc = player_controller()
    pc.set_ignore_look_input(True)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch_deg, yaw=yaw_deg))


def nearest_side(p, street_yaw):
    """Of the two directions across the street, the one with the closest simple-collision hit."""
    best = None
    for yaw in (street_yaw + 90.0, street_yaw - 90.0):
        dist, actor = obstacle_distance(p, yaw)
        if dist is not None and (best is None or dist < best[1]):
            best = (yaw, dist, actor)
    return best


def start_test(name):
    p = pawn()
    test = dict(name=name)
    if name == "facade":
        street_yaw = state["spawn_yaw"]
        side = nearest_side(p, street_yaw)
        if side is None:
            state["results"]["facade"] = dict(ok=False, reason="no simple-collision hit across the street: "
                                                                "buildings have no player collision")
            return None
        test["yaw"], test["dist"], test["actor"] = side
    else:  # bounds: above the roofs, towards the north wall
        loc = p.get_actor_location()
        p.set_actor_location(unreal.Vector(loc.x, loc.y, ROOF_CLEARANCE_Z), False, True)
        test["yaw"] = DIRECTIONS["north"]
        test["dist"], test["actor"] = obstacle_distance(p, test["yaw"])
        if test["dist"] is None:
            state["results"]["bounds"] = dict(ok=False, reason="no simple-collision hit to the north: no wall")
            return None
    p.get_movement_component().stop_movement_immediately()
    test["start"] = p.get_actor_location()
    speed = max(p.get_movement_component().get_max_speed(), 100.0)
    test["drive_s"] = max(MIN_DRIVE_S, 2.0 * test["dist"] / speed)
    face(test["yaw"])
    log("%s: obstacle %s at %.0f cm, driving %.1f s" % (name, test["actor"], test["dist"], test["drive_s"]))
    return test


def end_test(test):
    p = pawn()
    loc, start = p.get_actor_location(), test["start"]
    d = vec(test["yaw"])
    along = (loc.x - start.x) * d.x + (loc.y - start.y) * d.y
    speed = p.get_velocity().length()
    # Stopped by the obstacle: did not go past it, and moved towards it (not stuck at the start).
    ok = along <= test["dist"] + 50.0 and along >= min(100.0, test["dist"] * 0.5)
    state["results"][test["name"]] = dict(ok=ok, obstacle=test["actor"], obstacle_cm=round(test["dist"]),
                                          moved_along_cm=round(along), speed_cm_s=round(speed))
    face(test["yaw"] + 180.0, -15.0)  # look back from the obstacle so the screenshot shows the street
    screenshot(test["name"])


def finish():
    os.makedirs(OUT, exist_ok=True)
    ok = bool(state["results"]) and all(r.get("ok") for r in state["results"].values())
    report = dict(result="PASS" if ok else "FAIL", checks=state["results"])
    with open(os.path.join(OUT, "playtest.json"), "w") as f:
        json.dump(report, f, indent=1)
    log("RESULT " + report["result"] + " " + json.dumps(state["results"]))


def go(phase):
    state["phase"], state["phase_t"] = phase, 0.0


def tick(dt):
    state["t"] += dt
    state["phase_t"] += dt
    phase = state["phase"]
    p = pawn()

    if phase == "warmup":
        if p and state["phase_t"] >= WARMUP_S:
            loc = p.get_actor_location()
            state["spawn_yaw"] = p.get_actor_rotation().yaw
            face(state["spawn_yaw"])
            state["results"]["spawned"] = dict(ok=True, location=[round(loc.x), round(loc.y), round(loc.z)],
                                               pawn=p.get_class().get_name())
            state["queue"] = ["facade", "bounds"]
            go("spawn_shot")
        elif state["t"] > 120.0:
            state["results"]["spawned"] = dict(ok=False, reason="no pawn after 120 s")
            go("quit")

    elif phase == "spawn_shot" and state["phase_t"] >= SETTLE_S:
        screenshot("spawn")
        go("next")

    elif phase == "next" and state["phase_t"] >= SETTLE_S:
        state["test"] = None
        while state["queue"] and state["test"] is None:
            state["test"] = start_test(state["queue"].pop(0))
        go("drive" if state["test"] else "quit")

    elif phase == "drive":
        test = state["test"]
        if p is None:
            state["results"][test["name"]] = dict(ok=False, reason="pawn lost")
            go("quit")
        elif state["phase_t"] < test["drive_s"]:
            p.add_movement_input(vec(test["yaw"]), 1.0, True)
        else:
            go("stop")

    elif phase == "stop" and state["phase_t"] >= 0.5:  # let the pawn decelerate
        end_test(state["test"])
        go("next")

    elif phase == "quit" and state["phase_t"] >= SETTLE_S:
        finish()
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(game_world(), None, unreal.QuitPreference.QUIT, False)


os.makedirs(OUT, exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered")
