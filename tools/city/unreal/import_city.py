"""Build the generated city greybox level inside the Unreal Editor.

Input (made in the cloud or locally by tools/city/, never committed):
  assets/geo/processed/<zone>/gen/<focus>/greybox.glb + buildings_gen.csv + facade_modules.csv
  assets/geo/processed/<zone>/terrain/terrain.glb + terrain.json

Run without the UI (UE 5.8 on Mac: no UnrealEditor-Cmd; copy the script to a
path without spaces first, as for tools/level/build_playable_street.py):
  UnrealEditor GranTheftPraline.uproject -unattended -nosplash \
      "-ExecutePythonScript=__import__('gtp_run').main()" (with GTP_PY_SCRIPT=tools/city/unreal/import_city.py)
Settings come from the environment: GTP_CITY_ZONE (presqu-ile), GTP_CITY_FOCUS
(rue-de-la-republique).

Steps (idempotent, the map is rebuilt each run; nothing else is touched):
  1. import the building meshes (shell.glb by default, GTP_CITY_MESH=greybox for the
     version with modules baked in) and the terrain meshes into /Game/Environment/City/<Zone>/
  2. check every building mesh against the bounds computed by generate.py
     (catches an axis or pivot convention change of the glTF importer)
  3. collisions (complex as simple) on buildings and terrain; new map
     /Game/Maps/L_City_<Focus> with the lighting of the reference scene
     (GTP_CITY_LIGHT = Noon | GoldenHour | Rain | Night, art bible states), buildings
     and terrain at their Lambert-93 positions, a PlayerStart at the Bellecour end
  3b. optional Rhone riverside (riverside.glb + riverside.json from tools/city/riverside.py):
     water without collision, bridge deck and quay parapets with collision, folder City/Riverside
  4. report in Saved/CityImport/report.json
The facade module list (facade_modules.csv) is left for the facade kit / PCG
graph; this script only builds the greybox that already contains the modules.
"""

import csv
import json
import math
import os
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
GEN = os.path.join(REPO, "assets", "geo", "processed", ZONE, "gen", FOCUS)
TERRAIN = os.path.join(REPO, "assets", "geo", "processed", ZONE, "terrain")
# "shell" (roofs and party walls; place_facades.py adds the kit) or "greybox" (modules baked in)
MESH = os.environ.get("GTP_CITY_MESH", "shell")
CAMEL = "".join(p.capitalize() for p in FOCUS.split("-"))
ROOT = "/Game/Environment/City/" + "".join(p.capitalize() for p in ZONE.split("-"))
MAP = "/Game/Maps/L_City_" + CAMEL
BOUNDS_TOLERANCE_CM = 30.0
# Lighting state of the art bible, taken unchanged from the reference scene (rendering thread):
# Noon (default), GoldenHour, Rain or Night. Falls back to a plain sun and sky if missing.
LIGHT_STATE = os.environ.get("GTP_CITY_LIGHT", "Noon")
LIGHT_SOURCE_MAP = "/Game/ReferenceScene/Maps/ReferenceStreet_" + LIGHT_STATE
LIGHT_CLASSES = (unreal.DirectionalLight, unreal.SkyLight, unreal.SkyAtmosphere, unreal.ExponentialHeightFog,
                 unreal.VolumetricCloud, unreal.PostProcessVolume, unreal.Light)
KILL_Z = -5000.0
SPAWN_ALONG_AXIS_CM = 800.0   # start 8 m into the street from its Bellecour end

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    # Warning level: plain Log lines are hidden when the editor runs a script without UI.
    unreal.log_warning("[CityImport] " + msg)


def import_glb(path, dest):
    if eal.does_directory_exist(dest):
        eal.delete_directory(dest)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    asset_tools.import_asset_tasks([task])
    meshes = {}
    for p in eal.list_assets(dest, recursive=True):
        a = eal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            meshes[a.get_name()] = a
    log("%s: %d static meshes" % (os.path.basename(path), len(meshes)))
    return meshes


def set_complex_collision(meshes):
    """Complex-as-simple collision: the pawn walks on the terrain and is stopped by the walls."""
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    done = 0
    for mesh in meshes:
        body = mesh.get_editor_property("body_setup")
        if body is None:
            continue
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.set_editor_property("body_setup", body)
        for section in range(mesh.get_num_sections(0)):
            sm_sub.enable_section_collision(mesh, True, 0, section)
        eal.save_loaded_asset(mesh)
        done += 1
    return done


def open_level_with_bible_lighting(report):
    """Copy the reference scene map of the chosen lighting state, keep only its lighting.

    load + save_map, never duplicate_asset (crashes UE 5.8, see build_playable_street.py)."""
    editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if eal.does_asset_exist(MAP):
        eal.delete_asset(MAP)
    if eal.does_asset_exist(LIGHT_SOURCE_MAP) and level_sub.load_level(LIGHT_SOURCE_MAP) and \
            unreal.EditorLoadingAndSavingUtils.save_map(editor_sub.get_editor_world(), MAP) and \
            level_sub.load_level(MAP):
        kept = 0
        for a in actor_sub.get_all_level_actors():
            if isinstance(a, LIGHT_CLASSES) or isinstance(a, unreal.WorldSettings):
                kept += 1
                continue
            actor_sub.destroy_actor(a)
        report["lighting"] = {"source": LIGHT_SOURCE_MAP, "actors_kept": kept}
        log("lighting from %s (%d actors kept)" % (LIGHT_SOURCE_MAP, kept))
        return
    report["lighting"] = {"source": "fallback sun and sky", "missing": LIGHT_SOURCE_MAP}
    log("%s missing: plain sun and sky" % LIGHT_SOURCE_MAP)
    level_sub.new_level(MAP)
    sun = spawn(unreal.DirectionalLight, (0, 0, 10000), unreal.Rotator(0, -40, 135), "Sun", "Lighting")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere, (0, 0, 0), None, "SkyAtmosphere", "Lighting")
    sky = spawn(unreal.SkyLight, (0, 0, 2000), None, "SkyLight", "Lighting")
    sky.light_component.set_editor_property("real_time_capture", True)
    pp = spawn(unreal.PostProcessVolume, (0, 0, 0), None, "PostProcess_Greybox", "Lighting")
    pp.set_editor_property("unbound", True)
    s = pp.get_editor_property("settings")
    s.set_editor_property("override_auto_exposure_method", True)
    s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    s.set_editor_property("override_auto_exposure_bias", True)
    s.set_editor_property("auto_exposure_bias", 14.0)  # EV100 midday range of the art bible
    pp.set_editor_property("settings", s)


def check_bounds(mesh, expected, offset):
    """Compare the imported bounds with the pivot-relative bounds generate.py wrote.

    Returns (ok, baked, got). The UE 5.8 Interchange glTF importer bakes the node
    translation into the vertices (pivot at the world origin): the bounds then match
    the expected ones shifted by the building position, and the mesh is placed at 0."""
    box = mesh.get_bounding_box()
    got = [box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z]
    if max(abs(g - e) for g, e in zip(got, expected)) <= BOUNDS_TOLERANCE_CM:
        return True, False, got
    shifted = [e + offset[i % 3] for i, e in enumerate(expected)]
    if max(abs(g - e) for g, e in zip(got, shifted)) <= BOUNDS_TOLERANCE_CM:
        return True, True, got
    return False, False, got


def spawn(cls, loc, rot=None, label=None, folder=None):
    a = actor_sub.spawn_actor_from_class(cls, unreal.Vector(*loc), rot or unreal.Rotator(0, 0, 0))
    if label:
        a.set_actor_label(label)
    if folder:
        a.set_folder_path(folder)
    return a


def place_mesh(mesh, loc, label, folder):
    a = spawn(unreal.StaticMeshActor, loc, None, label, folder)
    comp = a.static_mesh_component
    comp.set_static_mesh(mesh)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    return a


def bellecour_start(axis):
    """Start a few metres into the street from its Bellecour (south, larger Y) end, facing up it."""
    pts = axis if axis[0][1] > axis[-1][1] else list(reversed(axis))
    (x0, y0, z0), (x1, y1, _) = pts[0], pts[1]
    dist = math.hypot(x1 - x0, y1 - y0) or 1.0
    t = min(1.0, SPAWN_ALONG_AXIS_CM / dist)
    loc = (x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + 120.0)
    return loc, math.degrees(math.atan2(y1 - y0, x1 - x0))


def terrain_baked(mesh, tile):
    """True if the terrain tile came in with its translation baked (bounds far from its pivot)."""
    box = mesh.get_bounding_box()
    cx, cy = (box.min.x + box.max.x) / 2, (box.min.y + box.max.y) / 2
    tx, ty = tile["ue_cm"][0], tile["ue_cm"][1]
    return math.hypot(cx - tx, cy - ty) < math.hypot(cx, cy)


def main():
    t0 = time.time()
    report = {"zone": ZONE, "focus": FOCUS, "map": MAP, "errors": []}
    with open(os.path.join(GEN, "buildings_gen.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    with open(os.path.join(TERRAIN, "terrain.json")) as f:
        terrain_meta = json.load(f)

    buildings = import_glb(os.path.join(GEN, MESH + ".glb"), ROOT + "/Buildings/" + CAMEL)
    prefix, bounds_col = ("SM_BuildingShell_", "shell_bounds_ue_cm") if MESH == "shell" else \
        ("SM_Building_", "bounds_ue_cm")
    for r in rows:
        r["mesh_name"] = r["mesh_name"].replace("SM_Building_", prefix)
        r["bounds_ue_cm"] = r[bounds_col]
    terrain = import_glb(os.path.join(TERRAIN, "terrain.glb"), ROOT + "/Terrain")

    # Convention check before anything is placed.
    bad, baked = [], 0
    for r in rows:
        m = buildings.get(r["mesh_name"])
        if m is None:
            bad.append((r["mesh_name"], "missing"))
            continue
        pos = (float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"]))
        ok, is_baked, got = check_bounds(m, [float(v) for v in r["bounds_ue_cm"].split()], pos)
        if not ok:
            bad.append((r["mesh_name"], [round(v) for v in got]))
        r["baked"] = is_baked
        baked += is_baked
    report["buildings_expected"] = len(rows)
    report["bounds_mismatch"] = len(bad)
    report["meshes_with_baked_position"] = baked
    if bad:
        report["errors"].append("bounds mismatch, first: %s" % (bad[:3],))
        log("STOP: %d meshes do not match their expected bounds (axis/pivot convention?): %s" % (len(bad), bad[:3]))
        write_report(report, t0)
        return

    # Optional Rhone riverside (tools/city/riverside.py): water, bridge deck, quay parapets.
    river_meta, river = None, {}
    if os.path.exists(os.path.join(GEN, "riverside.json")):
        with open(os.path.join(GEN, "riverside.json")) as f:
            river_meta = json.load(f)
        river = import_glb(os.path.join(GEN, "riverside.glb"), ROOT + "/Riverside/" + CAMEL)
    solid = [river[n["name"]] for n in (river_meta or {}).get("nodes", []) if n["collision"] and n["name"] in river]
    # Optional landmarks (tools/city/landmarks.py): Opera, Hotel de Ville... LiDAR volumes that
    # replace the generic shells of their BD TOPO buildings. Origin at the zone origin.
    mark_meta, marks = None, {}
    if os.path.exists(os.path.join(GEN, "landmarks.json")):
        with open(os.path.join(GEN, "landmarks.json"), encoding="utf-8") as f:
            mark_meta = json.load(f)
        marks = import_glb(os.path.join(GEN, "landmarks.glb"), ROOT + "/Landmarks/" + CAMEL)
        solid += [marks[n["name"]] for n in mark_meta["nodes"] if n["name"] in marks]
    # GTP_SHELL_MERGE=1: the shells merged per 250 m cell (generate.py shell_cells.glb), one
    # mesh and one draw per cell instead of one per building (ADR-0010, fil optimisation).
    cells_meta, cells = None, {}
    if MESH == "shell" and os.environ.get("GTP_SHELL_MERGE", "0") == "1" and \
            os.path.exists(os.path.join(GEN, "shell_cells.json")):
        with open(os.path.join(GEN, "shell_cells.json"), encoding="utf-8") as f:
            cells_meta = json.load(f)
        cells = import_glb(os.path.join(GEN, "shell_cells.glb"), ROOT + "/Buildings/" + CAMEL + "/Cells")
    shells = list(cells.values()) if cells_meta else list(buildings.values())
    report["collision_meshes"] = set_complex_collision(shells + list(terrain.values()) + solid)
    open_level_with_bible_lighting(report)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world() \
        .get_world_settings().set_editor_property("kill_z", KILL_Z)

    if cells_meta:
        for n in cells_meta["nodes"]:
            m = cells.get(n["name"])
            if m is None:
                report["errors"].append("shell cell mesh missing: " + n["name"])
                continue
            pos = (0.0, 0.0, 0.0) if terrain_baked(m, n) else tuple(n["ue_cm"])
            place_mesh(m, pos, n["name"], "City/Buildings")
        report["shell_cells"] = len(cells)
    else:
        for r in rows:
            pos = (0.0, 0.0, 0.0) if r["baked"] else (float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"]))
            place_mesh(buildings[r["mesh_name"]], pos, r["mesh_name"], "City/Buildings")
    terrain_check = {"baked": 0, "at_pivot": 0}
    for tile in terrain_meta["mesh_tiles"]:
        m = terrain.get(tile["name"])
        if m is None:
            report["errors"].append("terrain tile missing: " + tile["name"])
            continue
        if terrain_baked(m, tile):
            terrain_check["baked"] += 1
            pos = (0.0, 0.0, 0.0)
        else:
            terrain_check["at_pivot"] += 1
            pos = tuple(tile["ue_cm"])
        place_mesh(m, pos, tile["name"], "City/Terrain")
    report["terrain_bounds_check"] = terrain_check
    # Riverside nodes are written with their origin at the zone origin: placed at 0 either way.
    for n in (river_meta or {}).get("nodes", []):
        m = river.get(n["name"])
        if m is None:
            report["errors"].append("riverside mesh missing: " + n["name"])
            continue
        a = place_mesh(m, (0.0, 0.0, 0.0), n["name"], "City/Riverside")
        if not n["collision"]:
            # set_collision_enabled alone is runtime state and was not saved with the map:
            # write the profile on the component's body instance so it persists.
            comp = a.static_mesh_component
            comp.modify()
            comp.set_collision_profile_name("NoCollision")
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    report["riverside"] = sorted(river)
    for n in (mark_meta or {}).get("nodes", []):
        m = marks.get(n["name"])
        if m is None:
            report["errors"].append("landmark mesh missing: " + n["name"])
            continue
        place_mesh(m, (0.0, 0.0, 0.0), n["name"], "City/Landmarks")
    report["landmarks"] = sorted(marks)
    # Fall-in safety (riverside.py "recovery"): tagged trigger boxes over the water and quay
    # exits at street level; the game swims or fishes out whoever enters them.
    rec = (river_meta or {}).get("recovery")
    if rec:
        for i, b in enumerate(rec["boxes"]):
            c, h = b["centre_m"], b["half_m"]
            t = spawn(unreal.TriggerBox, (c[0] * 100.0, -c[1] * 100.0, c[2] * 100.0), None, "RiverRecovery_%03d" % i, "City/Riverside")
            t.collision_component.set_editor_property("box_extent", unreal.Vector(h[0] * 100, h[1] * 100, h[2] * 100))
            t.collision_component.set_collision_profile_name("OverlapAllDynamic")   # pawns and Chaos vehicles
            t.collision_component.set_editor_property("generate_overlap_events", True)
            t.tags = [rec["tag_boxes"]]
        for i, e in enumerate(rec["exits"]):
            # Unreal yaw is clockwise from +X with Y south: local yaw (counter-clockwise, y north) flips.
            t = spawn(unreal.TargetPoint, (e["x_m"] * 100.0, -e["y_m"] * 100.0, (e["z_m"] + 1.0) * 100.0),
                      unreal.Rotator(0, 0, -e["yaw_deg"]), "RiverExit_%02d" % i, "City/Riverside")
            t.tags = [rec["tag_exits"]]
        report["river_recovery"] = {"boxes": len(rec["boxes"]), "exits": len(rec["exits"]), "bed_z_m": rec["bed_z_m"]}

    # Player start: Bellecour end of the focus street, looking up the street (morning test map).
    with open(os.path.join(GEN, "report.json"), encoding="utf-8") as f:
        axis = json.load(f)["focus_axis_ue_cm"]
    loc, yaw = bellecour_start(axis)
    spawn(unreal.PlayerStart, loc, unreal.Rotator(0, 0, yaw), "PlayerStart_City")
    report["player_start"] = [round(v) for v in loc] + [round(yaw, 1)]

    level_sub.save_current_level()
    report.update({"buildings_placed": len(rows), "terrain_tiles": len(terrain),
                   "landscape_settings": terrain_meta})
    write_report(report, t0)


def write_report(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "report.json"), "w") as f:
        json.dump(report, f, indent=2)
    log("report: " + json.dumps(report))


main()
