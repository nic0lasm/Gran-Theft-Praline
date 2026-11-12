"""Find the scanned street surfaces in the project and get them ready for the M1, in the Unreal Editor.

For each surface of tools/city/surfaces.json, the textures are looked up in the project:
first a folder whose path holds the id (/Game/Environment/Surfaces/<id>/ by convention), then a
folder whose path holds every word of "match" (Megascans keeps its own names). Loose exports can
be imported too: GTP_SURFACES_DIR=<folder> with one sub-folder per id holding the images
(BaseColor/Albedo/_D, Normal/_N, ORDp/ORM or Roughness), imported to /Game/Environment/Surfaces/<id>/.

Each texture found is set for the M1 and re-saved only when something changes: at most 2K
(max_texture_size, GTP_SURFACE_MAX), colour sRGB, normal map compression, roughness linear masks.
Nothing is created but those textures: no material (gtp_surfaces.py builds the layers inside the
city materials, which carry their usage flags from creation).

Writes Saved/CityImport/surfaces.json {"surfaces": {id: {basecolor, normal, rough, rough_channel,
tile_cm, folder}}, "missing": [...], "changed": [...]}: apply_city_materials.py then layers them in.
Env: GTP_SURFACE_ROOTS (default "/Game"), GTP_SURFACE_MAX (2048), GTP_SURFACES_DIR, GTP_SURFACE_IDS.
Run with tools/city/unreal/surfaces_mac.sh.
"""

import json
import os
import re
import sys
import time

import unreal

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")))
from surface_files import IMAGE, loose_images as _loose_images, norm as _norm, role_of, word_sets  # noqa: E402,F401

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
CATALOG = os.path.join(REPO, "tools", "city", "surfaces.json")
ROOTS = os.environ.get("GTP_SURFACE_ROOTS", "/Game").split()
MAX = int(os.environ.get("GTP_SURFACE_MAX", "2048"))
LOOSE = os.environ.get("GTP_SURFACES_DIR", "")
ONLY = {s for s in os.environ.get("GTP_SURFACE_IDS", "").replace(",", " ").split() if s}
DEST = "/Game/Environment/Surfaces"
SKIP = ("/Game/Environment/City/", "/Game/Environment/Facades/", "/Game/Maps/", "/Game/Environment/Weather/")

eal = unreal.EditorAssetLibrary



def log(msg):
    unreal.log_warning("[Surfaces] " + msg)


def textures_by_folder():
    """{folder: [texture object paths]} for every Texture2D under the roots (city output skipped)."""
    out = {}
    for root in ROOTS:
        if not eal.does_directory_exist(root):
            continue
        for p in eal.list_assets(root, recursive=True, include_folder=False):
            if p.startswith(SKIP):
                continue
            data = eal.find_asset_data(p)
            if str(data.asset_class_path.asset_name) != "Texture2D":
                continue
            out.setdefault(p.rsplit("/", 1)[0], []).append(p.split(".")[0])
    return out


def find_folder(entry, folders):
    sid = entry["id"].lower()
    by_id = sorted(f for f in folders if sid in f.lower())
    if by_id:
        return by_id[0]
    for words in word_sets(entry):
        hits = sorted(f for f in folders if all(_norm(w) in _norm(f) for w in words))
        if hits:
            return hits[0]
    return None


def loose_images(entry):
    return _loose_images(entry, LOOSE)


def import_loose(entry):
    tasks = []
    images = loose_images(entry)
    folder = DEST + "/" + entry["id"]
    if images and eal.does_directory_exist(folder):
        # Our own import folder only: a source that changed (Street Cobblestone replaced by
        # Cobblestone Floor, 12/11) must not leave its old maps for the role lookup to pick.
        eal.delete_directory(folder)
    for path in images:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", path)
        t.set_editor_property("destination_path", DEST + "/" + entry["id"])
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        log("%s: %d loose images imported" % (entry["id"], len(tasks)))


def prepare(path, role, changed):
    """2K cap, colour space and compression for its role; save only when something changed."""
    tex = eal.load_asset(path)
    want = {"max_texture_size": MAX}
    if role == "basecolor":
        want["srgb"] = True
    elif role == "normal":
        want["srgb"] = False
        want["compression_settings"] = unreal.TextureCompressionSettings.TC_NORMALMAP
    else:
        want["srgb"] = False
        want["compression_settings"] = unreal.TextureCompressionSettings.TC_MASKS
    dirty = False
    for k, v in want.items():
        cur = tex.get_editor_property(k)
        if k == "max_texture_size":
            if cur == 0 or cur > v:
                tex.set_editor_property(k, v)
                dirty = True
        elif cur != v:
            tex.set_editor_property(k, v)
            dirty = True
    if dirty:
        eal.save_loaded_asset(tex, False)
        changed.append(path)


def main():
    t0 = time.time()
    with open(CATALOG, encoding="utf-8") as f:
        catalog = [e for e in json.load(f)["surfaces"] if not ONLY or e["id"] in ONLY]
    report = {"catalog": CATALOG, "roots": ROOTS, "max": MAX, "surfaces": {}, "missing": [], "incomplete": {},
              "changed": [], "errors": []}
    if LOOSE:
        for e in catalog:
            try:
                import_loose(e)
            except Exception as ex:  # noqa: BLE001  keep going, report it
                report["errors"].append("%s import: %r" % (e["id"], ex))
    folders = textures_by_folder()
    for e in catalog:
        try:
            folder = find_folder(e, folders)
            if folder is None:
                report["missing"].append(e["id"])
                continue
            found = {}
            for p in sorted(folders[folder]):
                role, ch = role_of(p.rsplit("/", 1)[1])
                if role and role not in found:
                    found[role] = (p, ch)
            if "basecolor" not in found:
                report["incomplete"][e["id"]] = {"folder": folder, "found": sorted(found)}
                continue
            s = {"folder": folder, "tile_cm": e.get("tile_cm", 200), "role": e.get("role", ""),
                 "priority": e.get("priority", 3)}
            for k in ("tint", "gain"):
                if e.get(k):
                    s[k] = e[k]
            found.pop("ao", None)          # not used: the ORDp red channel or nothing
            for role, (p, ch) in found.items():
                prepare(p, role, report["changed"])
                s[role] = p
                if role == "rough":
                    s["rough_channel"] = ch
            report["surfaces"][e["id"]] = s
        except Exception as ex:  # noqa: BLE001
            report["errors"].append("%s: %r" % (e["id"], ex))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "surfaces.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    log("%d surfaces found, %d missing, %d incomplete, %d textures re-saved, %d errors"
        % (len(report["surfaces"]), len(report["missing"]), len(report["incomplete"]), len(report["changed"]),
           len(report["errors"])))


main()
