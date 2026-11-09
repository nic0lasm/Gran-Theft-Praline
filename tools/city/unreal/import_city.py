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
  1. import the building and terrain meshes into /Game/Environment/City/<Zone>/
  2. check every building mesh against the bounds computed by generate.py
     (catches an axis or pivot convention change of the glTF importer)
  3. new map /Game/Maps/L_City_<Focus>: sun, sky, fixed exposure, buildings
     and terrain placed at their Lambert-93 positions, a PlayerStart in the street
  4. report in Saved/CityImport/report.json
The facade module list (facade_modules.csv) is left for the facade kit / PCG
graph; this script only builds the greybox that already contains the modules.
"""

import csv
import json
import os
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
GEN = os.path.join(REPO, "assets", "geo", "processed", ZONE, "gen", FOCUS)
TERRAIN = os.path.join(REPO, "assets", "geo", "processed", ZONE, "terrain")
CAMEL = "".join(p.capitalize() for p in FOCUS.split("-"))
ROOT = "/Game/Environment/City/" + "".join(p.capitalize() for p in ZONE.split("-"))
MAP = "/Game/Maps/L_City_" + CAMEL
BOUNDS_TOLERANCE_CM = 30.0

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


def check_bounds(mesh, expected):
    """True if the imported mesh has the pivot-relative bounds generate.py wrote."""
    box = mesh.get_bounding_box()
    got = [box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z]
    return max(abs(g - e) for g, e in zip(got, expected)) <= BOUNDS_TOLERANCE_CM, got


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


def main():
    t0 = time.time()
    report = {"zone": ZONE, "focus": FOCUS, "map": MAP, "errors": []}
    with open(os.path.join(GEN, "buildings_gen.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    with open(os.path.join(TERRAIN, "terrain.json")) as f:
        terrain_meta = json.load(f)

    buildings = import_glb(os.path.join(GEN, "greybox.glb"), ROOT + "/Buildings/" + CAMEL)
    terrain = import_glb(os.path.join(TERRAIN, "terrain.glb"), ROOT + "/Terrain")

    # Convention check before anything is placed.
    bad = []
    for r in rows:
        m = buildings.get(r["mesh_name"])
        if m is None:
            bad.append((r["mesh_name"], "missing"))
            continue
        ok, got = check_bounds(m, [float(v) for v in r["bounds_ue_cm"].split()])
        if not ok:
            bad.append((r["mesh_name"], [round(v) for v in got]))
    report["buildings_expected"] = len(rows)
    report["bounds_mismatch"] = len(bad)
    if bad:
        report["errors"].append("bounds mismatch, first: %s" % (bad[:3],))
        log("STOP: %d meshes do not match their expected bounds (axis/pivot convention?): %s" % (len(bad), bad[:3]))
        write_report(report, t0)
        return

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

    for r in rows:
        place_mesh(buildings[r["mesh_name"]], (float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"])),
                   r["mesh_name"], "City/Buildings")
    for tile in terrain_meta["mesh_tiles"]:
        m = terrain.get(tile["name"])
        if m is None:
            report["errors"].append("terrain tile missing: " + tile["name"])
            continue
        place_mesh(m, tuple(tile["ue_cm"]), tile["name"], "City/Terrain")

    # Player start: middle of the focus street axis, facing north (-Y in Unreal).
    with open(os.path.join(GEN, "report.json"), encoding="utf-8") as f:
        axis = json.load(f)["focus_axis_ue_cm"]
    x, y, z = axis[len(axis) // 2]
    spawn(unreal.PlayerStart, (x, y, z + 120.0), unreal.Rotator(0, 0, -90), "PlayerStart_City")

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
