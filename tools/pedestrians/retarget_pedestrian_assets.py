"""Makes a second pedestrian from a character on another skeleton (UE4 Mannequin or its own):
1. retargets the walk / idle / run animations to its skeleton (IK Retargeter, auto IK rigs),
2. adds LODs (the free packs ship with a single LOD).

Best effort on UE 5.8 (editor Python API varies): every step logs a line starting with "RETARGET"
and a failing step is reported and skipped, so the log says what worked. Idempotent.

  GTP_SRC_MESH   mesh the animations were made for (the game's Manny/player mesh)
  GTP_DST_MESH   mesh to make a pedestrian of, e.g. /Game/Man/Mesh/Full/SK_Man_Full_01
  GTP_SRC_ANIMS  comma separated animation asset paths (walk, idle, run)
  GTP_OUT_DIR    where the retargeted animations go (default /Game/Characters/Pedestrians/Retargeted)
  GTP_LODS       LOD count to build (default 3)

  UnrealEditor <project> -run=pythonscript -script=tools/pedestrians/retarget_pedestrian_assets.py
"""
import os

import unreal


def log(msg):
    unreal.log("RETARGET " + msg)


src_path = os.environ.get("GTP_SRC_MESH", "")
dst_path = os.environ.get("GTP_DST_MESH", "")
anims = [a for a in os.environ.get("GTP_SRC_ANIMS", "").split(",") if a]
out_dir = os.environ.get("GTP_OUT_DIR", "/Game/Characters/Pedestrians/Retargeted")
lods = int(os.environ.get("GTP_LODS", "3"))
if not (src_path and dst_path and anims):
    raise SystemExit("set GTP_SRC_MESH, GTP_DST_MESH and GTP_SRC_ANIMS")

src = unreal.load_asset(src_path)
dst = unreal.load_asset(dst_path)
tools = unreal.AssetToolsHelpers.get_asset_tools()


def make_rig(mesh, name):
    path = out_dir + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    rig = tools.create_asset(name, out_dir, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(mesh)
    ctrl.apply_auto_generated_retarget_definition()
    unreal.EditorAssetLibrary.save_loaded_asset(rig)
    return rig


try:
    src_rig = make_rig(src, "IK_" + src.get_name())
    dst_rig = make_rig(dst, "IK_" + dst.get_name())
    rt_name = "RTG_" + src.get_name() + "_to_" + dst.get_name()
    rt_path = out_dir + "/" + rt_name
    if unreal.EditorAssetLibrary.does_asset_exist(rt_path):
        retargeter = unreal.load_asset(rt_path)
    else:
        retargeter = tools.create_asset(rt_name, out_dir, unreal.IKRetargeter, unreal.IKRetargetFactory())
        ctrl = unreal.IKRetargeterController.get_controller(retargeter)
        ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, src_rig)
        ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, dst_rig)
        ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        unreal.EditorAssetLibrary.save_loaded_asset(retargeter)
    log("retargeter %s ready" % rt_path)
    batch = unreal.IKRetargetBatchOperation
    sequences = [unreal.load_asset(a) for a in anims]
    results = batch.duplicate_and_retarget(sequences, src, dst, retargeter, "", "", "_" + dst.get_name(), False)
    for r in results:
        log("animation %s" % r.get_path_name())
        unreal.EditorAssetLibrary.save_loaded_asset(r)
except Exception as exc:  # noqa: BLE001 (best effort, the log says what failed)
    log("FAILED retarget: %r" % (exc,))

try:
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    sub.regenerate_lod(dst, lods, [], False)
    log("LODs regenerated to %d on %s" % (lods, dst.get_path_name()))
    unreal.EditorAssetLibrary.save_loaded_asset(dst)
except Exception as exc:  # noqa: BLE001
    log("FAILED LOD generation: %r (add LODs with the Skeletal Mesh editor, LOD 1 at 50%%, LOD 2 at 20%%)" % (exc,))
