"""Place the wave-1 street furniture on the generated city map, inside the Unreal Editor.

Run after import_city.py and place_facades.py, same way (no UI, script copied to a
path without spaces). Input: gen/<focus>/props_placement.csv (tools/city/props.py),
the props imported by tools/assets/unreal/import_props.py under
/Game/Environment/Props/<Family>/<Asset>/SM_<Asset>_A (kept out of Git).

Steps (idempotent): open the map, delete folder City/Props, then one actor per
asset with a hierarchical instanced static mesh component (collision on), every
instance dropped onto the ground by a downward trace (terrain tiles have complex
collision since import_city.py). Assets not imported yet are listed in the
report and skipped. Report: Saved/CityImport/props_report.json.
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
MAP = "/Game/Maps/L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
TRACE_UP_CM, TRACE_DOWN_CM = 300.0, 1500.0
MAX_SNAP_CM = 250.0   # farther than this from the LiDAR ground: keep the LiDAR height

eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log_warning("[CityProps] " + msg)


def ground_z(world, x, y, z):
    res = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, z + TRACE_UP_CM), unreal.Vector(x, y, z - TRACE_DOWN_CM),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if isinstance(res, tuple):
        res = res[1] if res[0] else None
    if res is None:
        return None
    hz = res.to_tuple()[4].z
    return hz if abs(hz - z) <= MAX_SNAP_CM else None


def hism_actor(label, mesh):
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    actor.set_actor_label(label)
    actor.set_folder_path("City/Props")
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = sdl.get_object(sdl.get_data(handle))
    comp.set_static_mesh(mesh)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_collision_profile_name("BlockAll")
    return comp


def main():
    t0 = time.time()
    report = {"map": MAP, "errors": [], "missing_assets": [], "instances": {}}
    path = os.path.join(GEN, "props_placement.csv")
    if not os.path.exists(path):
        report["errors"].append("no props_placement.csv: run tools/city/props.py")
        return write(report, t0)
    with open(path, encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    level_sub.load_level(MAP)
    for a in actor_sub.get_all_level_actors():
        if str(a.get_folder_path()).startswith("City/Props"):
            actor_sub.destroy_actor(a)
    world = editor_sub.get_editor_world()
    comps, snapped, kept = {}, 0, 0
    for r in rows:
        p = r["asset_path"]
        if p not in comps:
            mesh = eal.load_asset(p) if eal.does_asset_exist(p) else None
            comps[p] = hism_actor("Props_" + r["asset"], mesh) if mesh else None
            if mesh is None:
                report["missing_assets"].append(r["catalog_id"] + " " + p)
        comp = comps[p]
        if comp is None:
            continue
        x, y, z = float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"])
        gz = ground_z(world, x, y, z)
        if gz is None:
            kept += 1
        else:
            z, snapped = gz, snapped + 1
        comp.add_instance(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, float(r["ue_yaw"])),
                                           unreal.Vector(1, 1, 1)), True)
        report["instances"][r["asset"]] = report["instances"].get(r["asset"], 0) + 1
    level_sub.save_current_level()
    report.update({"rows": len(rows), "placed": sum(report["instances"].values()), "snapped_to_ground": snapped,
                   "kept_lidar_height": kept})
    write(report, t0)


def write(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "props_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    log("report: " + json.dumps({k: v for k, v in report.items() if k != "instances"}))


main()
