"""Make the Roadside Construction props affordable on M1, before any of them is placed.

Runs inside the Unreal Editor (no UI): -ExecutePythonScript=<copy of this file>.
The pack (Fab, out of Git) holds Nanite scans of millions of triangles and 4K/8K
textures. On a GPU without Nanite the engine draws an uncontrolled fallback, so
each prop kept gets three plain LODs from the triangle budgets of
tools/assets/manifest.py (M1) and its textures are capped at 2048 px, the most
the M1 profile loads. Only the pack's own assets change; no map is touched.

Environment:
  GTP_ROADSIDE_MODE   list (default: report only) | apply
  GTP_ROADSIDE_ROOT   default /Game/Scene_RoadsideConstruction
  GTP_ROADSIDE_KEEP   regular expression on mesh names: the props worth keeping
                      for a Lyon street (default: cones, delineators, barriers,
                      jersey blocks, curbs, cement bags, cable wheels, toolboxes)
  GTP_ROADSIDE_LODS   triangles per LOD, default "5000 1500 400"
  GTP_ROADSIDE_TEX    texture cap in px, default 2048

Report: Saved/CityImport/roadside_report.json (triangles before and after,
Nanite, texture sizes). A Fab re-download restores the pack as it came.
"""

import json
import os
import re
import time

import unreal

ROOT = os.environ.get("GTP_ROADSIDE_ROOT", "/Game/Scene_RoadsideConstruction")
KEEP = os.environ.get("GTP_ROADSIDE_KEEP",
                      r"TrafficCone|Delineator|Barrier|Jersey|Curb|Bag_Cement|CableWheel|Toolbox")
LODS = [int(v) for v in os.environ.get("GTP_ROADSIDE_LODS", "5000 1500 400").split()]
TEX_CAP = int(os.environ.get("GTP_ROADSIDE_TEX", "2048"))
SCREEN_SIZES = [1.0, 0.25, 0.08]
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = os.path.join(SAVED, "CityImport", "roadside_report.json")

eal = unreal.EditorAssetLibrary
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def safe(fn, default=None):
    try:
        return fn()
    except Exception:
        return default


def tris(mesh):
    n = safe(lambda: sm_sub.get_lod_count(mesh), 0) or 0
    return [safe(lambda i=i: mesh.get_num_triangles(i), -1) for i in range(n)]


def reduce_mesh(mesh, report_row):
    """Nanite off, LOD 0..n rebuilt from the source at the budgeted triangle counts."""
    source = max(report_row["triangles_before"][0] if report_row["triangles_before"] else 0, 1)
    settings = mesh.get_editor_property("nanite_settings")
    if settings.enabled:
        settings.enabled = False
        mesh.set_editor_property("nanite_settings", settings)
    options = unreal.EditorScriptingMeshReductionOptions()
    options.auto_compute_lod_screen_size = False
    options.reduction_settings = [
        unreal.EditorScriptingMeshReductionSettings(min(1.0, target / float(source)), screen)
        for target, screen in zip(LODS, SCREEN_SIZES)
    ]
    sm_sub.set_lods_with_notification(mesh, options, False)
    eal.save_loaded_asset(mesh)
    report_row["triangles_after"] = tris(mesh)


def main():
    t0 = time.time()
    mode = os.environ.get("GTP_ROADSIDE_MODE", "list")
    rx = re.compile(KEEP, re.IGNORECASE)
    report = {"root": ROOT, "mode": mode, "keep": KEEP, "lods": LODS, "texture_cap": TEX_CAP,
              "meshes": [], "textures_capped": [], "errors": []}
    if not eal.does_directory_exist(ROOT):
        report["errors"].append("no pack at " + ROOT)
    else:
        for path in eal.list_assets(ROOT, recursive=True, include_folder=False):
            cls = str(eal.find_asset_data(path).asset_class_path.asset_name)
            name = path.split("/")[-1].split(".")[0]
            if cls == "StaticMesh" and rx.search(name):
                mesh = eal.load_asset(path)
                row = {"path": path.split(".")[0], "triangles_before": tris(mesh),
                       "nanite": safe(lambda: bool(mesh.get_editor_property("nanite_settings").enabled), False)}
                if mode == "apply":
                    try:
                        reduce_mesh(mesh, row)
                    except Exception as e:
                        report["errors"].append("%s: %s" % (name, e))
                report["meshes"].append(row)
            elif cls == "Texture2D":
                tex = eal.load_asset(path)
                w = safe(lambda: tex.blueprint_get_size_x(), 0)
                h = safe(lambda: tex.blueprint_get_size_y(), 0)
                if max(w, h) > TEX_CAP:
                    report["textures_capped"].append({"path": path.split(".")[0], "size": [w, h]})
                    if mode == "apply":
                        tex.set_editor_property("max_texture_size", TEX_CAP)
                        eal.save_loaded_asset(tex)
    report["seconds"] = round(time.time() - t0, 1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(report, f, indent=2)
    unreal.log("prepare_roadside: %d meshes, %d textures over %d px, %d errors"
               % (len(report["meshes"]), len(report["textures_capped"]), TEX_CAP, len(report["errors"])))


main()
