"""Gives the player's animation blueprint an upper-body layer, for the weapon poses (docs/gameplay/combat.md).

Measured on candidate 31b: the aim pose (Lyra's MM_Pistol_Idle_ADS retargeted) plays on the full body even through
the blend mask (a WeightFactor profile only shapes the blend in and out), and lowers the pelvis by about 65 cm.
GTA-like aiming moves the upper body only. This script, run in the GTAL editor (the game module must be built:
it calls UGTPAnimGraphTools::AddUpperBodyLayer):

1. Finds the player's animation blueprint (the anim class of PlayerPawnClass's mesh, else the Game Animation
   Sample's SandboxCharacter_CMC_ABP).
2. Copies its .uasset, and its skeleton's, to Saved/GTPCombat/backup/ (kept if already there: the first copy is the
   original). To undo: close the editor and copy them back.
3. Adds, at the end of its AnimGraph: final pose cached -> Layered blend per bone (from spine_01, mesh-space
   rotation) with the base from the cache and the upper body from a Slot "UpperBody" fed by the same cache ->
   output. Registers the slot on the skeleton. Already there = kept.
4. Saves the blueprint and the skeleton only when the blueprint compiles.

    UnrealEditor GranTheftPraline.uproject -run=pythonscript -script=tools/combat/unreal/player_upper_body_layer.py

Report: Saved/GTPCombat/player-upper-body-layer.txt (lines also in the log, prefixed "PUBL"). The weapon then plays
its full-pose clips in UpperBody by itself (UGTPWeaponComponent::UpperBodySlot): the log says
"weapon: full-pose clips play in slot UpperBody". These assets are Epic's sample content, out of Git: the Mac's
copy of the project needs the same run.
"""
import os
import shutil

import unreal

PAWN_CLASS = "/Game/Characters/Player/BP_PlayerCharacter.BP_PlayerCharacter_C"
FALLBACK_ABP = "/Game/Blueprints/SandboxCharacter_CMC_ABP"
SLOT = "UpperBody"
ROOT_BONE = "spine_01"

eal = unreal.EditorAssetLibrary
lines = []


def say(text):
    lines.append(text)
    unreal.log("PUBL " + text)


def player_abp_path():
    """Package path of the anim blueprint on the player pawn's mesh."""
    try:
        cls = unreal.load_class(None, PAWN_CLASS)
        cdo = unreal.get_default_object(cls)
        mesh = cdo.get_editor_property("mesh")
        anim_class = mesh.get_editor_property("anim_class") if mesh else None
        if anim_class:
            path = anim_class.get_path_name().split(".")[0]
            say("player pawn %s: anim class %s" % (PAWN_CLASS, anim_class.get_path_name()))
            return path
        say("player pawn %s: no anim class on its mesh" % PAWN_CLASS)
    except Exception as error:
        say("player pawn %s not read (%s)" % (PAWN_CLASS, error))
    return FALLBACK_ABP


def package_file(package):
    """Content/<...>.uasset for a /Game package."""
    if not package.startswith("/Game/"):
        return None
    content = unreal.SystemLibrary.convert_to_absolute_path(unreal.Paths.project_content_dir())
    return os.path.join(content, *package[len("/Game/"):].split("/")) + ".uasset"


def back_up(package, folder):
    source = package_file(package)
    if not source or not os.path.exists(source):
        say("backup: no file for %s" % package)
        return False
    target = os.path.join(folder, os.path.basename(source))
    if os.path.exists(target):
        say("backup kept: Saved/GTPCombat/backup/%s" % os.path.basename(source))
        return True
    shutil.copy2(source, target)
    say("backup: Saved/GTPCombat/backup/%s" % os.path.basename(source))
    return True


def main():
    saved = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")
    backup = os.path.join(saved, "backup")
    os.makedirs(backup, exist_ok=True)
    path = player_abp_path()
    abp = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not abp:
        say("BLOCKED: no animation blueprint at %s" % path)
        return saved
    skeleton = abp.get_editor_property("target_skeleton")
    skeleton_path = skeleton.get_path_name().split(".")[0] if skeleton else None
    say("blueprint %s, skeleton %s" % (path, skeleton_path))
    if not hasattr(unreal, "GTPAnimGraphTools"):
        say("BLOCKED: unreal.GTPAnimGraphTools missing (build the game module first)")
        return saved
    if not back_up(path, backup) or (skeleton_path and not back_up(skeleton_path, backup)):
        say("BLOCKED: no backup, nothing changed")
        return saved
    report = unreal.GTPAnimGraphTools.add_upper_body_layer(abp, SLOT, ROOT_BONE)
    say(report)
    if report.startswith("ok"):
        saved_abp = eal.save_loaded_asset(abp, False)
        saved_skeleton = eal.save_loaded_asset(skeleton, False) if skeleton else True
        say("saved: blueprint %s, skeleton %s" % (saved_abp, saved_skeleton))
    elif report.startswith("kept"):
        say("nothing to save")
    else:
        say("NOT SAVED: the blueprint in memory may have unlinked nodes; close the editor without saving")
    return saved


def run():
    saved = main()
    out = os.path.join(saved, "player-upper-body-layer.txt")
    with open(out, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/player-upper-body-layer.txt")


run()
