"""Lists every skeletal mesh under a content folder with what the pedestrian pool needs to know:
skeleton asset, whether it is the Manny/Quinn skeleton, triangles per LOD, material slots, height.

Run in the editor (any machine with the folder in the project):
  UnrealEditor <project> -run=pythonscript -script=tools/pedestrians/inspect_character_meshes.py
Folder: GTP_INSPECT_FOLDER (default /Game/Man/Mesh). Output: lines starting with "INSPECT" in the log.
"""
import os

import unreal

folder = os.environ.get("GTP_INSPECT_FOLDER", "/Game/Man/Mesh")
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for data in registry.get_assets_by_path(folder, recursive=True):
    if str(data.asset_class_path.asset_name) != "SkeletalMesh":
        continue
    mesh = unreal.load_asset(str(data.package_name))
    skeleton = mesh.get_editor_property("skeleton")
    skeleton_path = skeleton.get_path_name() if skeleton else "none"
    lods = []
    try:
        for lod in range(mesh.get_num_lods()):
            lods.append(str(unreal.SkeletalMeshEditorSubsystem().get_num_triangles(mesh, lod)))
    except Exception:  # API differs between versions: fall back to the LOD count
        lods = ["%d LODs" % mesh.get_num_lods()]
    bounds = mesh.get_bounds()
    height = bounds.box_extent.z * 2
    manny = any(k in skeleton_path.lower() for k in ("sk_mannequin", "manny", "quinn"))
    unreal.log("INSPECT %s | skeleton %s | manny_like %s | tris/LOD %s | materials %d | height_cm %.0f"
               % (data.package_name, skeleton_path, manny, "/".join(lods), len(mesh.materials), height))
