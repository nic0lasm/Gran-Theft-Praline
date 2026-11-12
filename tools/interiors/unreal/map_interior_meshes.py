"""Build the local class -> mesh map of the interiors from the assets imported in the project (step 3).

Run in the Unreal Editor (pythonscript, headless) once free packs are imported. For every class of
tools/interiors/catalog.json that lists keywords, looks for static meshes whose path (folders and
asset name) contains one of them (words compared without case, spaces and dashes as underscores), under the roots given
by GTP_INTERIOR_ROOTS (comma separated, default /Game). The first keyword wins over the next ones,
and inside a keyword the mesh with the fewest LOD0 triangles that still has LODs is preferred (the
M1 Mac draws the LOD chain, Nanite is inactive there). Writes Saved/InteriorBuild/mesh_map.json
({class: mesh path}) and mesh_map_report.json (candidates, unmatched classes). The map stays local:
it names assets that are not in Git. build_interior_level.py reads it through GTP_INTERIOR_MESHES.
"""

import json
import os
import sys

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "interiors"))
import interior_layout as il  # noqa: E402

ROOTS = [r.strip() for r in os.environ.get("GTP_INTERIOR_ROOTS", "/Game").split(",") if r.strip()]
# The generated interior levels are skipped, not the imported packs under /Game/Interiors/Free and /Fab.
SKIP = ("/Game/Interiors/L_", "/Game/Maps", "/Game/Developers")
eal = unreal.EditorAssetLibrary


def norm(text):
    return text.lower().replace(" ", "_").replace("-", "_")


def meshes():
    out = []
    for root in ROOTS:
        for path in eal.list_assets(root, recursive=True, include_folder=False):
            if path.startswith(SKIP):
                continue
            data = eal.find_asset_data(path)
            if str(data.asset_class_path.asset_name) != "StaticMesh":
                continue
            # Folder and asset name: imported packs keep the product name in the folder
            # (/Game/Interiors/Free/Models/Urban_Trash_Cup_Paper_01/SM_...), Megascans name assets by id.
            out.append((norm(path.split(".")[0]), path.split(".")[0]))
    return out


def describe(path):
    mesh = eal.load_asset(path)
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    return {"path": path, "lods": mesh.get_num_lods(), "tris_lod0": sub.get_number_triangles(mesh, 0)}


def main():
    catalog = il.load_json(il.DEFAULT_CATALOG)
    found = meshes()
    mapping, report = {}, {"roots": ROOTS, "meshes_seen": len(found), "classes": {}, "unmatched": []}
    for cls, info in sorted(catalog["classes"].items()):
        words = info.get("keywords", [])
        choice = None
        for word in words:
            hits = [describe(path) for name, path in found if norm(word) in name]
            if hits:
                hits.sort(key=lambda h: (h["lods"] < 2, h["tris_lod0"]))
                choice = hits[0]
                report["classes"][cls] = {"keyword": word, "chosen": choice, "candidates": len(hits)}
                break
        if choice:
            mapping[cls] = choice["path"]
        else:
            report["unmatched"].append(cls)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "InteriorBuild")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "mesh_map.json"), "w", encoding="utf-8") as handle:
        json.dump(mapping, handle, indent=1)
    with open(os.path.join(out, "mesh_map_report.json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1)
    unreal.log_warning("[Interior] mesh map: %d classes matched, %d unmatched" % (len(mapping), len(report["unmatched"])))


main()
