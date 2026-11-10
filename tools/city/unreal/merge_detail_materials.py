"""Bake every multi-material street detail into a one-material mesh, inside the Unreal Editor.

Without Nanite (the M1), each material slot of an instanced component is one more draw per
pass (base pass, every shadow page). Most catalogue details have 2 to 4 slots for a few
hundred triangles. This script spawns one actor per detail mesh named in
gen/<focus>/details_placement.csv, merges it with material baking (one atlas material, same
look at the distances where details are drawn) and saves the result as
``<asset folder>/SM_<Asset>_Merged``, with the LOD chain of the source rebuilt on it.
place_props.py uses the merged mesh when GTP_DETAILS_MERGED=1 and it exists.

Idempotent (an existing merged mesh is replaced). Meshes with one slot are left alone.
Run like the other city scripts (editor, no UI). Report: Saved/CityImport/merge_details_report.json.
Env: GTP_MERGE_TEXTURE (atlas size, default 512), GTP_MERGE_IDS (comma list, default all).
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
TEXTURE = int(os.environ.get("GTP_MERGE_TEXTURE", "512"))
ONLY = {t.strip() for t in os.environ.get("GTP_MERGE_IDS", "").split(",") if t.strip()}
SUFFIX = "_Merged"

eal = unreal.EditorAssetLibrary
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def log(msg):
    unreal.log_warning("[MergeDetails] " + msg)


def merged_path(mesh_path):
    """/Game/.../SM_Plaque_A -> /Game/.../SM_Plaque_Merged (asset name, no object suffix)."""
    folder, name = mesh_path.rsplit("/", 1)
    name = name.split(".")[0]
    base = name[:-2] if name.endswith("_A") else name
    return folder + "/" + base + SUFFIX


def lod_plan(mesh):
    """Screen sizes and shares of the source LODs (by vertex count), to rebuild them on the merge.
    No collision is built: details are placed with collision=false."""
    n = sm_sub.get_lod_count(mesh)
    tris0 = max(1, sm_sub.get_number_verts(mesh, 0))
    plan = []
    for i in range(n):
        s = unreal.EditorScriptingMeshReductionSettings()
        s.set_editor_property("percent_triangles", min(1.0, sm_sub.get_number_verts(mesh, i) / tris0))
        s.set_editor_property("screen_size", [1.0, 0.5, 0.25, 0.1, 0.05][min(i, 4)])
        plan.append(s)
    return plan


def merge_one(mesh, dest):
    actor = actor_sub.spawn_actor_from_object(mesh, unreal.Vector(0, 0, -100000))
    try:
        opts = unreal.MergeStaticMeshActorsOptions()
        opts.set_editor_property("destroy_source_actors", False)
        opts.set_editor_property("spawn_merged_actor", False)
        opts.set_editor_property("base_package_name", dest)
        ms = opts.get_editor_property("mesh_merging_settings")
        ms.set_editor_property("merge_materials", True)
        ms.set_editor_property("lod_selection_type", unreal.MeshLODSelectionType.SPECIFIC_LOD)
        ms.set_editor_property("specific_lod", 0)
        ms.set_editor_property("pivot_point_at_zero", False)
        mat = ms.get_editor_property("material_settings")
        mat.set_editor_property("texture_size", unreal.IntPoint(TEXTURE, TEXTURE))
        ms.set_editor_property("material_settings", mat)
        opts.set_editor_property("mesh_merging_settings", ms)
        res = sm_sub.merge_static_mesh_actors([actor], opts)
        ok = res[0] if isinstance(res, tuple) else bool(res)
    finally:
        actor_sub.destroy_actor(actor)
    if not ok or not eal.does_asset_exist(dest):
        return None
    merged = eal.load_asset(dest)
    plan = lod_plan(mesh)
    if len(plan) > 1:
        lods = unreal.EditorScriptingMeshReductionOptions()
        lods.set_editor_property("auto_compute_lod_screen_size", False)
        lods.set_editor_property("reduction_settings", plan)
        sm_sub.set_lods(merged, lods)
    eal.set_metadata_tag(merged, "MergedFrom", mesh.get_path_name())
    eal.save_asset(dest)
    return merged


def main():
    t0 = time.time()
    report = {"merged": {}, "single_slot": [], "failed": [], "texture": TEXTURE}
    paths = {}
    with open(os.path.join(GEN, "details_placement.csv"), newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            if r["asset_path"] and (not ONLY or r["catalog_id"] in ONLY):
                paths.setdefault(r["asset_path"], r["catalog_id"])
    for path, cid in sorted(paths.items()):
        if not eal.does_asset_exist(path):
            continue
        mesh = eal.load_asset(path)
        slots = len(mesh.get_editor_property("static_materials"))
        if slots <= 1:
            report["single_slot"].append(cid)
            continue
        dest = merged_path(path)
        if eal.does_asset_exist(dest):
            eal.delete_asset(dest)
        if merge_one(mesh, dest) is None:
            report["failed"].append(cid)
            log("merge failed: " + path)
        else:
            report["merged"][cid] = {"slots_before": slots, "mesh": dest}
            log("%s: %d slots -> 1 (%s)" % (cid, slots, dest))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "merge_details_report.json"), "w") as f:
        json.dump(report, f, indent=1)
    log("done: %d merged, %d single-slot, %d failed" % (len(report["merged"]), len(report["single_slot"]),
                                                        len(report["failed"])))


main()
