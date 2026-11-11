"""Street-tree meshes the M1 can draw: no Nanite, a decreasing LOD chain, impostor last.

Runs inside the Unreal Editor (no UI), through Content/Python/gtp_run.py. The Megascans
beech and alder meshes are all Nanite. A GPU without Nanite draws their fallback as LOD 0,
then the pack's authored LOD 1.. which are 4 to 100 times heavier (audit of 2026-11-11,
docs/evidence/trees-audit on feat/trees-audit-results): the chain is upside down. Past
LOD 1 the authored chain does decrease, down to an 8-triangle billboard.

For each source mesh this script makes a copy under /Game/<Pack>/M1/ (the pack folders are
out of Git, the source is never changed), turns Nanite off and rebuilds its LODs from the
source's own authored LODs: LOD 0 is the first authored LOD at or under GTP_TREES_M1_MAX
triangles, then the following ones, the billboard last. No simplifier runs on the leaves,
so the cards look as the pack drew them. Replaying the script rebuilds the copies.

Environment:
  GTP_TREES_M1_SOURCES  space-separated mesh paths (default DEFAULT_SOURCES: the 8.9 m alder
                        and the 3.7 m alder sapling; SM_BlackAlder_Field_05, fuller, needs MAX 7300)
  GTP_TREES_M1_MAX      most triangles accepted for LOD 0 (default 6000)

Report: Saved/CityImport/trees_m1_report.json (source and rebuilt triangles per LOD).
"""

import json
import os
import time

import unreal

DEFAULT_SOURCES = (
    "/Game/BlackAlder/Geometry/SimpleWind/SM_BlackAlder_Forest_06",   # 8.9 m: 5355, 1803, 913, 8
    "/Game/BlackAlder/Geometry/SimpleWind/SM_BlackAlder_Sapling_06",  # 3.7 m: 2577, 1251
)
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = os.path.join(SAVED, "CityImport", "trees_m1_report.json")

eal = unreal.EditorAssetLibrary
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def tris(mesh):
    return [mesh.get_num_triangles(i) for i in range(sm_sub.get_lod_count(mesh))]


def target_path(source):
    """/Game/<Pack>/Geometry/SimpleWind/SM_X -> /Game/<Pack>/M1/SM_X_M1"""
    return "/Game/%s/M1/%s_M1" % (source.split("/")[2], source.rsplit("/", 1)[1])


def chain(source_tris, most):
    """Authored LOD indices to keep: first one (>= 1) at or under `most`, then the rest."""
    for i in range(1, len(source_tris)):
        if 0 < source_tris[i] <= most:
            return list(range(i, len(source_tris)))
    return [len(source_tris) - 1]


def auto_screen_size(mesh):
    """Turns on the automatic LOD screen sizes when this engine exposes the flag.

    UE 5.8 does not expose it to Python on UStaticMesh (the property lookup
    fails), so the mesh keeps the flag of the duplicated source; the report
    says which case applied instead of failing the mesh.
    """
    for name in ("auto_compute_lod_screen_size", "b_auto_compute_lod_screen_size"):
        try:
            mesh.set_editor_property(name, True)
            return "set"
        except Exception:
            continue
    return "not exposed, kept from the source"


def prepare(source, most, row):
    src = eal.load_asset(source)
    if src is None:
        raise RuntimeError("cannot load " + source)
    row["source_triangles"] = tris(src)
    keep = chain(row["source_triangles"], most)
    row["source_lods_kept"] = keep

    dest = target_path(source)
    if eal.does_asset_exist(dest):
        eal.delete_asset(dest)
    mesh = eal.duplicate_asset(source, dest)
    if mesh is None:
        raise RuntimeError("cannot duplicate to " + dest)
    settings = mesh.get_editor_property("nanite_settings")
    settings.enabled = False
    mesh.set_editor_property("nanite_settings", settings)

    sm_sub.remove_lods(mesh)                      # LOD 0 only
    for dest_lod, src_lod in enumerate(keep):     # LOD 0 replaced, then appended
        got = sm_sub.set_lod_from_static_mesh(mesh, dest_lod, src, src_lod, True)
        if got < 0:
            raise RuntimeError("set_lod_from_static_mesh %d <- %d failed" % (dest_lod, src_lod))
    row["auto_lod_screen_size"] = auto_screen_size(mesh)
    eal.save_loaded_asset(mesh)

    row.update({"mesh": dest, "triangles": tris(mesh),
                "nanite": bool(mesh.get_editor_property("nanite_settings").enabled)})
    t = row["triangles"]
    row["decreasing"] = all(a >= b for a, b in zip(t, t[1:]))
    if not row["decreasing"] or row["nanite"]:
        raise RuntimeError("%s: chain %s, Nanite %s" % (dest, t, row["nanite"]))


def main():
    t0 = time.time()
    sources = os.environ.get("GTP_TREES_M1_SOURCES", " ".join(DEFAULT_SOURCES)).split()
    most = int(os.environ.get("GTP_TREES_M1_MAX", "6000"))
    report = {"max_lod0_triangles": most, "meshes": [], "errors": []}
    for source in sources:
        row = {"source": source}
        try:
            prepare(source, most, row)
        except Exception as e:
            report["errors"].append("%s: %s" % (source, e))
        report["meshes"].append(row)
    report["seconds"] = round(time.time() - t0, 1)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(report, f, indent=2)
    unreal.log("prepare_trees_m1: %d meshes, %d errors" % (len(report["meshes"]), len(report["errors"])))


main()
