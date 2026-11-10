"""In-game driving check, run by playtest_drive.sh in -game mode with -TestVehicle.

Steps, each logged and photographed:
  parked   the test car parked in front of the player (spawned by the game mode)
  enter    enter / exit key logic (interaction component), wait for the car to be possessed
  launch   full throttle for LAUNCH_S, speed sampled: time to 50 km/h, top speed
  turn     steering full left while rolling: yaw change
  brake    full brake from the current speed: stopping distance
  exit     enter / exit again: the player is back on foot next to the car
  damage   front damage applied: dent morph target and health
Writes Saved/Vehicles/playtest_drive.json (PASS / FAIL per check) and quits.

With GTP_DRIVE_MODE=bench (frame time run of playtest_drive.sh): enter the
car, drive for BENCH_S with gentle steering, write playtest_bench.json, quit.
"""

import json
import os

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
OUT = os.path.join(REPO, "GranTheftPraline", "Saved", "Vehicles")
WARMUP_S = 8.0
SETTLE_S = 1.5
LAUNCH_S = 9.0
TURN_S = 3.0
MAX_BRAKE_S = 8.0
TIMEOUT_S = 6.0
BENCH_S = 60.0
MODE = os.environ.get("GTP_DRIVE_MODE", "checks")

state = dict(t=0.0, phase="warmup", phase_t=0.0, checks={}, samples=[], handle=None, tilt=0.0)


def log(msg):
    unreal.log_warning("[DrivePlaytest] " + msg)


def player_controller():
    for pc in unreal.ObjectIterator(unreal.PlayerController):
        if pc.get_world() is not None and pc.is_local_player_controller():
            return pc
    return None


def world():
    pc = player_controller()
    return pc.get_world() if pc else None


def pawn():
    pc = player_controller()
    return pc.get_controlled_pawn() if pc else None


def vehicle():
    """The car nearest the player when first asked (the -TestVehicle one, next
    to the player; the level may also hold parked cars further away), then the
    same car for the whole run."""
    v = state.get("vehicle")
    if v is not None:
        return v
    p = pawn()
    cars = [c for c in unreal.ObjectIterator(unreal.GTPVehicle)
            if c.get_world() is not None and c.get_world() == world()]
    if not cars or p is None:
        return cars[0] if cars else None
    v = min(cars, key=lambda c: c.get_actor_location().distance(p.get_actor_location()))
    state["vehicle"] = v
    return v


def movement(v):
    return v.get_component_by_class(unreal.ChaosWheeledVehicleMovementComponent)


def interaction():
    return player_controller().get_component_by_class(unreal.GTPVehicleInteractionComponent)


def screenshot(name):
    path = os.path.join(OUT, name + ".png")
    unreal.SystemLibrary.execute_console_command(world(), 'HighResShot 1920x1080 filename="%s"' % path)


def check(name, ok, **data):
    data["ok"] = bool(ok)
    state["checks"][name] = data
    log("%s %s %s" % (name, "PASS" if ok else "FAIL", json.dumps(data)))


def go(phase):
    state["phase"], state["phase_t"] = phase, 0.0


def finish():
    os.makedirs(OUT, exist_ok=True)
    ok = bool(state["checks"]) and all(c["ok"] for c in state["checks"].values())
    report = dict(result="PASS" if ok else "FAIL", checks=state["checks"], speed_samples=state["samples"])
    name = "playtest_bench.json" if MODE == "bench" else "playtest_drive.json"
    with open(os.path.join(OUT, name), "w") as f:
        json.dump(report, f, indent=1)
    log("RESULT " + report["result"])


def tick(dt):
    try:
        step(dt)
    except Exception as exc:  # a broken step must not leave the game running forever
        check("script", False, error=repr(exc), phase=state["phase"])
        state["phase"] = "quit"
        finish()
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(world(), None, unreal.QuitPreference.QUIT, False)


def step(dt):
    state["t"] += dt
    state["phase_t"] += dt
    phase, pt = state["phase"], state["phase_t"]
    v = vehicle()

    if phase == "warmup":
        if pt >= WARMUP_S and pawn():
            if v is None:
                check("parked", False, reason="no GTPVehicle in the world: -TestVehicle and TestVehicleClass?")
                go("quit")
            else:
                d = v.get_actor_location().distance(pawn().get_actor_location())
                r = v.get_actor_rotation()
                check("parked", abs(r.roll) < 5 and abs(r.pitch) < 5, vehicle=v.get_class().get_name(),
                      distance_cm=round(d), roll=round(r.roll, 1), pitch=round(r.pitch, 1))
                if MODE != "bench":
                    screenshot("parked")
                go("enter")
        elif state["t"] > 120.0:
            check("parked", False, reason="no pawn after 120 s")
            go("quit")

    elif phase == "enter":
        if pt < SETTLE_S:
            return
        if "enter_t0" not in state:
            state["enter_t0"] = state["t"]
            interaction().handle_enter_exit()
        if pawn() == v:
            check("enter", True, seconds=round(state["t"] - state["enter_t0"], 2), state=str(v.get_vehicle_state()))
            state["start"] = v.get_actor_location()
            if MODE == "bench":
                go("bench")
            else:
                screenshot("seated")
                go("launch")
        elif state["t"] - state["enter_t0"] > TIMEOUT_S:
            check("enter", False, reason="car not possessed", state=str(v.get_vehicle_state()))
            go("quit")

    elif phase == "launch":
        m = movement(v)
        m.set_handbrake_input(False)
        m.set_throttle_input(1.0)
        speed = v.get_speed_kmh()
        r = v.get_actor_rotation()
        state["tilt"] = max(state["tilt"], abs(r.roll), abs(r.pitch))
        state["samples"].append([round(pt, 2), round(speed, 1)])
        if speed >= 50.0 and "t50" not in state:
            state["t50"] = pt
        if 5.0 <= pt < 5.0 + dt:
            screenshot("driving")
        if pt >= LAUNCH_S:
            top = max(s[1] for s in state["samples"])
            check("launch", top > 40.0 and state["tilt"] < 10.0, top_kmh=round(top, 1),
                  max_tilt_deg=round(state["tilt"], 1),
                  zero_to_50_s=round(state["t50"], 2) if "t50" in state else None,
                  distance_m=round(v.get_actor_location().distance(state["start"]) / 100.0, 1))
            state["yaw0"] = v.get_actor_rotation().yaw
            go("turn")

    elif phase == "turn":
        m = movement(v)
        m.set_throttle_input(0.5)
        m.set_steering_input(-1.0)
        if pt >= TURN_S:
            yaw = abs((v.get_actor_rotation().yaw - state["yaw0"] + 180.0) % 360.0 - 180.0)
            check("turn", yaw > 20.0, yaw_change_deg=round(yaw, 1), speed_kmh=round(v.get_speed_kmh(), 1))
            m.set_steering_input(0.0)
            m.set_throttle_input(0.0)
            state["brake_from"] = v.get_speed_kmh()
            state["brake_start"] = v.get_actor_location()
            go("brake")

    elif phase == "bench":
        # Gentle city driving: about 40 km/h, slow left-right weave.
        import math
        m = movement(v)
        speed = v.get_speed_kmh()
        m.set_handbrake_input(False)
        m.set_throttle_input(0.7 if speed < 40.0 else 0.0)
        m.set_brake_input(0.4 if speed > 50.0 else 0.0)
        m.set_steering_input(0.35 * math.sin(pt * 0.6))
        if int(pt) != int(pt - dt):
            state["samples"].append([round(pt, 1), round(speed, 1)])
        if pt >= BENCH_S:
            check("bench", True, seconds=round(pt, 1),
                  distance_m=round(v.get_actor_location().distance(state["start"]) / 100.0, 1))
            go("quit")

    elif phase == "brake":
        m = movement(v)
        m.set_brake_input(1.0)
        speed = abs(v.get_speed_kmh())
        if speed < 1.0 or pt > MAX_BRAKE_S:
            dist = v.get_actor_location().distance(state["brake_start"]) / 100.0
            check("brake", speed < 1.0, from_kmh=round(state["brake_from"], 1), distance_m=round(dist, 1),
                  seconds=round(pt, 2))
            m.set_brake_input(0.0)
            go("exit")

    elif phase == "exit":
        if pt < SETTLE_S:
            return
        if "exit_t0" not in state:
            state["exit_t0"] = state["t"]
            interaction().handle_enter_exit()
        p = pawn()
        if p is not None and p != v:
            d = p.get_actor_location().distance(v.get_actor_location())
            check("exit", True, seconds=round(state["t"] - state["exit_t0"], 2), distance_to_car_cm=round(d),
                  pawn=p.get_class().get_name())
            go("exit_shot")
        elif state["t"] - state["exit_t0"] > TIMEOUT_S:
            check("exit", False, reason="still in the car")
            go("quit")

    elif phase == "exit_shot" and pt >= SETTLE_S:
        screenshot("exited")
        health0 = v.get_health()
        v.apply_zone_damage(unreal.GTPDamageZone.FRONT, 300.0)
        check("damage", v.get_health() < health0, health_before=health0, health_after=v.get_health())
        go("damage_shot")

    elif phase == "damage_shot" and pt >= SETTLE_S:
        screenshot("damaged")
        go("quit")

    elif phase == "quit" and pt >= SETTLE_S:
        finish()
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_game(world(), None, unreal.QuitPreference.QUIT, False)


os.makedirs(OUT, exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered")
