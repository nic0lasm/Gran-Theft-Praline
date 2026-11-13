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
for mode in ("EXACT", "FUZZY"):
    try:
        ctrl.auto_map_chains(getattr(unreal.AutoMapChainType, mode), True)
        unreal.log("RTG auto_map_chains %s done" % mode)
        break
    except Exception as err:
        unreal.log("RTG auto_map_chains %s failed: %s" % (mode, err))
unreal.log("RTG controller methods: %s" % ", ".join(sorted(m for m in dir(ctrl) if not m.startswith("_") and ("chain" in m or "map" in m or "source" in m))))


def chain_names(rig):
    rc = unreal.IKRigController.get_controller(rig)
    for fn in ("get_retarget_chains", "get_retarget_chain_names"):
        if hasattr(rc, fn):
            out = []
            for c in getattr(rc, fn)():
                out.append(str(c.chain_name) if hasattr(c, "chain_name") else str(c))
            return out
    unreal.log("RTG rig controller methods: %s" % ", ".join(sorted(m for m in dir(rc) if "chain" in m)))
    return []


for name in chain_names(tgt):
    mapped = "?"
    for fn in ("get_source_chain", "get_source_chain_for_target"):
        if hasattr(ctrl, fn):
            try:
                mapped = str(getattr(ctrl, fn)(name))
            except Exception as err:
                mapped = "error %s" % err
            break
    unreal.log("RTG chain target %s <- source %s" % (name, mapped))
unreal.log("RTG source chains: %s" % ", ".join(chain_names(src)))
unreal.EditorAssetLibrary.save_asset(path)
unreal.log("RTG saved %s" % path)
