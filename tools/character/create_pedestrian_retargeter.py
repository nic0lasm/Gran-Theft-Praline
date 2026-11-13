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
import os

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
# Since UE 5.6 the chain mapping lives in the ops: make sure the default ops exist first.
for fn in ("add_default_ops", "add_default_op", "reset_ops_to_default", "add_default_retarget_ops"):
    if hasattr(ctrl, fn):
        try:
            getattr(ctrl, fn)()
            unreal.log("RTG %s done" % fn)
        except Exception as err:
            unreal.log("RTG %s failed: %s" % (fn, err))
unreal.log("RTG controller op methods: %s" % ", ".join(sorted(m for m in dir(ctrl) if not m.startswith("_") and "op" in m)))
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


# Explicit mapping, by name (case-insensitive); root has no source, "head" takes "Head".
source_by_lower = {n.lower(): n for n in chain_names(src)}
for name in chain_names(tgt):
    wanted = source_by_lower.get(name.lower())
    if not wanted:
        continue
    done = False
    for args in ((wanted, name), (unreal.Name(wanted), unreal.Name(name)), (name, wanted)):
        try:
            ctrl.set_source_chain(*args)
            done = True
            break
        except Exception as err:
            last = err
    if not done:
        unreal.log("RTG set_source_chain failed for %s: %s" % (name, last))
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
# Retarget poses: select "Default Pose" on both sides, make the target stance match the source one, list the ops.
for side in (unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET):
    try:
        names = [str(n) for n in ctrl.get_retarget_pose_names(side)]
        unreal.log("RTG %s retarget poses: %s, current %s" % (side, names, ctrl.get_current_retarget_pose_name(side)))
        if names:
            ctrl.set_current_retarget_pose(unreal.Name(names[0]), side)
    except Exception as err:
        unreal.log("RTG retarget pose API failed (%s): %s" % (side, err))
if os.environ.get("GTP_RTG_ALIGN", "1") == "1":
    for side in (unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET):
        try:
            ctrl.auto_align_all_bones(side)
            unreal.log("RTG auto_align_all_bones %s done" % side)
        except Exception as err:
            unreal.log("RTG auto_align_all_bones failed (%s): %s" % (side, err))
try:
    count = ctrl.get_num_retarget_ops() if hasattr(ctrl, "get_num_retarget_ops") else -1
    unreal.log("RTG ops: %s" % count)
    for i in range(max(count, 0)):
        op = ctrl.get_retarget_op_at_index(i)
        unreal.log("RTG op %d: %s enabled=%s" % (i, op, ctrl.get_op_enabled(i) if hasattr(ctrl, "get_op_enabled") else "?"))
except Exception as err:
    unreal.log("RTG ops listing failed: %s" % err)
unreal.log("RTG retargeter properties: %s" % ", ".join(sorted(p for p in dir(asset) if not p.startswith("_") and ("scale" in p.lower() or "pose" in p.lower()))))
unreal.EditorAssetLibrary.save_asset(path)
unreal.log("RTG saved %s" % path)
