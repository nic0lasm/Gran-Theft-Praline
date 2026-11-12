"""Bring the meshes chosen for the interiors within their budget (step 3, after map_interior_meshes.py).

Run in a headless editor (-nullrhi -ExecutePythonScript through Content/Python/gtp_run.py): the
static mesh editor subsystem is None in the pythonscript commandlet. Reads Saved/InteriorBuild/mesh_map.json and, for
every mesh under /Game/Interiors/Free or /Game/Interiors/Fab (nothing else is ever modified):
  - triangles: Nanite off and LOD0 rebuilt from the source first (with Nanite on, LOD0 is only
    the Nanite fallback), then LOD0 reduced to the class budget of tools/interiors/catalog.json (tris_lod0, 25 %
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
    """Textures of every material slot. get_used_textures takes a UMaterial only: a material
    instance gives its base material's textures plus its own texture parameter values."""
    found = {}
    mel = unreal.MaterialEditingLibrary
    for index in range(len(mesh.static_materials)):
        material = mesh.get_material(index)
        if material is None:
            continue
        base = material.get_base_material() if isinstance(material, unreal.MaterialInstance) else material
        textures = list(mel.get_used_textures(base)) if isinstance(base, unreal.Material) else []
        if isinstance(material, unreal.MaterialInstanceConstant):
            try:
                for name in mel.get_texture_parameter_names(base):
                    value = mel.get_material_instance_texture_parameter_value(material, name)
                    if value is not None:
                        textures.append(value)
            except Exception as error:   # an API difference costs the overrides, not the run
                log("texture parameters of %s not read: %s" % (material.get_path_name(), error))
        for texture in textures:
            if isinstance(texture, unreal.Texture2D):
                found[texture.get_path_name()] = texture
    return list(found.values())


def budget_mesh(cls, info, path, report):
    mesh = eal.load_asset(path)
    if mesh is None:
        report["errors"].append("cannot load " + path)
        return
    nanite = mesh.get_editor_property("nanite_settings")
    entry = {"path": path, "size": info["size"], "budget": info["tris_lod0"], "tris_before": mesh.get_num_triangles(0),
             "lods_before": mesh.get_num_lods(), "nanite_before": bool(nanite.enabled), "textures": 0,
             "texture_mb_before": 0.0, "texture_mb_after": 0.0}
    budget = info["tris_lod0"]
    cap = TEXTURE_CAP[info["size"]]
    textures = textures_of(mesh)
    entry["textures"] = len(textures)
    for texture in textures:
        sx, sy = texture.blueprint_get_size_x(), texture.blueprint_get_size_y()
        entry["texture_mb_before"] += texture_mb(sx, sy)
        entry["texture_mb_after"] += texture_mb(sx, sy, cap)

    def keep_for(tris):
        return max(0.001, float(budget) / tris) if tris > budget * TOLERANCE else 1.0

    if APPLY:
        # The source counts, not LOD0 as it stands: with Nanite on, LOD0 is the Nanite fallback (a
        # 542 k triangle scanned cup showed 2.6 k), and an earlier reduction would be counted twice.
        # Nanite off and LOD0 rebuilt at 100 % give the source; the chain is then cut from it.
        if nanite.enabled:
            nanite.enabled = False
            mesh.set_editor_property("nanite_settings", nanite)
        full = unreal.StaticMeshReductionOptions(auto_compute_lod_screen_size=True,
                                                 reduction_settings=[unreal.StaticMeshReductionSettings(percent_triangles=1.0, screen_size=1.0)])
        sub.set_lods(mesh, full)
        entry["tris_source"] = mesh.get_num_triangles(0)
        keep = keep_for(entry["tris_source"])
        settings = [unreal.StaticMeshReductionSettings(percent_triangles=keep, screen_size=1.0)]
        settings += [unreal.StaticMeshReductionSettings(percent_triangles=keep * share, screen_size=0.0) for share in LOD_CHAIN]
        options = unreal.StaticMeshReductionOptions(auto_compute_lod_screen_size=True, reduction_settings=settings)
        sub.set_lods(mesh, options)
        eal.save_loaded_asset(mesh)
        if mesh.get_num_triangles(0) > budget * TOLERANCE * 1.1:
            report["warnings"].append("%s: LOD0 %d triangles after the budget, over %d" % (path, mesh.get_num_triangles(0), budget))
        for texture in textures:
            current = texture.get_editor_property("max_texture_size")
            if current == 0 or current > cap:
                texture.set_editor_property("max_texture_size", cap)
                eal.save_loaded_asset(texture)
    else:
        keep = keep_for(entry["tris_before"])
        if nanite.enabled:
            entry["note"] = "Nanite on: tris_before is the Nanite fallback, the source is larger (known after apply)"
    entry["tris_after"] = mesh.get_num_triangles(0) if APPLY else int(entry["tris_before"] * keep)
    entry["lods_after"] = mesh.get_num_lods() if APPLY else 1 + len(LOD_CHAIN)
    entry["texture_mb_before"] = round(entry["texture_mb_before"], 1)
    entry["texture_mb_after"] = round(entry["texture_mb_after"], 1)
    report["classes"][cls] = entry


def main():
    catalog = il.load_json(il.DEFAULT_CATALOG)
    report = {"apply": APPLY, "classes": {}, "skipped": {}, "errors": [], "warnings": []}
    if sub is None:
        # None in the pythonscript commandlet: this script needs a headless editor.
        report["errors"].append("StaticMeshEditorSubsystem is None: run in the editor (-nullrhi -ExecutePythonScript), not -run=pythonscript")
        os.makedirs(SAVED, exist_ok=True)
        with open(os.path.join(SAVED, "mesh_budget_report.json"), "w", encoding="utf-8") as handle:
            json.dump(report, handle, indent=1)
        log(report["errors"][0])
        return
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
        "over_budget_before": sum(1 for e in entries if e.get("tris_source", e["tris_before"]) > e["budget"] * TOLERANCE),
        "tris_before": sum(e["tris_before"] for e in entries),
        "tris_source": sum(e.get("tris_source", e["tris_before"]) for e in entries),
        "nanite_meshes": sum(1 for e in entries if e["nanite_before"]),
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
