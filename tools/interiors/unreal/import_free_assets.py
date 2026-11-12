"""Import the free (CC0) downloads into the project before map_interior_meshes.py (step 3).

Run in the Unreal Editor (pythonscript, headless). GTP_FREE_ASSETS names the download folder
(for example GTAL-assets/downloads-free). Every .gltf, .glb or .fbx found there, and every one found
inside a .zip, is imported as a static mesh with its textures under
/Game/Interiors/Free/Models/<name>. Texture-only zips (ambientCG material sets) are imported under
/Game/Interiors/Free/Surfaces/<name> (colour, normal GL, roughness, metalness, AO, displacement).
Zips are unpacked with Python's zipfile into Saved/InteriorImport/extract: nothing downloaded is
ever run. Already imported folders are skipped, so the script can be run again after new downloads.
Writes Saved/InteriorImport/import_report.json. The imported assets stay out of Git.

Then: GTP_INTERIOR_ROOTS=/Game/Interiors/Free map_interior_meshes.py.
"""

import json
import os
import re
import time
import zipfile

import unreal

SOURCE = os.environ.get("GTP_FREE_ASSETS", "")
DEST = "/Game/Interiors/Free"
MODELS = (".gltf", ".glb", ".fbx", ".obj")
TEXTURES = (".png", ".jpg", ".jpeg", ".exr", ".tga")
MAPS = ("color", "basecolor", "albedo", "diff", "normalgl", "nor_gl", "roughness", "rough", "metalness", "metal",
        "ambientocclusion", "_ao", "displacement", "disp", "opacity")
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
EXTRACT = os.path.join(SAVED, "InteriorImport", "extract")
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(text):
    unreal.log("InteriorImport: " + text)


def clean(name):
    name = re.sub(r"_(1k|2k|4k|8k)(_[a-z]+)?$", "", name, flags=re.I)
    return re.sub(r"[^A-Za-z0-9_]", "_", name).strip("_") or "asset"


def safe_extract(archive, target):
    """Unpack a zip, refusing members that would land outside target."""
    root = os.path.realpath(target)
    with zipfile.ZipFile(archive) as z:
        for member in z.namelist():
            dest = os.path.realpath(os.path.join(target, member))
            if not dest.startswith(root + os.sep) and dest != root:
                raise ValueError("unsafe path in " + archive + ": " + member)
        z.extractall(target)


def import_files(files, dest):
    tasks = []
    for path in files:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("save", True)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    return sum(len(t.get_editor_property("imported_object_paths") or []) for t in tasks)


def walk(folder):
    for base, _dirs, names in os.walk(folder):
        for name in names:
            yield os.path.join(base, name)


def handle(name, files, report):
    models = [f for f in files if f.lower().endswith(MODELS)]
    if models:
        # one model per entry: prefer glTF over FBX, and the 2k file when several resolutions exist
        models.sort(key=lambda f: (not f.lower().endswith((".gltf", ".glb")), "_2k" not in f.lower(), len(f)))
        dest = DEST + "/Models/" + name
        kind, chosen = "model", models[:1]
    else:
        chosen = [f for f in files if f.lower().endswith(TEXTURES) and any(m in os.path.basename(f).lower() for m in MAPS)]
        dest = DEST + "/Surfaces/" + name
        kind = "surface"
    if not chosen:
        report["skipped"].append({"name": name, "why": "no model and no texture map"})
        return
    if eal.does_directory_exist(dest) and eal.list_assets(dest, recursive=True):
        report["already"].append(name)
        return
    count = import_files(chosen, dest)
    report[kind + "s"].append({"name": name, "dest": dest, "files": len(chosen), "assets": count})
    if count == 0:
        report["errors"].append("nothing imported from " + name)


def main():
    t0 = time.time()
    report = {"source": SOURCE, "models": [], "surfaces": [], "already": [], "skipped": [], "errors": []}
    if not SOURCE or not os.path.isdir(SOURCE):
        report["errors"].append("GTP_FREE_ASSETS is not a folder: " + SOURCE)
    else:
        loose = {}
        for path in walk(SOURCE):
            low = path.lower()
            if low.endswith(".zip"):
                name = clean(os.path.splitext(os.path.basename(path))[0])
                target = os.path.join(EXTRACT, name)
                try:
                    if not os.path.isdir(target):
                        safe_extract(path, target)
                    handle(name, list(walk(target)), report)
                except (zipfile.BadZipFile, ValueError) as error:
                    report["errors"].append(str(error))
            elif low.endswith(MODELS):
                # a loose model and the textures next to it form one entry
                loose.setdefault(os.path.dirname(path), []).append(path)
        for folder, models in sorted(loose.items()):
            for model in models:
                name = clean(os.path.splitext(os.path.basename(model))[0])
                handle(name, [model], report)
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(SAVED, "InteriorImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "import_report.json"), "w", encoding="utf-8") as handle_out:
        json.dump(report, handle_out, indent=1)
    log("%d models, %d surfaces, %d already there, %d skipped, %d errors" % (
        len(report["models"]), len(report["surfaces"]), len(report["already"]), len(report["skipped"]),
        len(report["errors"])))


main()
