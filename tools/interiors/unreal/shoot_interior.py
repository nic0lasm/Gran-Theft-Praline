"""Three stills of a generated interior level in -game mode: counter, kitchen, dining room.

Start the editor on the interior level itself, for example
  UnrealEditor GranTheftPraline.uproject /Game/Interiors/L_Interior_fastfood_bellecour -game -windowed
      -ResX=1920 -ResY=1080 -NoScreenMessages -unattended -ExecCmds="py C:/no_spaces/shoot_interior.py"
(copy this file to a path without spaces first: -ExecCmds splits on them). GTP_INTERIOR_SPEC picks the
spec (default fastfood_bellecour). A camera actor is spawned and made the view target, so the pawn
does not matter. The script waits for shaders and exposure, takes one HighResShot per view, writes
Saved/InteriorBuild/shots/*.png and shots.json, then quits. Never run it during a measurement.
"""

import json
import math
import os

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
SPEC_ID = os.environ.get("GTP_INTERIOR_SPEC", "fastfood_bellecour")
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "InteriorBuild", "shots")
WARMUP_S, SETTLE_S, TIMEOUT_S = 15.0, 3.0, 240.0
EYE_M = 1.65

with open(os.path.join(REPO, "tools", "interiors", SPEC_ID + ".json"), encoding="utf-8") as handle:
    SPEC = json.load(handle)
ZONES = {z["name"]: z["rect"] for z in SPEC["zones"]}


def centre(name):
    x0, y0, x1, y1 = ZONES[name]
    return (x0 + x1) / 2.0, (y0 + y1) / 2.0


def view(name, eye, target, pitch):
    """Interior frame in metres (y grows from the shop front to the back) to UE cm (y flipped)."""
    yaw = math.degrees(math.atan2(-(target[1] - eye[1]), target[0] - eye[0]))
    return name, unreal.Vector(eye[0] * 100.0, -eye[1] * 100.0, EYE_M * 100.0), unreal.Rotator(0.0, pitch, yaw)


def views():
    qx, qy = centre("queue")
    cx, cy = centre("counter")
    kx0, ky0, kx1, ky1 = ZONES["kitchen"]
    dx0, dy0, dx1, dy1 = ZONES["dining_ground"]
    return [
        view("01_counter", (qx, ZONES["queue"][1] + 0.6), (cx + 2.0, cy), -8.0),
        view("02_kitchen", (kx0 + 0.6, (ky0 + ky1) / 2.0), (kx1, (ky0 + ky1) / 2.0), -12.0),
        view("03_dining", (dx1 - 0.5, dy1 - 0.4), (dx0 + 1.0, dy0 + 1.0), -14.0),
    ]


state = {"t": 0.0, "phase_t": 0.0, "phase": "warmup", "index": 0, "camera": None, "shots": [], "error": None}


def controller():
    for pc in unreal.ObjectIterator(unreal.PlayerController):
        if pc.get_world() is not None and pc.is_local_player_controller():
            return pc
    return None


def camera(pc):
    if state["camera"] is None:
        world = pc.get_world()
        transform = unreal.Transform()
        actor = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(world, unreal.CameraActor, transform)
        actor = unreal.GameplayStatics.finish_spawning_actor(actor, transform)
        actor.camera_component.set_editor_property("field_of_view", 75.0)
        actor.camera_component.set_editor_property("constrain_aspect_ratio", False)
        pc.set_view_target_with_blend(actor, 0.0)
        state["camera"] = actor
    return state["camera"]


def tick(dt):
    state["t"] += dt
    state["phase_t"] += dt
    pc = controller()
    shots = views()
    if state["phase"] == "warmup":
        if pc and state["phase_t"] >= WARMUP_S:
            state["phase"], state["phase_t"] = "settle", 0.0
        elif state["t"] > TIMEOUT_S:
            state["error"] = "no player controller after %d s" % TIMEOUT_S
            state["phase"], state["phase_t"] = "quit", 0.0
    elif state["phase"] == "settle":
        name, location, rotation = shots[state["index"]]
        camera(pc).set_actor_location_and_rotation(location, rotation, False, True)
        if state["phase_t"] >= SETTLE_S:
            path = os.path.join(OUT, name + ".png").replace("\\", "/")
            unreal.SystemLibrary.execute_console_command(pc.get_world(), 'HighResShot 1920x1080 filename="%s"' % path)
            state["shots"].append({"name": name, "path": path, "location": [location.x, location.y, location.z],
                                   "pitch": rotation.pitch, "yaw": rotation.yaw})
            state["index"] += 1
            state["phase"] = "settle" if state["index"] < len(shots) else "quit"
            state["phase_t"] = 0.0
    elif state["phase"] == "quit" and state["phase_t"] >= SETTLE_S:
        with open(os.path.join(OUT, "shots.json"), "w", encoding="utf-8") as handle_out:
            json.dump({"spec": SPEC_ID, "shots": state["shots"], "error": state["error"]}, handle_out, indent=1)
        unreal.log_warning("[InteriorShots] %d shots, error=%s" % (len(state["shots"]), state["error"]))
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(pc.get_world() if pc else None, None, unreal.QuitPreference.QUIT, False)


os.makedirs(OUT, exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
