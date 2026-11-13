"""Create the IK retargeter that drives the MetaHuman player from the pawn's UEFN mannequin pose.

  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="<repo>/tools/character/create_pedestrian_retargeter.py" -unattended -nosplash -nullrhi

The retargeter shipped with the sample has no source IK rig (SourceIKRigAsset = None), so the body stays in its
reference pose. This one is saved under /Game/MetaHumans/GTP (not versioned), source = the UEFN mannequin rig
used by the pedestrians, target = the MetaHuman retarget rig, chains auto-mapped by name (fuzzy).
Lines "RTG ...": the chain mapping, to check the arms and legs are mapped. Use it with
-PlayerVisualRetargeter=/Game/MetaHumans/GTP/RTG_UEFNped_to_Metahuman.RTG_UEFNped_to_Metahuman
Everything stays under /Game.
"""
import unreal

SOURCE = "/Game/Characters/Pedestrians/Retargeted/IK_SKM_UEFN_Mannequin"
TARGET = "/Game/MetaHumans/Common/Common/Rigs/IK_Metahuman_Retarget"
FOLDER = "/Game/MetaHumans/GTP"
NAME = "RTG_UEFNped_to_Metahuman"

src = unreal.load_asset(SOURCE)
tgt = unreal.load_asset(TARGET)
unreal.log("RTG source %s target %s" % (src, tgt))
if not src or not tgt:
    raise SystemExit("RTG rigs missing")

path = FOLDER + "/" + NAME
if unreal.EditorAssetLibrary.does_asset_exist(path):
    asset = unreal.load_asset(path)
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(NAME, FOLDER, unreal.IKRetargeter, unreal.IKRetargetFactory())
ctrl = unreal.IKRetargeterController.get_controller(asset)
ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src)
ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, tgt)
ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
try:
    for chain in ctrl.get_all_chain_settings() if hasattr(ctrl, "get_all_chain_settings") else []:
        unreal.log("RTG chain %s" % chain)
except Exception as err:
    unreal.log("RTG chain list unavailable: %s" % err)
try:
    names = [str(n) for n in ctrl.get_retarget_chain_names(unreal.RetargetSourceOrTarget.TARGET)] if hasattr(ctrl, "get_retarget_chain_names") else []
    for n in names:
        unreal.log("RTG target chain %s <- source %s" % (n, ctrl.get_source_chain(n) if hasattr(ctrl, "get_source_chain") else "?"))
except Exception as err:
    unreal.log("RTG mapping unavailable: %s" % err)
unreal.EditorAssetLibrary.save_asset(path)
unreal.log("RTG saved %s" % path)
