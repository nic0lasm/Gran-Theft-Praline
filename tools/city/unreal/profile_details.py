"""Measure what the street dressing costs on the GPU, in -game mode, without rebuilding the map.

Run by profile_details.sh (copied to a path without spaces, started with -ExecCmds="py <copy>",
rendering profile applied by the caller). The camera stands at fixed points of the street
(the PlayerStart end, a quarter, the middle looking at the shops, three quarters) and, for each
configuration, records SAMPLE_S of frame times per point after SETTLE_S. Configurations hide
actors by the tags place_props.py sets: GTP_Set_<props|details> and GTP_Cat_<catalogue id>.

  all           everything visible
  no_details    the City/Details set hidden
  no_props      the City/Props set hidden (furniture of props.py)
  no_<id>       one catalogue group of the details hidden, for the heaviest groups
Writes Saved/CityImport/details_profile.json: p50/p95 per configuration and the delta to "all".
"""

import json
import math
import os

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
OUT = os.path.join(REPO, "GranTheftPraline", "Saved", "CityImport")
WARMUP_S, SETTLE_S, SAMPLE_S = 15.0, 1.5, 4.0
POINTS = [(0.02, 0.0), (0.25, 0.0), (0.5, 65.0), (0.75, 0.0)]   # share of the street, look offset (deg)
GROUPS = ["W14-42", "W14-32", "W13-19", "W14-03", "W1-46", "W1-45", "W1-56", "W14-41", "W14-46", "W13-07",
          "W14-34", "W1-14"]

with open(os.path.join(REPO, "assets", "geo", "processed", ZONE, "gen", FOCUS, "report.json"), encoding="utf-8") as f:
    AXIS = json.load(f)["focus_axis_ue_cm"]
if AXIS[0][1] < AXIS[-1][1]:
    AXIS = list(reversed(AXIS))
SEG = [math.dist(a[:2], b[:2]) for a, b in zip(AXIS, AXIS[1:])]
LENGTH = sum(SEG)
CONFIGS = [("all", [])] + [("no_details", ["GTP_Set_details"]), ("no_props", ["GTP_Set_props"])] + \
    [("no_" + g, ["GTP_Cat_" + g]) for g in GROUPS] + [("all_again", [])]

state = dict(t=0.0, phase="warmup", phase_t=0.0, config=0, point=0, samples={}, cur=[], handle=None)


def log(msg):
    unreal.log_warning("[CityProfile] " + msg)


def controller():
    for pc in unreal.ObjectIterator(unreal.PlayerController):
        if pc.get_world() is not None and pc.is_local_player_controller():
            return pc
    return None


def pose(share):
    d = max(0.0, min(share * LENGTH, LENGTH - 1.0))
    for (a, b), L in zip(zip(AXIS, AXIS[1:]), SEG):
        if d <= L:
            t = d / L if L else 0.0
            return unreal.Vector(*(a[k] + (b[k] - a[k]) * t + (100.0 if k == 2 else 0.0) for k in range(3))), \
                math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        d -= L
    a, b = AXIS[-2], AXIS[-1]
    return unreal.Vector(b[0], b[1], b[2] + 100.0), math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def place(share, look):
    pc = controller()
    pawn = pc.get_controlled_pawn()
    loc, yaw = pose(share)
    pawn.set_actor_location_and_rotation(loc, unreal.Rotator(0.0, 0.0, yaw), False, True)
    pc.set_ignore_look_input(True)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=-6.0, yaw=yaw + look))


def apply(config):
    world = controller().get_world()
    hidden = set(CONFIGS[config][1])
    for tag in ["GTP_Set_details", "GTP_Set_props"] + ["GTP_Cat_" + g for g in GROUPS]:
        for a in unreal.GameplayStatics.get_all_actors_with_tag(world, tag):
            a.set_actor_hidden_in_game(False)
    for tag in hidden:
        for a in unreal.GameplayStatics.get_all_actors_with_tag(world, tag):
            a.set_actor_hidden_in_game(True)


def pct(v, p):
    v = sorted(v)
    return round(v[min(len(v) - 1, int(round(p / 100.0 * (len(v) - 1))))], 2) if v else None


def finish():
    res = {"configs": {}, "points": POINTS, "sample_s": SAMPLE_S, "profile": os.environ.get("GTP_PROFILE", "unknown"),
           "note": "editor -game frame times, camera still; compare deltas, not absolute values"}
    base = state["samples"].get("all", [])
    for name, _ in CONFIGS:
        f = state["samples"].get(name, [])
        res["configs"][name] = {"frames": len(f), "p50_ms": pct(f, 50), "p95_ms": pct(f, 95),
                                "delta_p50_ms": round(pct(f, 50) - pct(base, 50), 2) if f and base else None}
    with open(os.path.join(OUT, "details_profile.json"), "w") as fh:
        json.dump(res, fh, indent=2)
    log("report: " + json.dumps(res))
    unreal.unregister_slate_post_tick_callback(state["handle"])
    unreal.SystemLibrary.quit_game(controller().get_world(), None, unreal.QuitPreference.QUIT, False)


def go(phase):
    state["phase"], state["phase_t"] = phase, 0.0


def tick(dt):
    state["t"] += dt
    state["phase_t"] += dt
    pc = controller()
    if pc is None or pc.get_controlled_pawn() is None:
        if state["t"] > 240:
            log("no pawn after 240 s")
            unreal.unregister_slate_post_tick_callback(state["handle"])
        return
    ph = state["phase"]
    if ph == "warmup":
        place(*POINTS[0])
        if state["phase_t"] >= WARMUP_S:
            apply(0)
            go("settle")
    elif ph == "settle":
        place(*POINTS[state["point"]])
        if state["phase_t"] >= SETTLE_S:
            go("sample")
    elif ph == "sample":
        state["cur"].append(dt * 1000.0)
        if state["phase_t"] >= SAMPLE_S:
            name = CONFIGS[state["config"]][0]
            state["samples"].setdefault(name, []).extend(state["cur"])
            state["cur"] = []
            state["point"] += 1
            if state["point"] >= len(POINTS):
                state["point"], state["config"] = 0, state["config"] + 1
                if state["config"] >= len(CONFIGS):
                    return finish()
                apply(state["config"])
            go("settle")


os.makedirs(OUT, exist_ok=True)
state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("registered, %d configurations x %d points" % (len(CONFIGS), len(POINTS)))
