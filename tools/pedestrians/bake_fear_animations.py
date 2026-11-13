"""Bake the MoCap Online fear animations (UE4 mannequin skeleton) onto the MetaHuman skeleton (editor script).

  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="<repo>/tools/pedestrians/bake_fear_animations.py" -unattended -nosplash -nullrhi

Needs /Game/MCO_Mocap_Basics (MoCap Online Free) migrated into the game project, and the IK Rig of the UE4 mannequin
(found by name: an IKRigDefinition containing "UE4" and "Mannequin"; GTP_SOURCE_RIG overrides). Makes
/Game/MetaHumans/GTP/RTG_UE4_to_Metahuman (chains mapped by name) and bakes the animations whose name contains one of
GTP_FEAR_WORDS (default scared,fear,cower,crouch,panic,run,fall,death,dying,flinch,stumble) to
/Game/MetaHumans/GTP/Anims with the prefix "MF_", root locked, pelvis travel frozen for the locomotion ones.
Lines "FEAR ...". Everything stays under /Game, not versioned.
"""
import os

import unreal

TARGET_RIG = "/Game/MetaHumans/Common/Common/Rigs/IK_Metahuman_Retarget"
FOLDER = "/Game/MetaHumans/GTP"
ANIMS = FOLDER + "/Anims"
NAME = "RTG_UE4_to_Metahuman"
words = [w for w in (os.environ.get("GTP_FEAR_WORDS") or "scared,fear,cower,crouch,panic,run,fall,death,dying,flinch,stumble").lower().split(",") if w]

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()

source_rig = None
override = os.environ.get("GTP_SOURCE_RIG")
for d in registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/IKRig", "IKRigDefinition"), True):
    path = str(d.object_path) if hasattr(d, "object_path") else str(d.package_name)
    unreal.log("FEAR ik rig candidate %s" % path)
    n = str(d.asset_name)
    if (override and override in path) or (not override and "UE4" in n and "Mannequin" in n and "Metahuman" not in n):
        source_rig = source_rig or d.get_asset()
target_rig = unreal.load_asset(TARGET_RIG)
unreal.log("FEAR source rig %s target rig %s" % (source_rig, target_rig))
if not (source_rig and target_rig):
    raise SystemExit("FEAR rigs missing")

unreal.EditorAssetLibrary.make_directory(FOLDER)
unreal.EditorAssetLibrary.make_directory(ANIMS)
path = FOLDER + "/" + NAME
if unreal.EditorAssetLibrary.does_asset_exist(path):
    rtg = unreal.load_asset(path)
else:
    rtg = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.IKRetargeter, unreal.IKRetargetFactory())
ctrl = unreal.IKRetargeterController.get_controller(rtg)
ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
for fn in ("add_default_ops",):
    if hasattr(ctrl, fn):
        getattr(ctrl, fn)()
ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)


def chain_names(rig):
    rc = unreal.IKRigController.get_controller(rig)
    return [str(c.chain_name) if hasattr(c, "chain_name") else str(c) for c in rc.get_retarget_chains()]


source_by_lower = {n.lower(): n for n in chain_names(source_rig)}
for name in chain_names(target_rig):
    wanted = source_by_lower.get(name.lower())
    if wanted:
        try:
            ctrl.set_source_chain(wanted, name)
        except Exception as err:
            unreal.log("FEAR set_source_chain %s failed: %s" % (name, err))
    unreal.log("FEAR chain %s <- %s" % (name, ctrl.get_source_chain(name)))
unreal.EditorAssetLibrary.save_asset(path)

source_mesh = None
target_mesh = None
for d in registry.get_assets_by_path("/Game", recursive=True):
    if str(d.asset_class_path.asset_name) != "SkeletalMesh":
        continue
    n = str(d.asset_name).lower()
    if n == "m_med_nrw_body":
        target_mesh = d.get_asset()
    if "mannequin" in n and "metahuman" not in n and source_mesh is None and "mco" in str(d.package_name).lower():
        source_mesh = d.get_asset()
unreal.log("FEAR source mesh %s target mesh %s" % (source_mesh, target_mesh))
if not (source_mesh and target_mesh):
    raise SystemExit("FEAR meshes missing")

selected = []
for d in registry.get_assets_by_path("/Game/MCO_Mocap_Basics", recursive=True):
    if str(d.asset_class_path.asset_name) == "AnimSequence" and any(w in str(d.asset_name).lower() for w in words):
        selected.append(d)
        unreal.log("FEAR selected %s" % d.asset_name)
if not selected:
    raise SystemExit("FEAR no animation matches %s" % words)
batch = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
    assets_to_retarget=selected, source_mesh=source_mesh, target_mesh=target_mesh, ik_retarget_asset=rtg,
    prefix="MF_", target_path=ANIMS, overwrite_existing_files=True)
for a in batch:
    asset = a.get_asset() if hasattr(a, "get_asset") else a
    for prop, value in (("force_root_lock", True), ("enable_root_motion", False)):
        try:
            asset.set_editor_property(prop, value)
        except Exception as err:
            unreal.log("FEAR %s could not set %s: %s" % (asset.get_name(), prop, err))
    name = asset.get_name().lower()
    if "run" in name or "walk" in name or "stumble" in name:
        try:
            frames = int(unreal.AnimationLibrary.get_num_frames(asset))
            poses = [unreal.AnimationLibrary.get_bone_pose_for_frame(asset, "pelvis", i, False) for i in range(frames)]
            first = poses[0].translation
            asset.controller.open_bracket("PinPelvis")
            asset.controller.set_bone_track_keys("pelvis", [unreal.Vector(first.x, first.y, p.translation.z) for p in poses],
                                                 [p.rotation for p in poses], [p.scale3d for p in poses])
            asset.controller.close_bracket()
            unreal.log("FEAR %s pelvis pinned" % asset.get_name())
        except Exception as err:
            unreal.log("FEAR %s pelvis pin failed: %s" % (asset.get_name(), err))
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log("FEAR made %s" % asset.get_name())
unreal.log("FEAR done %d" % len(batch))
