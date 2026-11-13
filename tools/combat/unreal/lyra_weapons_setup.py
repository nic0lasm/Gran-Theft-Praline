"""Prepares the Lyra weapons for the GTAL player (ADR-0011, docs/gameplay/combat.md).

Run in the Unreal editor of the GTAL project (Python), after the Lyra assets were
migrated with their Lyra paths (Epic content, never in Git):
    UnrealEditor GranTheftPraline.uproject -run=pythonscript -script=tools/combat/unreal/lyra_weapons_setup.py

What it does, and reports in Saved/GTPCombat/lyra-weapons-setup.txt:
1. Checks which migrated Lyra assets are present (meshes, montages, aim poses,
   muzzle flash, impacts, decals) and lists the missing ones.
2. Retargets the Lyra weapon animations (UE5 Mannequin skeleton) onto the player's
   skeleton (UEFN Mannequin) with an IK retargeter found in the project, into
   /Game/Combat/Weapons/Anim/ with the suffix _UEFN. Nothing of Lyra is modified.
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
# Our clip names (UGTPWeaponComponent::Clips) <- words in the Lyra montage names.
CLIP_WORDS = [("_Fire", "_Fire"), ("_Reload", "_Reload"), ("_Draw", "_Equip"), ("_DryFire", "_DryFire")]
PREFIX = {"pistol": "AM_Pistol", "carbine": "AM_Carbine"}
# IK rigs of the Game Animation Sample, used to build a retargeter when the project has none.
SOURCE_RIG = "/Game/Characters/UE5_Mannequins/Rigs/IK_UE5_Mannequin_Retarget"
TARGET_RIG = "/Game/Characters/UEFN_Mannequin/Rigs/IK_UEFN_Mannequin"
MADE_RTG = "/Game/Combat/Weapons/RTG_GTP_UE5_to_UEFN"
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
    candidates = assets_of_class("IKRetargeter")
    names = [str(c.package_name) for c in candidates]
    say("IK retargeters in the project: %s" % (", ".join(names) or "none"))
    best = None
    for c in candidates:
        n = str(c.asset_name).lower()
        if "uefn" in n and ("ue5" in n or "manny" in n or "mannequin" in n):
            best = c
            break
    if best:
        return best.get_asset()
    return make_retargeter()


def make_retargeter():
    """Builds UE5 Mannequin -> UEFN Mannequin from the sample's IK rigs (chains mapped by name)."""
    if eal.does_asset_exist(MADE_RTG):
        return eal.load_asset(MADE_RTG)
    source, target = load(SOURCE_RIG), load(TARGET_RIG)
    if not source or not target:
        say("no IK rig to build a retargeter (%s: %s, %s: %s)" % (SOURCE_RIG, bool(source), TARGET_RIG, bool(target)))
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
        controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        eal.save_loaded_asset(rtg)
        say("built retargeter %s from %s and %s" % (MADE_RTG, SOURCE_RIG, TARGET_RIG))
        return rtg
    except Exception as error:
        say("BLOCKED: could not build a retargeter (%s)" % error)
        return None


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
    made = unreal.IKRetargetBatchOperation.duplicate_and_retarget(todo, source_mesh, target_mesh, rtg, "", "", "", SUFFIX, True)
    for data in made:
        old = str(data.package_name)
        if old.startswith(OUT_DIR):
            continue
        new = OUT_DIR + "/" + str(data.asset_name)
        if eal.does_asset_exist(new):
            continue
        if eal.rename_asset(old, new):
            say("retargeted %s" % new)
        else:
            say("retargeted %s (left in place, could not move)" % old)


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
            sources += [eal.load_asset(p) for p in eal.list_assets(folder, recursive=False) if "/AM_" in p]
        if load(AIM_POSES[kind]):
            sources.append(load(AIM_POSES[kind]))
    source_skeleton = sources[0].get_editor_property("skeleton") if sources else None
    source_mesh = find_mesh("SK_Mannequin", source_skeleton) if source_skeleton else None
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
    for path in eal.list_assets(OUT_DIR, recursive=False) if eal.does_directory_exist(OUT_DIR) else []:
        name = os.path.basename(path).split(".")[0]
        kind = "pistol" if "Pistol" in name else ("carbine" if "Rifle" in name else None)
        if not kind:
            continue
        asset = OUT_DIR + "/" + name + "." + name
        if "Idle_ADS" in name:
            say('.Clips=(Name="%s_AimLoop",Asset="%s")' % (PREFIX[kind], asset))
            continue
        for ours, word in CLIP_WORDS:
            if name.endswith(word + SUFFIX):
                say('.Clips=(Name="%s%s",Asset="%s")' % (PREFIX[kind], ours, asset))
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
