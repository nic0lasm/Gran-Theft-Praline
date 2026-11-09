"""Build the playable level L_RueRepublique (version 0.1) inside the Unreal Editor.

Prerequisite: tools/rendering/reference_scene/build_reference_scene.py has run,
so /Game/ReferenceScene/Geo and /Game/ReferenceScene/Maps/ReferenceStreet_Noon
exist. Lighting and post-process come from that map unchanged: this script
only adds what makes the street playable.

Run from the editor (Tools > Execute Python Script) or headless:
  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
      -script="<repo>/tools/level/build_playable_street.py"

Steps (idempotent, the map is rebuilt from scratch each run):
  1. collisions on the greybox meshes (buildings and ground block, roads do not)
  2. /Game/Maps/L_RueRepublique duplicated from ReferenceStreet_Noon
  3. camera path actor removed, PlayerStart placed on the street axis
  4. invisible walls around the playable area, kill-Z below the ground
  5. checks by line traces, report written to Saved/PlayableStreet/report.json
"""

import json
import math
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
GEO_NAME = "rue-republique"
STREET = "Rue de la République"
GEO_DIR = "/Game/ReferenceScene/Geo"
SOURCE_MAP = "/Game/ReferenceScene/Maps/ReferenceStreet_Noon"
TARGET_MAP = "/Game/Maps/L_RueRepublique"
CUBE = "/Engine/BasicShapes/Cube.Cube"

# Gameplay limits, in centimetres.
KILL_Z = -3000.0           # 30 m below the reference ground
WALL_HEIGHT = 6000.0       # above the tallest building (35.4 m)
WALL_THICKNESS = 100.0
WALL_INSET = 50.0          # walls stand just inside the ground edges
SPAWN_ALONG_AXIS = 500.0   # start 5 m into the street, away from the bbox edge
SPAWN_ABOVE_GROUND = 100.0
NO_COLLISION_MESHES = ("Roads",)  # 2 cm ribbons on top of the ground

eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log("[PlayableStreet] " + msg)


def warn(msg):
    unreal.log_warning("[PlayableStreet] " + msg)


def load_meta():
    path = os.path.join(REPO, "assets", "geo", "processed", GEO_NAME, GEO_NAME + ".meta.json")
    with open(path) as f:
        return json.load(f)


# --------------------------------------------------------------------------- collisions

def set_mesh_collision(mesh, enabled):
    """Complex-as-simple: the extruded footprints (courtyards, L shapes) block exactly."""
    body = mesh.get_editor_property("body_setup")
    if body is None:
        warn("%s has no body setup" % mesh.get_name())
        return False
    if enabled:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        body.set_editor_property("collision_response", unreal.BodyCollisionResponse.BODY_COLLISION_ENABLED)
    else:
        body.set_editor_property("collision_response", unreal.BodyCollisionResponse.BODY_COLLISION_DISABLED)
    mesh.set_editor_property("body_setup", body)
    return True


def setup_collisions():
    done = 0
    for path in eal.list_assets(GEO_DIR, recursive=True):
        mesh = eal.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        enabled = mesh.get_name() not in NO_COLLISION_MESHES
        if set_mesh_collision(mesh, enabled):
            eal.save_loaded_asset(mesh)
            done += 1
    log("collision set on %d meshes" % done)
    return done


# --------------------------------------------------------------------------- level

def spawn(cls, loc, rot=None, label=None):
    actor = actor_sub.spawn_actor_from_class(cls, unreal.Vector(*loc), rot or unreal.Rotator(0, 0, 0))
    if label:
        actor.set_actor_label(label)
    return actor


def duplicate_map():
    if not eal.does_asset_exist(SOURCE_MAP):
        raise RuntimeError("%s is missing: run tools/rendering/reference_scene/build_reference_scene.py first"
                           % SOURCE_MAP)
    if eal.does_asset_exist(TARGET_MAP):
        eal.delete_asset(TARGET_MAP)
    if not eal.duplicate_asset(SOURCE_MAP, TARGET_MAP):
        # Fallback: open the source map and save it under the new name.
        level_sub.load_level(SOURCE_MAP)
        if not unreal.EditorLoadingAndSavingUtils.save_map(editor_sub.get_editor_world(), TARGET_MAP):
            raise RuntimeError("could not duplicate %s" % SOURCE_MAP)
    if not level_sub.load_level(TARGET_MAP):
        raise RuntimeError("could not load %s" % TARGET_MAP)
    log("duplicated %s -> %s" % (SOURCE_MAP, TARGET_MAP))


def remove_actors():
    removed = []
    for actor in actor_sub.get_all_level_actors():
        if isinstance(actor, (unreal.LevelSequenceActor, unreal.PlayerStart)):
            removed.append(actor.get_actor_label())
            actor_sub.destroy_actor(actor)
    log("removed: " + ", ".join(removed))


def find_actor(label):
    for actor in actor_sub.get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def ground_box():
    """(min_x, min_y, max_x, max_y, top_z) of the Ground actor, in cm."""
    ground = find_actor("Ground")
    if ground is None:
        raise RuntimeError("no actor labelled Ground in %s" % TARGET_MAP)
    origin, extent = ground.get_actor_bounds(False)
    return (origin.x - extent.x, origin.y - extent.y, origin.x + extent.x, origin.y + extent.y,
            origin.z + extent.z)


def trace(world, start, end, ignore=()):
    """Line trace on the visibility channel; returns the hit result or None."""
    res = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(*start), unreal.Vector(*end),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, list(ignore), unreal.DrawDebugTrace.NONE, True)
    if isinstance(res, tuple):  # (bool, HitResult) in some engine versions
        return res[1] if res[0] else None
    return res


def hit_location(hit):
    t = hit.to_tuple()
    # HitResult tuple: (blocking_hit, initial_overlap, time, distance, location, impact_point, ...)
    return t[4]


def place_player_start(world, axis, top_z):
    (x0, y0), (x1, y1) = axis[0], axis[1]
    heading = math.atan2(y1 - y0, x1 - x0)
    x = x0 + math.cos(heading) * SPAWN_ALONG_AXIS
    y = y0 + math.sin(heading) * SPAWN_ALONG_AXIS
    hit = trace(world, (x, y, top_z + 5000.0), (x, y, KILL_Z))
    z = (hit_location(hit).z if hit else top_z) + SPAWN_ABOVE_GROUND
    ps = spawn(unreal.PlayerStart, (x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=math.degrees(heading)), "PlayerStart")
    log("PlayerStart at (%.0f, %.0f, %.0f), yaw %.1f" % (x, y, z, math.degrees(heading)))
    return ps


def place_bounds(box):
    min_x, min_y, max_x, max_y, top_z = box
    cube = eal.load_asset(CUBE)
    min_x += WALL_INSET
    min_y += WALL_INSET
    max_x -= WALL_INSET
    max_y -= WALL_INSET
    cx, cy = (min_x + max_x) / 2, (min_y + max_y) / 2
    sx, sy = max_x - min_x, max_y - min_y
    z = top_z + WALL_HEIGHT / 2 - 200.0  # sink 2 m into the ground, no gap at the base
    walls = {
        "Bounds_North": ((cx, min_y), (sx, WALL_THICKNESS)),
        "Bounds_South": ((cx, max_y), (sx, WALL_THICKNESS)),
        "Bounds_West": ((min_x, cy), (WALL_THICKNESS, sy)),
        "Bounds_East": ((max_x, cy), (WALL_THICKNESS, sy)),
    }
    for label, ((x, y), (wx, wy)) in walls.items():
        actor = spawn(unreal.StaticMeshActor, (x, y, z), label=label)
        comp = actor.static_mesh_component
        comp.set_static_mesh(cube)
        actor.set_actor_scale3d(unreal.Vector(wx / 100.0, wy / 100.0, WALL_HEIGHT / 100.0))
        comp.set_collision_profile_name("BlockAll")
        comp.set_editor_property("cast_shadow", False)
        actor.set_actor_hidden_in_game(True)
        actor.set_folder_path("Gameplay/Bounds")
        actor.set_editor_property("tags", [unreal.Name("PlayBounds")])
    log("bounds %.0f x %.0f m" % (sx / 100.0, sy / 100.0))
    return (min_x, min_y, max_x, max_y)


def set_world_settings(world):
    ws = world.get_world_settings()
    ws.set_editor_property("kill_z", KILL_Z)
    ws.set_editor_property("enable_world_bounds_checks", True)
    log("kill-Z %.0f cm" % KILL_Z)


# --------------------------------------------------------------------------- checks

def resample(points, step):
    out = []
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        seg = math.hypot(x1 - x0, y1 - y0)
        n = max(1, int(seg // step))
        h = math.atan2(y1 - y0, x1 - x0)
        for i in range(n):
            f = i / n
            out.append((x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, h))
    return out


def run_checks(world, axis, ps, bounds, top_z):
    checks = {}
    loc = ps.get_actor_location()

    hit = trace(world, (loc.x, loc.y, loc.z), (loc.x, loc.y, loc.z - 1000.0), [ps])
    gap = loc.z - hit_location(hit).z if hit else None
    checks["spawn_on_ground"] = dict(ok=bool(hit) and 0 < gap <= 300.0, gap_cm=gap)

    hit = trace(world, (loc.x, loc.y, loc.z + 2000.0), (loc.x, loc.y, loc.z), [ps])
    checks["spawn_not_under_roof"] = dict(ok=hit is None)

    # Facades on both sides of the street block a pedestrian-height trace.
    samples = resample(axis, 1000.0)
    blocked = 0
    for x, y, h in samples:
        for side in (1, -1):
            nx, ny = -math.sin(h) * side, math.cos(h) * side
            if trace(world, (x, y, top_z + 150.0), (x + nx * 4000.0, y + ny * 4000.0, top_z + 150.0)):
                blocked += 1
    ratio = blocked / float(2 * len(samples))
    checks["facades_block"] = dict(ok=ratio >= 0.8, ratio=round(ratio, 3), traces=2 * len(samples))

    # Every wall blocks a trace leaving the playable area from its centre.
    min_x, min_y, max_x, max_y = bounds
    cx, cy, z = (min_x + max_x) / 2, (min_y + max_y) / 2, top_z + 4500.0  # above roofs
    far = 100000.0
    walls_ok = all(trace(world, (cx, cy, z), end) for end in
                   ((cx, cy - far, z), (cx, cy + far, z), (cx - far, cy, z), (cx + far, cy, z)))
    checks["bounds_block"] = dict(ok=walls_ok)

    ws = world.get_world_settings()
    checks["kill_z"] = dict(ok=abs(ws.get_editor_property("kill_z") - KILL_Z) < 1.0)

    ok = all(c["ok"] for c in checks.values())
    report = dict(map=TARGET_MAP, result="PASS" if ok else "FAIL", checks=checks,
                  player_start=[round(loc.x), round(loc.y), round(loc.z)])
    out_dir = os.path.join(REPO, "GranTheftPraline", "Saved", "PlayableStreet")
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "report.json"), "w") as f:
        json.dump(report, f, indent=1)
    for name, c in checks.items():
        (log if c["ok"] else warn)("%s %s %s" % ("PASS" if c["ok"] else "FAIL", name, c))
    log("RESULT " + report["result"])
    return ok


def main():
    meta = load_meta()
    axis = [tuple(p) for p in meta["streets"][STREET]["axis_ue_cm"]]
    setup_collisions()
    duplicate_map()
    remove_actors()
    world = editor_sub.get_editor_world()
    box = ground_box()
    ps = place_player_start(world, axis, box[4])
    bounds = place_bounds(box)
    set_world_settings(world)
    level_sub.save_current_level()
    log("saved " + TARGET_MAP)
    run_checks(world, axis, ps, bounds, box[4])


main()
