"""In-game check of L_RueRepublique, run by playtest_street.sh in -game mode.

Waits for the game world, takes a screenshot at the spawn point, then drives
the player pawn into a facade and into a boundary wall and checks that it is
stopped by collision. Writes Saved/PlayableStreet/playtest.json and quits.

The pawn is whatever the game mode spawns (DefaultPawn until the playable
character lands); the checks only use movement input and the pawn location.
"""

import json
import math
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(REPO, "GranTheftPraline", "Saved", "PlayableStreet")
WARMUP_S = 8.0     # shaders, Lumen and auto exposure settle
DRIVE_S = 6.0      # long enough to cross the 15-20 m to the facade
QUIT_DELAY_S = 3.0

state = dict(t=0.0, phase="warmup", phase_t=0.0, results={}, start=None, handle=None)


def log(msg):
    unreal.log("[Playtest] " + msg)


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def pawn():
    world = game_world()
    return unreal.GameplayStatics.get_player_pawn(world, 0) if world else None


def screenshot(name):
    path = os.path.join(OUT, name + ".png")
    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, path)
    log("screenshot " + path)


def drive(p, yaw_deg):
    rad = math.radians(yaw_deg)
    p.add_movement_input(unreal.Vector(math.cos(rad), math.sin(rad), 0.0), 1.0, True)


def finish():
    os.makedirs(OUT, exist_ok=True)
    ok = all(r.get("ok") for r in state["results"].values()) and bool(state["results"])
    report = dict(result="PASS" if ok else "FAIL", checks=state["results"])
    with open(os.path.join(OUT, "playtest.json"), "w") as f:
        json.dump(report, f, indent=1)
    log("RESULT " + report["result"])


def next_phase(name):
    state["phase"], state["phase_t"] = name, 0.0
    p = pawn()
    state["start"] = p.get_actor_location() if p else None
    log("phase " + name)


def tick(dt):
    state["t"] += dt
    state["phase_t"] += dt
    p = pawn()
    phase = state["phase"]

    if phase == "warmup":
        if p and state["phase_t"] >= WARMUP_S:
            loc = p.get_actor_location()
            state["results"]["spawned"] = dict(ok=True, location=[round(loc.x), round(loc.y), round(loc.z)])
            screenshot("spawn")
            next_phase("facade")
        elif state["t"] > 120.0:
            state["results"]["spawned"] = dict(ok=False, reason="no pawn after 120 s")
            next_phase("quit")

    elif phase in ("facade", "bounds"):
        # facade: turn 90 degrees right of the street and walk into the buildings;
        # bounds: rise above the roofs and fly back towards the north wall, ~30 m behind the spawn.
        yaw = p.get_actor_rotation().yaw if p else 0.0
        if phase == "facade":
            drive(p, yaw + 90.0)
        else:
            if state["phase_t"] < 4.0:  # ~48 m up, above the 35 m roofs
                p.add_movement_input(unreal.Vector(0.0, 0.0, 1.0), 1.0, True)
            else:
                drive(p, yaw + 180.0)
        limit = DRIVE_S if phase == "facade" else DRIVE_S * 2
        if state["phase_t"] >= limit:
            loc, start = p.get_actor_location(), state["start"]
            moved = math.hypot(loc.x - start.x, loc.y - start.y)
            speed = p.get_velocity().length()
            # Blocked = the pawn moved a bit then stopped, instead of travelling freely.
            state["results"][phase] = dict(ok=speed < 50.0 and moved > 100.0, moved_cm=round(moved),
                                           speed_cm_s=round(speed))
            screenshot(phase)
            next_phase("bounds" if phase == "facade" else "quit")

    elif phase == "quit" and state["phase_t"] >= QUIT_DELAY_S:
        finish()
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(game_world(), None, unreal.QuitPreference.QUIT, False)


os.makedirs(OUT, exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered")
