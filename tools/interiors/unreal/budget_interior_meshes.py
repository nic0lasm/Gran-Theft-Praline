"""Bring the meshes chosen for the interiors within their budget (step 3, after map_interior_meshes.py).

Run in the Unreal Editor (pythonscript, headless). Reads Saved/InteriorBuild/mesh_map.json and, for
every mesh under /Game/Interiors/Free or /Game/Interiors/Fab (nothing else is ever modified):
  - triangles: LOD0 reduced to the class budget of tools/interiors/catalog.json (tris_lod0, 25 %
    tolerance), then a chain of 3 LODs (50 %, 25 %, 10 %): the M1 draws the LOD chain;
  - Nanite off on these props: the M1 has no Nanite, and LODs keep both machines alike;
  - textures: largest mip capped by the size class of the object (micro 256, small 512, medium
    1024, large 2048 px), which is what keeps 300-600 MB Megascans scans in a shop of 9 500 objects;
  - report Saved/InteriorBuild/mesh_budget_report.json: per class tris before and after, textures
    capped, an estimate of the texture memory before and after (BC compression, with mips).
GTP_BUDGET_APPLY=0 writes the report without changing anything. A re-import restores an asset.
"""

import json
import os
import sys

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "interiors"))
import interior_layout as il  # noqa: E402

SAVED = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "InteriorBuild")
APPLY = os.environ.get("GTP_BUDGET_APPLY", "1") == "1"
ALLOWED = ("/Game/Interiors/Free/", "/Game/Interiors/Fab/")
TEXTURE_CAP = {"micro": 256, "small": 512, "medium": 1024, "large": 2048}
TOLERANCE = 1.25
LOD_CHAIN = (0.5, 0.25, 0.1)
eal = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def log(text):
    unreal.log_warning("[InteriorBudget] " + text)


def texture_mb(size_x, size_y, cap=None):
    """BC7/BC5-like 1 byte per texel, a third more for mips."""
    if cap:
        scale = min(1.0, float(cap) / max(size_x, size_y, 1))
        size_x, size_y = max(1, int(size_x * scale)), max(1, int(size_y * scale))
    return size_x * size_y * 4.0 / 3.0 / (1024 * 1024)


def textures_of(mesh):
    found = {}
    for index in range(len(mesh.static_materials)):
        material = mesh.get_material(index)
        if material is None:
            continue
        for texture in unreal.MaterialEditingLibrary.get_used_textures(material):
            if isinstance(texture, unreal.Texture2D):
                found[texture.get_path_name()] = texture
    return list(found.values())


def budget_mesh(cls, info, path, report):
    mesh = eal.load_asset(path)
    if mesh is None:
        report["errors"].append("cannot load " + path)
        return
    entry = {"path": path, "size": info["size"], "budget": info["tris_lod0"], "tris_before": sub.get_number_triangles(mesh, 0),
             "lods_before": mesh.get_num_lods(), "textures": 0, "texture_mb_before": 0.0, "texture_mb_after": 0.0}
    budget = info["tris_lod0"]
    keep = 1.0
    if entry["tris_before"] > budget * TOLERANCE:
        keep = max(0.01, float(budget) / entry["tris_before"])
    cap = TEXTURE_CAP[info["size"]]
    textures = textures_of(mesh)
    entry["textures"] = len(textures)
    for texture in textures:
        sx, sy = texture.blueprint_get_size_x(), texture.blueprint_get_size_y()
        entry["texture_mb_before"] += texture_mb(sx, sy)
        entry["texture_mb_after"] += texture_mb(sx, sy, cap)
    if APPLY:
        settings = [unreal.StaticMeshReductionSettings(percent_triangles=keep, screen_size=1.0)]
        settings += [unreal.StaticMeshReductionSettings(percent_triangles=keep * share, screen_size=0.0) for share in LOD_CHAIN]
        options = unreal.StaticMeshReductionOptions(auto_compute_lod_screen_size=True, reduction_settings=settings)
        sub.set_lods(mesh, options)
        nanite = mesh.get_editor_property("nanite_settings")
        if nanite.enabled:
            nanite.enabled = False
            mesh.set_editor_property("nanite_settings", nanite)
        eal.save_loaded_asset(mesh)
        for texture in textures:
            current = texture.get_editor_property("max_texture_size")
            if current == 0 or current > cap:
                texture.set_editor_property("max_texture_size", cap)
                eal.save_loaded_asset(texture)
    entry["tris_after"] = sub.get_number_triangles(mesh, 0) if APPLY else int(entry["tris_before"] * keep)
    entry["lods_after"] = mesh.get_num_lods() if APPLY else 1 + len(LOD_CHAIN)
    entry["texture_mb_before"] = round(entry["texture_mb_before"], 1)
    entry["texture_mb_after"] = round(entry["texture_mb_after"], 1)
    report["classes"][cls] = entry


def main():
    catalog = il.load_json(il.DEFAULT_CATALOG)
    report = {"apply": APPLY, "classes": {}, "skipped": {}, "errors": []}
    map_path = os.path.join(SAVED, "mesh_map.json")
    mapping = il.load_json(map_path) if os.path.exists(map_path) else {}
    if not mapping:
        report["errors"].append("no mesh map at " + map_path + " (run map_interior_meshes.py first)")
    for cls, path in sorted(mapping.items()):
        if not path.startswith(ALLOWED):
            report["skipped"][cls] = path + " (outside /Game/Interiors/Free and /Fab: left as it is)"
            continue
        if cls not in catalog["classes"]:
            report["skipped"][cls] = "not in the catalog"
            continue
        budget_mesh(cls, catalog["classes"][cls], path, report)
    entries = report["classes"].values()
    report["totals"] = {
        "meshes": len(report["classes"]),
        "over_budget_before": sum(1 for e in entries if e["tris_before"] > e["budget"] * TOLERANCE),
        "tris_before": sum(e["tris_before"] for e in entries),
        "tris_after": sum(e["tris_after"] for e in entries),
        "texture_mb_before": round(sum(e["texture_mb_before"] for e in entries), 1),
        "texture_mb_after": round(sum(e["texture_mb_after"] for e in entries), 1),
    }
    os.makedirs(SAVED, exist_ok=True)
    with open(os.path.join(SAVED, "mesh_budget_report.json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1)
    log("%(meshes)d meshes, %(over_budget_before)d over budget, tris %(tris_before)d -> %(tris_after)d, "
        "textures %(texture_mb_before).0f -> %(texture_mb_after).0f MB" % report["totals"])


main()
