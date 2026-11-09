"""Build the generated city greybox level inside the Unreal Editor.

Input (made in the cloud or locally by tools/city/, never committed):
  assets/geo/processed/<zone>/gen/<focus>/greybox.glb + buildings_gen.csv + facade_modules.csv
  assets/geo/processed/<zone>/terrain/terrain.glb + terrain.json

Run without the UI (UE 5.8 on Mac: no UnrealEditor-Cmd; copy the script to a
path without spaces first, as for tools/level/build_playable_street.py):
  UnrealEditor GranTheftPraline.uproject -unattended -nosplash \
      -ExecutePythonScript=/tmp/import_city.py
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
    report["collision_meshes"] = set_complex_collision(list(buildings.values()) + list(terrain.values()))
    open_level_with_bible_lighting(report)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world() \
        .get_world_settings().set_editor_property("kill_z", KILL_Z)

    for r in rows:
        place_mesh(buildings[r["mesh_name"]], (float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"])),
                   r["mesh_name"], "City/Buildings")
    for tile in terrain_meta["mesh_tiles"]:
        m = terrain.get(tile["name"])
        if m is None:
            report["errors"].append("terrain tile missing: " + tile["name"])
            continue
        place_mesh(m, tuple(tile["ue_cm"]), tile["name"], "City/Terrain")

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
