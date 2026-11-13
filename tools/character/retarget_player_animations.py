"""Bake walk / idle / run animations onto the MetaHuman skeleton (offline retarget, no runtime IK needed).

  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="<repo>/tools/character/retarget_player_animations.py" -unattended -nosplash -nullrhi

Uses the retargeter made by create_pedestrian_retargeter.py (run it first). The duplicated animations land
next to their sources with the prefix "MH_" (the game finds them by that name). Everything stays under /Game (not versioned).
Env: GTP_RTG_ANIMS = comma separated list of source animation paths (default: the pedestrians' walk/idle/run).
"""
import os

import unreal

RTG = "/Game/MetaHumans/GTP/RTG_UEFNped_to_Metahuman"
DEFAULTS = [
    "/Game/Characters/UEFN_Mannequin/Animations/Walk/M_Neutral_Walk_Loop_F_L_20",
    "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Relaxed_Stand_Idle_Loop",
    "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F_R_20",
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
unreal.log("ANIMS rtg %s source mesh %s target mesh %s" % (rtg, source_mesh, target_mesh))
if not (rtg and source_mesh and target_mesh):
    raise SystemExit("ANIMS missing inputs")

assets = []
for p in paths:
    a = unreal.load_asset(p)
    unreal.log("ANIMS source %s -> %s" % (p, a))
    if a:
        assets.append(a)
batch = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
    assets, source_mesh, target_mesh, rtg, "", "", "MH_", "", False)
for a in batch:
    unreal.log("ANIMS made %s" % a.get_path_name())
    unreal.EditorAssetLibrary.save_loaded_asset(a)
unreal.log("ANIMS done %d" % len(batch))
