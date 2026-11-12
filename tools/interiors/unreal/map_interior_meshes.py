"""Build the local class -> mesh map of the interiors from the assets imported in the project (step 3).

Run in a headless editor (-nullrhi -ExecutePythonScript through Content/Python/gtp_run.py) once free
packs are imported. For every class of
tools/interiors/catalog.json that lists keywords, looks for static meshes whose path (folders and
asset name) contains one of them as whole words (case, CamelCase, digits and separators ignored,
plural allowed), under the roots given
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
import re
import os
import sys

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "interiors"))
import interior_layout as il  # noqa: E402

ROOTS = [r.strip() for r in os.environ.get("GTP_INTERIOR_ROOTS", "/Game").split(",") if r.strip()]
# The generated interior levels are skipped, not the imported packs under /Game/Interiors/Free and /Fab.
SKIP = ("/Game/Interiors/L_", "/Game/Maps", "/Game/Developers")
# Packs whose licence is not recorded (credits blocked.txt from credits_from_manifest.py): never
# placed until the listing is checked. GTP_INTERIOR_EXCLUDE adds folders (comma separated).
BLOCKED = ("/Game/Interiors/Fab/ModernKitchenClutterPack",
           "/Game/Interiors/Free/Models/eu_guest_wc_Combined")   # a kit: its pieces are the objects
SKIP += BLOCKED + tuple(p.strip() for p in os.environ.get("GTP_INTERIOR_EXCLUDE", "").split(",") if p.strip())
eal = unreal.EditorAssetLibrary


def norm(text):
    """Words separated by single underscores, CamelCase and letter-digit boundaries split:
    "SM_WallHungToilet" -> "sm_wall_hung_toilet", "Food_Tray1" -> "food_tray_1"."""
    text = re.sub(r"([a-z])([A-Z])", r"\1_\2", text)
    text = re.sub(r"([A-Za-z])([0-9])", r"\1_\2", text)
    return "_" + "_".join(w for w in re.split(r"[^a-z0-9]+", text.lower()) if w) + "_"


def has_word(name, key):
    """The keyword as whole words of the name, plural allowed: "straw" is not "strawberry",
    "bun" is in "hamburger_buns"."""
    core = key.strip("_")
    return any("_" + core + end + "_" in name for end in ("", "s", "es"))


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


MODELS = "/Game/Interiors/Free/Models/"


def folder(path):
    """The model a mesh belongs to: /Game/Interiors/Free/Models/<name> for the free imports, even
    when the importer nested the meshes (<name>/<name>/StaticMeshes/Object_16); else its folder."""
    if path.startswith(MODELS):
        return MODELS + path[len(MODELS):].split("/", 1)[0]
    return path.rsplit("/", 1)[0]


def size_error(hit, dims):
    """How far the mesh is from the catalog size of the class: the mean |log| of the ratios of the
    sides, sorted, so a locker door (2 cm thick) or a handle loses against the whole object even
    when its largest side matches. Without catalog dims, the largest side against 1 m."""
    have = sorted(max(v, 0.02) for v in hit["size_m"])
    if not dims:
        return abs(math.log(have[-1] / 1.0))
    want = sorted(max(v, 0.02) for v in dims)
    return sum(abs(math.log(h / w)) for h, w in zip(have, want)) / 3.0


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
        # Words that disqualify a mesh for this class: packaging on a counter is new, not litter.
        avoid = [norm(w) for w in info.get("avoid", [])]
        choice = None
        for word in words:
            key = norm(word)
            hits = {}
            for name, path in found:
                if not has_word(name, key) or any(has_word(name, bad) for bad in avoid):
                    continue
                if has_word(norm(folder(path)), key):
                    # The keyword names the model (its folder): a glTF or FBX with several nodes is
                    # imported as one mesh per node, so the whole folder competes, not the piece whose
                    # name happens to come first.
                    for other in by_folder[folder(path)]:
                        hits[other] = True
                else:
                    hits[path] = True
            # A model combined by combine_multi_mesh.py stands for its pieces: they leave the race.
            hits = {p: True for p in hits if folder(p) + "_Combined" not in by_folder}
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
    # Every mesh seen, to tune the catalog keywords away from the machine that has the assets.
    with open(os.path.join(out, "meshes_seen.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(sorted(path for _name, path in found)) + "\n")
    with open(os.path.join(out, "mesh_map_report.json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1)
    unreal.log_warning("[Interior] mesh map: %d classes matched, %d unmatched, %d from multi-mesh folders"
                       % (len(mapping), len(report["unmatched"]), len(report["multi_mesh"])))


main()
