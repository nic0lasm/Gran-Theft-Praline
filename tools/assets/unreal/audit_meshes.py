"""Measure what imported packs would cost before they reach the city map.

Runs inside the Unreal Editor (no UI): -ExecutePythonScript=<copy of this file>.
For every static mesh under the given content folders it records the LOD count,
triangles per LOD, Nanite, material slots and bounds; for every texture its size.
Nothing is changed or saved.

Environment:
  GTP_AUDIT_ROOTS  space-separated content folders (default: the Fab tree packs
                   and Roadside Construction)
  GTP_AUDIT_OUT    output JSON (default Saved/CityImport/mesh_audit.json); a CSV
                   with one row per mesh is written next to it

Textures above 2048 px are counted: the M1 profile streams at most 2K (ADR-0002),
so a larger source only costs disk and import time.
"""

import csv
import json
import os
import time

import unreal

DEFAULT_ROOTS = "/Game/EuropeanBeech /Game/BlackAlder /Game/Megaplant_Library /Game/Scene_RoadsideConstruction"
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = os.environ.get("GTP_AUDIT_OUT") or os.path.join(SAVED, "CityImport", "mesh_audit.json")
TEXTURE_CAP = 2048

eal = unreal.EditorAssetLibrary
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def safe(fn, default=None):
    try:
        return fn()
    except Exception:  # an odd asset must not stop the audit
        return default


def mesh_row(path, mesh):
    lods = safe(lambda: sm_sub.get_lod_count(mesh), 0) or 0
    tris = [safe(lambda i=i: mesh.get_num_triangles(i), -1) for i in range(lods)]
    nanite = safe(lambda: bool(mesh.get_editor_property("nanite_settings").enabled), False)
    ext = safe(lambda: mesh.get_bounds().box_extent)
    return {
        "path": path,
        "lods": lods,
        "triangles": tris,
        "lod0_triangles": tris[0] if tris else -1,
        "last_lod_triangles": tris[-1] if tris else -1,
        "nanite": nanite,
        "materials": safe(lambda: len(mesh.get_editor_property("static_materials")), -1),
        "size_cm": [round(2 * ext.x), round(2 * ext.y), round(2 * ext.z)] if ext else None,
    }


def main():
    t0 = time.time()
    roots = os.environ.get("GTP_AUDIT_ROOTS", DEFAULT_ROOTS).split()
    report = {"roots": {}, "meshes": [], "errors": []}
    for root in roots:
        if not eal.does_directory_exist(root):
            report["roots"][root] = "missing"
            continue
        meshes = textures = big = 0
        tex_bytes_px = 0
        for path in eal.list_assets(root, recursive=True, include_folder=False):
            cls = str(eal.find_asset_data(path).asset_class_path.asset_name)
            if cls not in ("StaticMesh", "Texture2D"):
                continue
            asset = eal.load_asset(path)
            if asset is None:
                report["errors"].append("cannot load " + path)
                continue
            if cls == "StaticMesh":
                meshes += 1
                report["meshes"].append(mesh_row(path.split(".")[0], asset))
            else:
                textures += 1
                w = safe(lambda: asset.blueprint_get_size_x(), 0)
                h = safe(lambda: asset.blueprint_get_size_y(), 0)
                tex_bytes_px += w * h
                if max(w, h) > TEXTURE_CAP:
                    big += 1
        report["roots"][root] = {"meshes": meshes, "textures": textures,
                                 "textures_over_2k": big, "texture_megapixels": round(tex_bytes_px / 1e6, 1)}
    report["seconds"] = round(time.time() - t0, 1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(report, f, indent=2)
    with open(os.path.splitext(OUT)[0] + ".csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["path", "lods", "lod0_triangles", "last_lod_triangles", "nanite", "materials",
                    "size_x_cm", "size_y_cm", "size_z_cm"])
        for m in sorted(report["meshes"], key=lambda m: m["lod0_triangles"]):
            s = m["size_cm"] or ["", "", ""]
            w.writerow([m["path"], m["lods"], m["lod0_triangles"], m["last_lod_triangles"], m["nanite"],
                        m["materials"]] + list(s))
    unreal.log("audit_meshes: %d meshes, %s" % (len(report["meshes"]), json.dumps(report["roots"])))


main()
