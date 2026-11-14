"""Fixes the player's runtime retarget on a copy: RTG_UEFNped_to_Metahuman -> RTG_UEFNped_to_Metahuman_Fixed.

The visual body's pelvis was left at the component's origin (portrait bench "rig: ..." lines, 2026-11-14), pulling
every vertex weighted to it towards the feet (a shirt's or hoodie's hem hanging like a tunic or a cape, a point
between the legs), with Kellan as with Nicolito. The retargeter's text export shows why: its Root Motion op has
TargetRoot = pelvis (SourceRoot = root, CopyFromSourceRoot): it puts the target's pelvis where the source's root is,
on the ground. And its op stack is there three times (Pelvis Motion, FK Chains, Run IK Rig, Root Motion, then the
same suffixed _0 and _1: create_pedestrian_retargeter.py added the default ops at each run), so the body was
retargeted three times per frame.

On the copy (the original, used by the game, is not touched): the duplicated ops are removed (a name ending in _<n>
whose base name is also in the stack) and the Root Motion op is disabled. The visual is attached to the pawn's mesh,
which the movement component moves: the body needs no root motion of its own, its root stays at the component's
origin like the source's. Then the copy is exported as text to Saved/GTPCombat/rig/ to check it.

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/character/fix_player_retargeter.py -unattended -nosplash

Try it: -PlayerVisualRetargeter=/Game/MetaHumans/GTP/RTG_UEFNped_to_Metahuman_Fixed.RTG_UEFNped_to_Metahuman_Fixed
(with -NicolitoPortrait, the "rig:" lines should no longer list the pelvis); PlayerVisualRetargeter in
DefaultGame.ini once proven. Report: Saved/GTPCombat/fix-player-retargeter.txt (log lines prefixed "RTGFIX").
"""
import os
import re

import unreal

ORIGINAL = "/Game/MetaHumans/GTP/RTG_UEFNped_to_Metahuman"
FIXED = ORIGINAL + "_Fixed"
ROOT_MOTION = "Root Motion"

eal = unreal.EditorAssetLibrary
lines = []
folder = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")


def say(text):
    lines.append(text)
    unreal.log("RTGFIX " + text)


def stack(controller):
    """(index, name, enabled) of every op."""
    found = []
    for index in range(controller.get_num_retarget_ops()):
        found.append((index, str(controller.get_op_name(index)), controller.get_retarget_op_enabled(index)))
    return found


def show(label, controller):
    say("%s: %s" % (label, "; ".join("%d %s%s" % (index, name, "" if enabled else " (disabled)") for index, name, enabled in stack(controller))))


def export_text(asset):
    target = os.path.join(folder, "rig")
    os.makedirs(target, exist_ok=True)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = os.path.join(target, asset.get_name() + ".t3d")
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    ok = unreal.Exporter.run_asset_export_task(task)
    return "exported to Saved/GTPCombat/rig/%s.t3d: %s" % (asset.get_name(), ok and os.path.exists(task.filename))


def main():
    if not eal.does_asset_exist(ORIGINAL):
        say("BLOCKED: %s missing" % ORIGINAL)
        return
    if eal.does_asset_exist(FIXED):
        say("%s exists: the same fix is applied again" % FIXED)
    elif not eal.duplicate_asset(ORIGINAL, FIXED):
        say("BLOCKED: %s not copied to %s" % (ORIGINAL, FIXED))
        return
    else:
        say("%s copied from %s" % (FIXED, ORIGINAL))
    asset = eal.load_asset(FIXED)
    controller = unreal.IKRetargeterController.get_controller(asset)
    show("ops before", controller)
    names = [name for _, name, _ in stack(controller)]
    duplicates = [index for index, name, _ in stack(controller)
                  if re.fullmatch(r"(.+)_\d+", name) and re.fullmatch(r"(.+)_\d+", name).group(1) in names]
    for index in sorted(duplicates, reverse=True):
        name = str(controller.get_op_name(index))
        say("op %d %s removed (a copy): %s" % (index, name, controller.remove_retarget_op(index)))
    for index, name, enabled in stack(controller):
        if name == ROOT_MOTION and enabled:
            controller.set_retarget_op_enabled(index, False)
            say("op %d %s disabled: %s" % (index, name, not controller.get_retarget_op_enabled(index)))
    show("ops after", controller)
    left = [name for _, name, _ in stack(controller)]
    if len(left) != len(set(left)) or any(re.fullmatch(r"(.+)_\d+", name) for name in left):
        say("NOT SAVED: the stack still has copies")
        return
    say("saved %s: %s" % (FIXED, eal.save_loaded_asset(asset, False)))
    say(export_text(asset))
    say("try: -PlayerVisualRetargeter=%s.%s" % (FIXED, FIXED.rsplit("/", 1)[-1]))


def run():
    try:
        main()
    except Exception as error:  # noqa: BLE001 (reported, the report is still written)
        say("BLOCKED: %r" % (error,))
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "fix-player-retargeter.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/fix-player-retargeter.txt")


run()
