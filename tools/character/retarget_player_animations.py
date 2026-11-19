"""Bake walk / idle / run animations onto the MetaHuman skeleton (offline retarget, no runtime IK needed).

  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="<repo>/tools/character/retarget_player_animations.py" -unattended -nosplash -nullrhi

Uses the retargeter made by create_pedestrian_retargeter.py and fixed by fix_player_retargeter.py (run them
first; clips baked with the unfixed one have the body's pelvis on the ground). The duplicated animations land
in /Game/MetaHumans/GTP/Anims with the prefix "MH_" (the game finds them by that name). Everything stays under /Game (not versioned).
Env: GTP_RTG_ANIMS = comma separated list of source animation paths (default: the pedestrians' walk/idle/run and
their hands-up loop; GTP_RTG_ANIMS=/Game/Characters/Pedestrians/Anims/A_HandsUp bakes that one alone, as MH_A_HandsUp).
For the crowd: the bodies of the City Sample pack have their own skeleton asset (same name as the MetaHuman one, another
asset). GTP_RTG_TARGET_MESH=/Game/Crowd/Character/Male/NormalWeight/Meshes/m_tal_nrw_body with GTP_RTG_PREFIX=CS_ and
GTP_RTG_FOLDER=/Game/MetaHumans/GTP/AnimsCrowd bakes the same loops on that skeleton; the crowd takes CS_<loop> when it
is there (CrowdSkeletonAnimationFolder). The MH_ clips are left alone by such a run.
"""
import os

import unreal

RTG = "/Game/MetaHumans/GTP/RTG_UEFNped_to_Metahuman_Fixed"
DEFAULTS = [
    "/Game/Characters/UEFN_Mannequin/Animations/Walk/M_Neutral_Walk_Loop_F_L_20",
    "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Relaxed_Stand_Idle_Loop",
    "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F_R_20",
    # The pedestrians' hands-up loop (tools/pedestrians/make_handsup_animation.py makes it); skipped when absent.
    "/Game/Characters/Pedestrians/Anims/A_HandsUp",
]
paths = [p for p in os.environ.get("GTP_RTG_ANIMS", "").split(",") if p] or DEFAULTS

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
rtg = unreal.load_asset(RTG)
source_mesh = None
target_mesh = None
for d in registry.get_assets_by_path("/Game/MetaHumans", recursive=True):
    if str(d.asset_class_path.asset_name) == "SkeletalMesh" and str(d.asset_name).lower().endswith("_body") or str(d.asset_name).lower() == "m_med_nrw_body":
        target_mesh = target_mesh or d.get_asset()
source_mesh = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")
if os.environ.get("GTP_RTG_TARGET_MESH"):
    target_mesh = unreal.load_asset(os.environ["GTP_RTG_TARGET_MESH"])
PREFIX = os.environ.get("GTP_RTG_PREFIX", "MH_")
unreal.log("ANIMS rtg %s source mesh %s target mesh %s (skeleton %s) prefix %s" % (
    rtg, source_mesh, target_mesh, target_mesh.get_editor_property("skeleton").get_path_name() if target_mesh else None, PREFIX))
if not (rtg and source_mesh and target_mesh):
    raise SystemExit("ANIMS missing inputs")

assets = []
for p in paths:
    a = unreal.load_asset(p)
    unreal.log("ANIMS source %s -> %s" % (p, a))
    if a:
        assets.append(unreal.EditorAssetLibrary.find_asset_data(p))
op = unreal.IKRetargetBatchOperation
unreal.log("ANIMS signature: %s" % op.duplicate_and_retarget.__doc__)
folder = os.environ.get("GTP_RTG_FOLDER", "/Game/MetaHumans/GTP/Anims")
unreal.EditorAssetLibrary.make_directory(folder)
batch = None
attempts = (
    dict(assets_to_retarget=assets, source_mesh=source_mesh, target_mesh=target_mesh, ik_retarget_asset=rtg,
         target_path=folder, search="", replace="", prefix=PREFIX, suffix="", overwrite_existing_files=True),
    dict(assets_to_retarget=assets, source_mesh=source_mesh, target_mesh=target_mesh, ik_retarget_asset=rtg,
         target_path=folder, prefix=PREFIX),
)
for kwargs in attempts:
    try:
        batch = op.duplicate_and_retarget(**kwargs)
        unreal.log("ANIMS call worked with %s" % sorted(kwargs))
        break
    except Exception as err:
        unreal.log("ANIMS call failed (%s): %s" % (sorted(kwargs), err))
if batch is None:
    raise SystemExit("ANIMS no call variant worked")
for a in batch:
    unreal.log("ANIMS made %s" % a)
    # The pawn moves the body: the animations must not carry their own displacement (root motion).
    asset = a.get_asset() if hasattr(a, "get_asset") else a
    for prop, value in (("force_root_lock", True), ("enable_root_motion", False)):
        try:
            asset.set_editor_property(prop, value)
            unreal.log("ANIMS %s %s = %s" % (asset.get_name(), prop, value))
        except Exception as err:
            unreal.log("ANIMS %s could not set %s: %s" % (asset.get_name(), prop, err))
    try:
        unreal.EditorAssetLibrary.save_loaded_asset(a.get_asset() if hasattr(a, "get_asset") else a)
    except Exception as err:
        unreal.log("ANIMS save failed: %s" % err)
def pin_pelvis(asset, bone="pelvis"):
    """The retarget moved the walk's travel onto the pelvis: keep its height, freeze its horizontal place."""
    ctrl = getattr(asset, "controller", None)
    try:
        frames = int(unreal.AnimationLibrary.get_num_frames(asset))
        transforms = [unreal.AnimationLibrary.get_bone_pose_for_frame(asset, bone, i, False) for i in range(frames)]
        first = transforms[0].translation
        drift = max(abs(t.translation.x - first.x) + abs(t.translation.y - first.y) for t in transforms)
        unreal.log("ANIMS %s pelvis drift before %.1f cm over %d frames" % (asset.get_name(), drift, frames))
        pos = [unreal.Vector(first.x, first.y, t.translation.z) for t in transforms]
        quat = [t.rotation for t in transforms]
        scale = [t.scale3d for t in transforms]
        ctrl.open_bracket("PinPelvis")
        ctrl.set_bone_track_keys(bone, pos, quat, scale)
        ctrl.close_bracket()
        unreal.log("ANIMS %s pelvis pinned" % asset.get_name())
    except Exception as err:
        unreal.log("ANIMS %s pelvis pin failed: %s; library methods: %s" % (
            asset.get_name(), err, ", ".join(sorted(m for m in dir(unreal.AnimationLibrary) if "bone" in m or "pose" in m))))


# Animations made by an earlier run (same names or renamed copies): lock their root too.
for d in registry.get_assets_by_path(folder, recursive=False):
    existing = d.get_asset()
    if isinstance(existing, unreal.AnimSequence) and str(d.asset_name).startswith(PREFIX):
        for prop, value in (("force_root_lock", True), ("enable_root_motion", False)):
            try:
                existing.set_editor_property(prop, value)
            except Exception as err:
                unreal.log("ANIMS %s could not set %s: %s" % (d.asset_name, prop, err))
        if "Walk" in str(d.asset_name) or "Run" in str(d.asset_name):
            pin_pelvis(existing, os.environ.get("GTP_PELVIS", "pelvis"))
        unreal.EditorAssetLibrary.save_loaded_asset(existing)
        unreal.log("ANIMS root locked %s" % d.asset_name)
unreal.log("ANIMS done %d" % len(batch))
