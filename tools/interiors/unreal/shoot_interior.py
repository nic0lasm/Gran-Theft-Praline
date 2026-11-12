"""Three stills of a generated interior level in -game mode: counter, kitchen, dining room.

Start the editor on the interior level itself, for example
  UnrealEditor GranTheftPraline.uproject /Game/Interiors/L_Interior_fastfood_bellecour -game -windowed
      -ResX=1920 -ResY=1080 -NoScreenMessages -unattended -ExecCmds="py C:/no_spaces/shoot_interior.py"
(copy this file to a path without spaces first: -ExecCmds splits on them). GTP_INTERIOR_SPEC picks the
spec (default fastfood_bellecour). The cameras are placed by build_interior_level.py (CameraActors tagged
GTP_Shot=<name>, positions from the spec zones); each one becomes the view target in turn, so the
pawn does not matter. The script waits for shaders and exposure, takes one HighResShot per view, writes
Saved/InteriorBuild/shots/*.png and shots.json, then quits. Never run it during a measurement.
"""

import json
import os

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
SPEC_ID = os.environ.get("GTP_INTERIOR_SPEC", "fastfood_bellecour")
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "InteriorBuild", "shots")
# Warm-up: a freshly built level compiles its shaders in -game (checkerboard default material
# until then): 90 s by default, GTP_SHOT_WARMUP_S overrides.
WARMUP_S = float(os.environ.get("GTP_SHOT_WARMUP_S", "90"))
SETTLE_S, TIMEOUT_S = 3.0, 240.0 + WARMUP_S

state = {"t": 0.0, "phase_t": 0.0, "phase": "warmup", "index": 0, "target": None, "shots": [], "error": None}


def controller():
    for pc in unreal.ObjectIterator(unreal.PlayerController):
        if pc.get_world() is not None and pc.is_local_player_controller():
            return pc
    return None


def cameras(pc):
    """[(name, camera actor)] in name order, from the GTP_Shot=<name> tags."""
    found = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(pc.get_world(), unreal.CameraActor):
        for tag in actor.tags:
            text = str(tag)
            if text.startswith("GTP_Shot="):
                found.append((text[len("GTP_Shot="):], actor))
    return sorted(found, key=lambda item: item[0])


def tick(dt):
    state["t"] += dt
    state["phase_t"] += dt
    pc = controller()
    shots = cameras(pc) if pc else []
    if state["phase"] == "warmup":
        if pc and state["phase_t"] >= WARMUP_S:
            state["phase"], state["phase_t"] = ("settle", 0.0) if shots else ("quit", 0.0)
            if not shots:
                state["error"] = "no GTP_Shot camera in the level (rebuild it with build_interior_level.py)"
        elif state["t"] > TIMEOUT_S:
            state["error"] = "no player controller after %d s" % TIMEOUT_S
            state["phase"], state["phase_t"] = "quit", 0.0
    elif state["phase"] == "settle":
        name, cam = shots[state["index"]]
        if state["phase_t"] == 0.0 or state.get("target") != name:
            pc.set_view_target_with_blend(cam, 0.0)
            state["target"] = name
        if state["phase_t"] >= SETTLE_S:
            path = os.path.join(OUT, name + ".png").replace("\\", "/")
            unreal.SystemLibrary.execute_console_command(pc.get_world(), 'HighResShot 1920x1080 filename="%s"' % path)
            loc, rot = cam.get_actor_location(), cam.get_actor_rotation()
            state["shots"].append({"name": name, "path": path, "location": [loc.x, loc.y, loc.z],
                                   "pitch": rot.pitch, "yaw": rot.yaw})
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
