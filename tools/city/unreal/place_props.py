"""Place the wave-1 street furniture on the generated city map, inside the Unreal Editor.

Run after import_city.py and place_facades.py, same way (no UI, script copied to a
path without spaces). Input: gen/<focus>/props_placement.csv (tools/city/props.py),
the props imported by tools/assets/unreal/import_props.py under
/Game/Environment/Props/<Family>/<Asset>/SM_<Asset>_A (kept out of Git).

Steps (idempotent): open the map, delete folder City/Props, then one actor per
asset with a hierarchical instanced static mesh component (collision on), every
instance dropped onto the ground by a downward trace (terrain tiles have complex
collision since import_city.py). Wave-1 rows name their asset path; any
row is also matched by its catalogue id through the CatalogId metadata tag that
import_props.py sets, so later waves (14...) appear as soon as they are imported.
Assets not imported yet are listed in the report and skipped. Report: Saved/CityImport/props_report.json.
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
MAX_SNAP_CM = 250.0
# One run per placement table: the street furniture (default) and the facade and street
# details of tools/city/details.py (GTP_PROPS_SET=details). Each set owns its outliner folder,
# so placing one never removes the other.
PROPS_SET = os.environ.get("GTP_PROPS_SET", "props")
FOLDER = "City/" + PROPS_SET.capitalize()
CSV_NAME = PROPS_SET + "_placement.csv"
PROPS_ROOT = "/Game/Environment/Props"

# Draw distances by prop size (largest bounding box side), metres, and per set (fil
# optimisation: numbers to tune after the m1 measure). Culling starts at CULL_START_SHARE of
# the end. Props smaller than NO_SHADOW_BELOW_M cast no dynamic shadow.
CULL_END_M = {"props": {"small": 30.0, "medium": 60.0, "large": 120.0},   # fil optimisation 10/11
              "details": {"small": 40.0, "medium": 60.0, "large": 120.0}}
SIZE_CLASSES_M = (("small", 1.0), ("medium", 3.0))   # up to 1 m small, up to 3 m medium
CULL_START_SHARE = 0.8
NO_SHADOW_BELOW_M = 0.5


def tune_render(comp, mesh, report):
    """Cull distance and shadows from the mesh size; LOD check for the optimisation thread."""
    ext = mesh.get_bounds().box_extent
    size_m = 2.0 * max(ext.x, ext.y, ext.z) / 100.0
    cls = next((c for c, lim in SIZE_CLASSES_M if size_m <= lim), "large")
    end = CULL_END_M.get(PROPS_SET, CULL_END_M["props"])[cls] * 100.0
    comp.set_editor_property("instance_start_cull_distance", int(end * CULL_START_SHARE))
    comp.set_editor_property("instance_end_cull_distance", int(end))
    if size_m < NO_SHADOW_BELOW_M:
        comp.set_editor_property("cast_shadow", False)
    tally = report.setdefault("cull_classes", {})
    tally[cls] = tally.get(cls, 0) + 1
    if mesh.get_num_lods() < 2:
        report.setdefault("meshes_without_lods", []).append(mesh.get_name())   # farther than this from the LiDAR ground: keep the LiDAR height

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
    actor.set_folder_path(FOLDER)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = sdl.get_object(sdl.get_data(handle))
    comp.set_static_mesh(mesh)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_collision_profile_name("BlockAll")
    return comp


def catalog_index():
    """CatalogId tag (set by import_props.py) -> static mesh, for every imported prop."""
    out = {}
    for path in eal.list_assets(PROPS_ROOT, recursive=True, include_folder=False):
        a = eal.load_asset(path)
        tag = eal.get_metadata_tag(a, "CatalogId") if isinstance(a, unreal.StaticMesh) else ""
        if tag and tag not in out:
            out[tag] = path
    return out


def main():
    t0 = time.time()
    report = {"map": MAP, "errors": [], "missing_assets": [], "instances": {}}
    path = os.path.join(GEN, CSV_NAME)
    if not os.path.exists(path):
        report["errors"].append("no %s: run tools/city/%s.py" % (CSV_NAME, PROPS_SET))
        return write(report, t0)
    with open(path, encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    level_sub.load_level(MAP)
    for a in actor_sub.get_all_level_actors():
        if str(a.get_folder_path()).startswith(FOLDER):
            actor_sub.destroy_actor(a)
    world = editor_sub.get_editor_world()
    by_id = catalog_index() if eal.does_directory_exist(PROPS_ROOT) else {}
    report["imported_catalog_ids"] = len(by_id)
    comps, snapped, kept, no_collision = {}, 0, 0, set()
    for r in rows:
        p = r["asset_path"] if r["asset_path"] and eal.does_asset_exist(r["asset_path"]) else \
            by_id.get(r["catalog_id"], "")
        key = p or r["catalog_id"]
        if key not in comps:
            mesh = eal.load_asset(p) if p else None
            comps[key] = hism_actor("Props_" + r["catalog_id"] + "_" + (r["asset"] or mesh.get_name() if mesh else ""),
                                    mesh) if mesh else None
            if mesh is not None:
                tune_render(comps[key], mesh, report)
            if mesh is None:
                report["missing_assets"].append(r["catalog_id"])
        comp = comps[key]
        if comp is None:
            continue
        if r.get("collision") == "false" and key not in no_collision:
            comp.set_collision_profile_name("NoCollision")   # small details: nothing to walk into
            no_collision.add(key)
        x, y, z = float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"])
        # Wall and roof details (snap=keep) keep their height; everything else drops onto the ground.
        gz = None if r.get("snap") == "keep" else ground_z(world, x, y, z)
        if gz is None:
            kept += 1
        else:
            z, snapped = gz, snapped + 1
        scale = unreal.Vector(float(r.get("scale_x") or 1), float(r.get("scale_y") or 1), float(r.get("scale_z") or 1))
        comp.add_instance(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, float(r["ue_yaw"])),
                                           scale), True)
        report["instances"][r["catalog_id"]] = report["instances"].get(r["catalog_id"], 0) + 1
    level_sub.save_current_level()
    report.update({"rows": len(rows), "placed": sum(report["instances"].values()), "snapped_to_ground": snapped,
                   "kept_lidar_height": kept})
    write(report, t0)


def write(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, PROPS_SET + "_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    log("report: " + json.dumps({k: v for k, v in report.items() if k != "instances"}))


main()
