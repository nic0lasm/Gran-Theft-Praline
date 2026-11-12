"""Build the local class -> mesh map of the interiors from the assets imported in the project (step 3).

Run in a headless editor (-nullrhi -ExecutePythonScript through Content/Python/gtp_run.py) once free
packs are imported. For every class of
tools/interiors/catalog.json that lists keywords, looks for static meshes whose path (folders and
asset name) contains one of them (words compared without case, spaces and dashes as underscores), under the roots given
by GTP_INTERIOR_ROOTS (comma separated, default /Game). The first keyword wins over the next ones.
When the keyword names a model folder, every mesh of that folder competes (a multi-node glTF or FBX
is imported as one mesh per node). Inside a keyword, the mesh closest to the catalog size (dims_m)
wins, then one with LODs, then the fewest LOD0 triangles (the M1 Mac draws the LOD chain, Nanite is
inactive there). The report lists the classes whose mesh comes from a multi-mesh folder (multi_mesh):
those place one piece of the model until it is imported as one combined mesh. Writes Saved/InteriorBuild/mesh_map.json
({class: mesh path}) and mesh_map_report.json (candidates, unmatched classes). The map stays local:
it names assets that are not in Git. build_interior_level.py reads it through GTP_INTERIOR_MESHES.
"""

import json
import math
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


def triangles(mesh):
    """LOD0 triangles, from the mesh itself (UE 5.8: StaticMeshEditorSubsystem has no
    get_number_triangles, and it is None in the pythonscript commandlet)."""
    return mesh.get_num_triangles(0)


def describe(path):
    mesh = eal.load_asset(path)
    box = mesh.get_bounding_box()
    size = [abs(box.max.x - box.min.x) / 100.0, abs(box.max.y - box.min.y) / 100.0, abs(box.max.z - box.min.z) / 100.0]
    return {"path": path, "lods": mesh.get_num_lods(), "tris_lod0": triangles(mesh), "size_m": [round(v, 3) for v in size]}


def folder(path):
    return path.rsplit("/", 1)[0]


def size_error(hit, dims):
    """How far the mesh is from the catalog size of the class: |log| of the ratio of the largest
    sides, so a handle or a drawer of a 1 m object scores badly against the whole object."""
    want = max(dims or [0.0]) or 1.0
    have = max(hit["size_m"]) or 1e-3
    return abs(math.log(have / want))


def main():
    catalog = il.load_json(il.DEFAULT_CATALOG)
    found = meshes()
    by_folder = {}
    for name, path in found:
        by_folder.setdefault(folder(path), []).append(path)
    cache = {}

    def info_of(path):
        if path not in cache:
            cache[path] = describe(path)
        return cache[path]

    mapping, report = {}, {"roots": ROOTS, "meshes_seen": len(found), "classes": {}, "unmatched": [], "multi_mesh": []}
    for cls, info in sorted(catalog["classes"].items()):
        words = info.get("keywords", [])
        dims = info.get("dims_m", [])
        choice = None
        for word in words:
            key = norm(word)
            hits = {}
            for name, path in found:
                if key not in name:
                    continue
                if key in norm(folder(path)):
                    # The keyword names the model (its folder): a glTF or FBX with several nodes is
                    # imported as one mesh per node, so the whole folder competes, not the piece whose
                    # name happens to come first.
                    for other in by_folder[folder(path)]:
                        hits[other] = True
                else:
                    hits[path] = True
            if hits:
                described = [info_of(path) for path in hits]
                # Closest to the catalog size first (pieces and other objects lose), then LODs, then
                # fewer triangles.
                described.sort(key=lambda h: (round(size_error(h, dims), 1), h["lods"] < 2, h["tris_lod0"]))
                choice = described[0]
                siblings = len(by_folder[folder(choice["path"])])
                report["classes"][cls] = {"keyword": word, "chosen": choice, "candidates": len(described),
                                          "meshes_in_its_folder": siblings, "catalog_size_m": dims}
                if siblings > 1:
                    report["multi_mesh"].append(cls)
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
    unreal.log_warning("[Interior] mesh map: %d classes matched, %d unmatched, %d from multi-mesh folders"
                       % (len(mapping), len(report["unmatched"]), len(report["multi_mesh"])))


main()
