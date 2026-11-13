"""Prepares the Lyra weapons for the GTAL player (ADR-0011, docs/gameplay/combat.md).

Run in the Unreal editor of the GTAL project (Python), after the Lyra assets were
migrated with their Lyra paths (Epic content, never in Git):
    UnrealEditor GranTheftPraline.uproject -run=pythonscript -script=tools/combat/unreal/lyra_weapons_setup.py

What it does, and reports in Saved/GTPCombat/lyra-weapons-setup.txt:
1. Checks which migrated Lyra assets are present (meshes, montages, aim poses,
   muzzle flash, impacts, decals) and lists the missing ones.
2. Retargets the Lyra weapon animations (UE5 Mannequin skeleton) onto the player's
   skeleton (UEFN Mannequin) into /Game/Combat/Weapons/Anim/ with the suffix _UEFN,
   with its own retargeter (RTG_GTP_UE5_to_UEFN, chains mapped by name) built from the
   first IK rigs found for each side. Source mesh: Lyra's SKM_Manny (or any mesh on the
   animations' skeleton). Nothing of Lyra is modified.
3. Lists the slot names found in the player's animation blueprints (the Lyra
   montages play in UpperBody and FullBodyAdditivePreAim) and whether the player's
   skeleton has a weapon_r socket or bone (else hand_r with an offset).
4. Prints the DefaultGame.ini lines (Looks, Clips) matching what exists.
5. Prints the full paths of the Lyra weapon sounds (shots, dry fire), for the audio
   settings (UGTPAudioSettings::WeaponVoices).
Run it again after any change: retargeted assets that exist are kept.
"""
import os

import unreal

OUT_DIR = "/Game/Combat/Weapons/Anim"
SUFFIX = "_UEFN"

# Lyra paths (kept by the editor's Migrate).
MESHES = {
    "pistol": "/Game/Weapons/Pistol/Mesh/SK_Pistol",
    "carbine": "/Game/Weapons/Rifle/Mesh/SK_Rifle",
}
ANIM_FOLDERS = {"pistol": "/Game/Weapons/Pistol/Animations", "carbine": "/Game/Weapons/Rifle/Animations"}
AIM_POSES = {
    "pistol": "/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Pistol/MM_Pistol_Idle_ADS",
    "carbine": "/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS",
}
EFFECTS = [
    "/Game/Effects/Particles/Weapons/NS_WeaponFire_MuzzleFlash_Rifle",
    "/Game/Effects/Particles/Impacts/NS_ImpactConcrete",
    "/Game/Effects/Particles/Impacts/NS_ImpactGlass",
    "/Game/Effects/Materials/Decals/M_GlassBulletHoles_Decal",
]
PREFIX = {"pistol": "AM_Pistol", "carbine": "AM_Carbine"}
# IK rigs for our retargeter, first found wins (Game Animation Sample, Lyra, the pedestrians' UEFN rig).
SOURCE_RIGS = [
    "/Game/Characters/UE5_Mannequins/Rigs/IK_UE5_Mannequin_Retarget",
    "/Game/Characters/Heroes/Mannequin/Rigs/IK_Mannequin",
]
TARGET_RIGS = [
    "/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin",
    "/Game/Characters/Pedestrians/Retargeted/IK_SKM_UEFN_Mannequin",
]
MADE_RTG = "/Game/Combat/Weapons/RTG_GTP_UE5_to_UEFN"
# Lyra's mannequin, the source mesh of the weapon animations.
SOURCE_MESHES = ["/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny", "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Quinn"]
# Full-pose actions: the player's animation blueprint has no additive slot, so these play instead of
# the additive montages (Lyra's Fire exists only as an additive: it needs an additive slot).
ACTIONS = "/Game/Characters/Heroes/Mannequin/Animations/Actions"
LYRA_WORD = {"pistol": "Pistol", "carbine": "Rifle"}
ACTION_WORDS = ["Equip", "Reload", "DryFire", "Fire"]
SLOT = "DefaultSlot"
# Lyra weapon sounds, found by name wherever the migration put them.
SOUND_NAMES = ["MSS_Weapons_Pistol_Fire", "MSS_Weapons_Rifle_Fire", "Weapons_Pistol_DryFire_01", "Weapons_Rifle_DryFire_01"]
SLOT_NAMES = ["DefaultSlot", "UpperBody", "UpperBodyAdditive", "FullBody", "FullBodyAdditivePreAim", "Arms"]

eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
lines = []


def say(msg):
    lines.append(msg)
    unreal.log_warning("[LyraWeapons] " + msg)


def load(path):
    return eal.load_asset(path) if eal.does_asset_exist(path) else None


def assets_of_class(class_name, under="/Game"):
    found = []
    for data in registry.get_assets_by_path(under, recursive=True):
        if str(data.asset_class_path.asset_name) == class_name:
            found.append(data)
    return found


def check_present():
    say("== 1. Migrated Lyra assets")
    missing = 0
    for path in list(MESHES.values()) + list(AIM_POSES.values()) + EFFECTS:
        ok = eal.does_asset_exist(path)
        missing += 0 if ok else 1
        say("%s %s" % ("ok     " if ok else "MISSING", path))
    for kind, folder in ANIM_FOLDERS.items():
        names = [p for p in eal.list_assets(folder, recursive=False) if "/AM_" in p] if eal.does_directory_exist(folder) else []
        say("%s montages in %s: %s" % (kind, folder, ", ".join(os.path.basename(n).split(".")[0] for n in names) or "none"))
    return missing


def find_retargeter():
    names = [str(c.package_name) for c in assets_of_class("IKRetargeter")]
    say("IK retargeters in the project (not used, ours is built from the rigs): %s" % (", ".join(names) or "none"))
    return make_retargeter()


def first_asset(paths):
    for path in paths:
        asset = load(path)
        if asset:
            return path, asset
    return None, None


def chain_names(rig):
    controller = unreal.IKRigController.get_controller(rig)
    for fn in ("get_retarget_chains", "get_retarget_chain_names"):
        if hasattr(controller, fn):
            return [str(c.chain_name) if hasattr(c, "chain_name") else str(c) for c in getattr(controller, fn)()]
    return []


def make_retargeter():
    """Builds UE5 Mannequin -> UEFN Mannequin from the first IK rigs found (chains mapped by name)."""
    if eal.does_asset_exist(MADE_RTG):
        say("retargeter kept: %s" % MADE_RTG)
        return eal.load_asset(MADE_RTG)
    source_path, source = first_asset(SOURCE_RIGS)
    target_path, target = first_asset(TARGET_RIGS)
    if not source or not target:
        say("BLOCKED: no IK rig to build a retargeter (source %s, target %s)" % (source_path, target_path))
        return None
    folder, name = MADE_RTG.rsplit("/", 1)
    try:
        rtg = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory())
        controller = unreal.IKRetargeterController.get_controller(rtg)
        controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source)
        controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target)
        # UE 5.6+ retargeters are an op stack; older ones have no such calls.
        if hasattr(controller, "add_default_ops"):
            controller.add_default_ops()
        if hasattr(controller, "assign_ik_rig_to_all_ops"):
            controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, source)
            controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target)
        for mode in ("EXACT", "FUZZY"):
            try:
                controller.auto_map_chains(getattr(unreal.AutoMapChainType, mode), True)
                break
            except Exception as error:
                say("auto_map_chains %s failed: %s" % (mode, error))
        # Then by name, case-insensitive (the method create_pedestrian_retargeter.py proved on 5.8).
        by_lower = {n.lower(): n for n in chain_names(source)}
        mapped = []
        for chain in chain_names(target):
            wanted = by_lower.get(chain.lower())
            if not wanted:
                continue
            for args in ((wanted, chain), (unreal.Name(wanted), unreal.Name(chain))):
                try:
                    controller.set_source_chain(*args)
                    mapped.append(chain)
                    break
                except Exception:
                    pass
        say("chains mapped by name: %s" % (", ".join(mapped) or "none"))
        for side in (unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET):
            try:
                controller.auto_align_all_bones(side)
            except Exception as error:
                say("auto_align_all_bones skipped (%s)" % error)
        eal.save_loaded_asset(rtg)
        say("built retargeter %s from %s and %s" % (MADE_RTG, source_path, target_path))
        return rtg
    except Exception as error:
        say("BLOCKED: could not build a retargeter (%s)" % error)
        return None


def find_source_mesh(skeleton):
    """Lyra's mannequin, else any skeletal mesh on the animations' skeleton (read from the registry, nothing loaded)."""
    for path in SOURCE_MESHES:
        mesh = load(path)
        if mesh and (skeleton is None or mesh.skeleton == skeleton):
            return mesh
    if skeleton is None:
        return None
    wanted = skeleton.get_path_name()
    for data in assets_of_class("SkeletalMesh"):
        if wanted in str(data.get_tag_value("Skeleton") or ""):
            return data.get_asset()
    return None


def is_additive(anim):
    try:
        return anim.get_editor_property("additive_anim_type") != unreal.AdditiveAnimationType.AAT_NONE
    except Exception:
        return False


def find_mesh(word, skeleton=None):
    for data in assets_of_class("SkeletalMesh"):
        if word.lower() in str(data.asset_name).lower():
            mesh = data.get_asset()
            if skeleton is None or mesh.skeleton == skeleton:
                return mesh
    return None


def retarget(sources, source_mesh, target_mesh, rtg):
    if not eal.does_directory_exist(OUT_DIR):
        eal.make_directory(OUT_DIR)
    todo = []
    for src in sources:
        name = os.path.basename(src.get_path_name()).split(".")[0]
        if eal.does_asset_exist(OUT_DIR + "/" + name + SUFFIX):
            say("kept %s/%s%s" % (OUT_DIR, name, SUFFIX))
            continue
        todo.append(eal.find_asset_data(src.get_path_name()))
    if not todo:
        return
    op = unreal.IKRetargetBatchOperation
    made = None
    attempts = (
        dict(assets_to_retarget=todo, source_mesh=source_mesh, target_mesh=target_mesh, ik_retarget_asset=rtg,
             target_path=OUT_DIR, search="", replace="", prefix="", suffix=SUFFIX, overwrite_existing_files=False),
        dict(assets_to_retarget=todo, source_mesh=source_mesh, target_mesh=target_mesh, ik_retarget_asset=rtg,
             search="", replace="", prefix="", suffix=SUFFIX),
    )
    for kwargs in attempts:
        try:
            made = op.duplicate_and_retarget(**kwargs)
            break
        except Exception as error:
            say("duplicate_and_retarget (%s) failed: %s" % (", ".join(sorted(kwargs)), error))
    if made is None:
        say("BLOCKED: no duplicate_and_retarget call worked")
        return
    for data in made:
        if not hasattr(data, "package_name"):  # an object rather than asset data
            say("retargeted %s" % data.get_path_name())
            continue
        old = str(data.package_name)
        if old.startswith(OUT_DIR):
            say("retargeted %s" % old)
            continue
        new = OUT_DIR + "/" + str(data.asset_name)
        if eal.does_asset_exist(new):
            continue
        if eal.rename_asset(old, new):
            say("retargeted %s" % new)
        else:
            say("retargeted %s (left in place, could not move)" % old)
    # The batch retarget leaves its results in memory only: write them to disk, or the game finds nothing.
    saved = 0
    for path in eal.list_assets(OUT_DIR, recursive=True, include_folder=False):
        if eal.save_asset(path, only_if_is_dirty=False):
            saved += 1
        else:
            say("NOT SAVED %s" % path)
    say("saved %d assets in %s" % (saved, OUT_DIR))


def scan_slots(target_skeleton):
    say("== 3. Slots in the player's animation blueprints")
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    for data in assets_of_class("AnimBlueprint"):
        abp = data.get_asset()
        try:
            skel = abp.get_editor_property("target_skeleton")
        except Exception:
            skel = None
        if target_skeleton and skel != target_skeleton:
            continue
        rel = str(data.package_name)[len("/Game/"):] + ".uasset"
        path = os.path.join(content, rel)
        found = []
        try:
            with open(path, "rb") as f:
                blob = f.read()
            found = [s for s in SLOT_NAMES if s.encode("ascii") in blob]
        except OSError:
            pass
        say("%s: %s" % (data.package_name, ", ".join(found) or "no known slot name"))


def scan_hand(target_mesh):
    for socket in ("weapon_r", "hand_r"):
        say("player socket %s: %s" % (socket, "yes" if target_mesh.find_socket(socket) else "no"))
    try:
        pose = unreal.AnimPoseExtensions.get_reference_pose(target_mesh.skeleton)
        bones = [str(b) for b in unreal.AnimPoseExtensions.get_bone_names(pose)]
        for bone in ("weapon_r", "hand_r", "ik_hand_gun"):
            say("player bone %s: %s" % (bone, "yes" if bone in bones else "no"))
    except Exception as error:  # older Python API
        say("bones not readable (%s)" % error)


def main():
    missing = check_present()
    say("== 2. Retarget UE5 Mannequin -> player")
    rtg = find_retargeter()
    target_mesh = find_mesh("UEFN")
    sources = []
    for kind, folder in ANIM_FOLDERS.items():
        if eal.does_directory_exist(folder):
            sources += [eal.load_asset(p) for p in eal.list_assets(folder, recursive=False) if "/AM_MM_" in p]
        if load(AIM_POSES[kind]):
            sources.append(load(AIM_POSES[kind]))
        for word in ACTION_WORDS:
            action = load("%s/MM_%s_%s" % (ACTIONS, LYRA_WORD[kind], word))
            if action:
                sources.append(action)
                say("action %s: %s" % (action.get_name(), "additive" if is_additive(action) else "full pose"))
            else:
                say("action MM_%s_%s: not migrated" % (LYRA_WORD[kind], word))
    source_skeleton = sources[0].get_editor_property("skeleton") if sources else None
    source_mesh = find_source_mesh(source_skeleton)
    say("source mesh: %s ; target mesh: %s ; retargeter: %s" % (
        source_mesh.get_path_name() if source_mesh else "NONE", target_mesh.get_path_name() if target_mesh else "NONE",
        rtg.get_path_name() if rtg else "NONE"))
    if sources and source_mesh and target_mesh and rtg:
        retarget(sources, source_mesh, target_mesh, rtg)
    else:
        say("BLOCKED: retarget skipped (missing source animations, source mesh, player mesh or UE5->UEFN retargeter)")
    if target_mesh:
        scan_slots(target_mesh.skeleton)
        scan_hand(target_mesh)

    say("== 4. DefaultGame.ini lines matching what exists")
    say("[/Script/GranTheftPraline.GTPWeaponComponent]")
    for kind in ("pistol", "carbine"):
        mesh = MESHES[kind] + "." + os.path.basename(MESHES[kind]) if eal.does_asset_exist(MESHES[kind]) else ""
        say('+Looks=(Mesh="%s",MuzzleSocket="Muzzle",ClipPrefix="%s",MuzzleFlash="%s")' % (
            mesh, PREFIX[kind], "/Game/Effects/Particles/Weapons/NS_WeaponFire_MuzzleFlash_Rifle.NS_WeaponFire_MuzzleFlash_Rifle"))
    # Every clip in the slot the player's blueprint has, on the upper body only (UGTPWeaponComponent::UpperBodyRoot).
    # Full-pose action first, else the montage; an additive one is listed but never played in a full-pose slot.
    for kind in ("pistol", "carbine"):
        lyra = LYRA_WORD[kind]
        for ours, word in (("_Draw", "Equip"), ("_Reload", "Reload"), ("_DryFire", "DryFire"), ("_Fire", "Fire"), ("_AimLoop", "Idle_ADS")):
            chosen = None
            for name in ("MM_%s_%s%s" % (lyra, word, SUFFIX), "AM_MM_%s_%s%s" % (lyra, word, SUFFIX)):
                anim = load(OUT_DIR + "/" + name)
                if not anim:
                    continue
                seq = anim
                if isinstance(anim, unreal.AnimMontage):
                    try:
                        seq = anim.get_editor_property("slot_anim_tracks")[0].anim_track.anim_segments[0].get_editor_property("anim_reference")
                    except Exception:
                        seq = None
                if seq and not is_additive(seq):
                    chosen = name
                    break
                chosen = chosen or (name + " (additive: needs an additive slot)")
            if not chosen:
                say("; %s%s: no retargeted animation" % (PREFIX[kind], ours))
                continue
            if "additive" in chosen:
                say("; %s%s: %s" % (PREFIX[kind], ours, chosen))
                continue
            say('.Clips=(Name="%s%s",Asset="%s/%s.%s",Slot="%s")' % (PREFIX[kind], ours, OUT_DIR, chosen, chosen, SLOT))
    say("== 5. Lyra weapon sounds (full paths)")
    found = {}
    for data in registry.get_assets_by_path("/Game", recursive=True):
        name = str(data.asset_name)
        if name in SOUND_NAMES:
            found.setdefault(name, []).append(str(data.package_name) + "." + name)
    for name in SOUND_NAMES:
        for path in found.get(name, []) or ["MISSING"]:
            say("%s %s" % (name, path))
    missing += sum(1 for name in SOUND_NAMES if name not in found)
    say("missing Lyra assets: %d" % missing)

    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "GTPCombat")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "lyra-weapons-setup.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


main()
