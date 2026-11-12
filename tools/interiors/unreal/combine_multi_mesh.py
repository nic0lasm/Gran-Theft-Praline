"""Re-import the multi-mesh free models as one combined static mesh (step 3, before map_interior_meshes.py).

Run in a headless editor (-nullrhi -ExecutePythonScript through Content/Python/gtp_run.py).
A glTF or FBX with several nodes is imported as one static mesh per node, each at its own
origin: the mesh map can then only place one piece (a fridge door, a table top). For every folder
/Game/Interiors/Free/Models/<name> holding more than one static mesh, the source file recorded in
the import data is imported again through Interchange with "combine static meshes" on, into
/Game/Interiors/Free/Models/<name>_Combined. The pieces stay where they are; the combined mesh,
being the whole object, wins the size test of map_interior_meshes.py. Folders already combined are
skipped, so the script can run again. GTP_COMBINE_ONLY limits it to some folder names (comma
separated). Writes Saved/InteriorImport/combine_report.json. Nothing downloaded is ever run.
"""

import json
import os
import time

import unreal

ROOT = "/Game/Interiors/Free/Models"
PIPELINE_SOURCE = "/Interchange/Pipelines/DefaultAssetsPipeline"
PIPELINE = "/Game/Interiors/Free/Pipelines/GTP_CombineMeshesPipeline"
ONLY = [n.strip() for n in os.environ.get("GTP_COMBINE_ONLY", "").split(",") if n.strip()]
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
eal = unreal.EditorAssetLibrary


def log(text):
    unreal.log_warning("[InteriorCombine] " + text)


def static_meshes(folder):
    out = []
    for path in eal.list_assets(folder, recursive=False, include_folder=False):
        data = eal.find_asset_data(path)
        if str(data.asset_class_path.asset_name) == "StaticMesh":
            out.append(path.split(".")[0])
    return out


def source_file(mesh_path):
    mesh = eal.load_asset(mesh_path)
    data = mesh.get_editor_property("asset_import_data") if mesh else None
    if data is None:
        return ""
    try:
        return data.get_first_filename()
    except Exception:
        files = data.extract_filenames()
        return files[0] if files else ""


def combine_pipeline(report):
    """A copy of the default Interchange assets pipeline with combine static meshes on."""
    if not eal.does_asset_exist(PIPELINE):
        if not eal.does_asset_exist(PIPELINE_SOURCE):
            report["errors"].append("no Interchange default pipeline at " + PIPELINE_SOURCE)
            return None
        if eal.duplicate_asset(PIPELINE_SOURCE, PIPELINE) is None:
            report["errors"].append("cannot copy " + PIPELINE_SOURCE)
            return None
    pipeline = eal.load_asset(PIPELINE)
    try:
        mesh_pipeline = pipeline.get_editor_property("mesh_pipeline")
        # UE 5.8: bCombineStaticMeshes is deprecated for CombineStaticMeshesBehavior, an enum whose
        # name is not stable across versions: its type is read from the current value and the
        # member that combines everything is picked by name.
        current = mesh_pipeline.get_editor_property("combine_static_meshes_behavior")
        enum = type(current)
        members = [m for m in dir(enum) if m.isupper() and isinstance(getattr(enum, m), enum)]
        report["combine_enum"] = {"type": enum.__name__, "members": members, "was": str(current)}
        wanted = [m for m in members if "ALL" in m] or [m for m in members if "COMBINE" in m and "NO" not in m and "DONT" not in m]
        if not wanted:
            report["errors"].append("no 'combine all' member in %s: %s" % (enum.__name__, members))
            return None
        mesh_pipeline.set_editor_property("combine_static_meshes_behavior", getattr(enum, wanted[0]))
        report["combine_enum"]["set"] = wanted[0]
        pipeline.set_editor_property("mesh_pipeline", mesh_pipeline)
    except Exception as error:
        report["errors"].append("cannot set combine_static_meshes_behavior on the pipeline: %s" % error)
        return None
    eal.save_loaded_asset(pipeline)
    return pipeline


def import_combined(filename, dest, report):
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    source = unreal.InterchangeManager.create_source_data(filename)
    params = unreal.ImportAssetParameters()
    params.is_automated = True
    params.override_pipelines.append(unreal.SoftObjectPath(PIPELINE))
    try:
        result = manager.import_asset(dest, source, params)
    except Exception as error:
        report["errors"].append("import of %s failed: %s" % (filename, error))
        return 0
    if result is False:
        report["errors"].append("import of %s refused" % filename)
    eal.save_directory(dest, only_if_is_dirty=False, recursive=True)
    return len(static_meshes(dest))


def main():
    t0 = time.time()
    report = {"combined": [], "already": [], "single": 0, "no_source": [], "errors": []}
    pipeline = combine_pipeline(report)
    folders = [f.rstrip("/") for f in eal.list_assets(ROOT, recursive=False, include_folder=True) if f.endswith("/")]
    for folder in sorted(folders):
        name = folder.rsplit("/", 1)[1]
        if pipeline is None or name.endswith("_Combined") or (ONLY and name not in ONLY):
            continue
        pieces = static_meshes(folder)
        if len(pieces) < 2:
            report["single"] += 1
            continue
        dest = folder + "_Combined"
        if eal.does_directory_exist(dest) and static_meshes(dest):
            report["already"].append(name)
            continue
        filename = source_file(pieces[0])
        if not filename or not os.path.isfile(filename):
            report["no_source"].append({"name": name, "file": filename})
            continue
        count = import_combined(filename, dest, report)
        report["combined"].append({"name": name, "pieces": len(pieces), "file": filename, "meshes_after": count})
        if count != 1:
            report["errors"].append("%s: %d static meshes after the combined import (1 expected)" % (name, count))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(SAVED, "InteriorImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "combine_report.json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1)
    log("%d combined, %d already, %d single, %d without source, %d errors" % (
        len(report["combined"]), len(report["already"]), report["single"], len(report["no_source"]), len(report["errors"])))


main()
