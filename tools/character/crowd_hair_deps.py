"""Which of City Sample's hair assets Nicolito's hair needs and our project lacks, as a list for the copy tool.
Read only: changes no asset (docs/gameplay/player-character.md, "Hair").

For each hairstyle: the crowd's groom (Hair_<S|M|L>_<style> under /Game/Crowd/Character/Male), City Sample's binding
made for the chosen head's face (GroomBindings/GB_<head>_nrw_FaceMesh_<groom>) and, when the pack has one, its baked
texture (painted on the scalp under the strands, GTP_NICOLITO_HAIR=auto). For each of those present, every /Game
package it depends on, hard or soft, followed down; each one missing is written to the list, as a pattern relative
to Content (Crowd/.../Name.*, with its .uexp and .ubulk). A groom or binding not copied yet is written from where
the full City Sample keeps it. Run, copy the list, run again until it says 0 missing:

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/character/crowd_hair_deps.py -unattended -nosplash
    python tools/vehicles/copy_citysample_content.py --src <CitySample>/Content --dst <project>/GranTheftPraline/Content \
        --list <project>/GranTheftPraline/Saved/CitySampleCopy/missing.txt --dry-run   (then without --dry-run, editor closed)

Env: GTP_HAIR_STYLES (default BuzzCut,SideSweptFringe,Messy,PulledBack), GTP_NICOLITO_HEAD (default m_004).
GTP_DEPS_OF=/Game/MetaHumans/GTP/Nicolito/BP_Nicolito_Full (comma-separated packages) lists instead everything those
need under /Game (Saved/CitySampleCopy/closure.txt, size by top folder): what another copy of the project, such as
the Mac's, must receive at the same paths for the player's visual.
GTP_CROWD_HAIR=1 lists instead what the hair of the crowd needs (docs/gameplay/pedestrians-mass.md, "Hair"): the
hair meshes City Sample shows from a distance (Hair_*_LOD5 of its heads) and its baked scalp textures, the player's
style left out, with everything they depend on; same list file, same copy, run again until 0 missing.
Report: Saved/GTPCombat/crowd-hair-deps.txt (lines also in the log, prefixed "HAIRDEPS"); the list:
Saved/CitySampleCopy/missing.txt.
"""
import os
import re

import unreal

CROWD_MALE = "/Game/Crowd/Character/Male"
GROOM_BINDINGS = CROWD_MALE + "/GroomBindings"
BAKED_DIR = "/Game/Crowd/Character/Shared/Textures/Hair"
# Where the full City Sample keeps each male groom (its listing, 2026-11-14), and the styles it has a baked texture for.
CITYSAMPLE_GROOMS = {
    "BuzzCut": "m_004/Hair/Hair/Hair_S_BuzzCut",
    "PulledBack": "m_004/Hair/Hair/Hair_S_PulledBack",
    "SideSweptFringe": "m_003/Hair/Hair/Hair_M_SideSweptFringe",
    "Messy": "m_002/Hair/Hair/Hair_S_Messy",
    "CurlyFade": "m_002/Hair/Hair/Hair_S_CurlyFade",
    "AfroFade": "m_001/Hair/Hair/Hair_S_AfroFade",
    "HairLoss": "m_006/Hair/Hair/Hair_S_HairLoss",
}
BAKED_STYLES = {"BuzzCut", "PulledBack", "AfroFade", "HairLoss"}
# For the crowd (GTP_CROWD_HAIR=1): where the full City Sample keeps the hair mesh of each style (its listing,
# 2026-11-14) and the styles with a baked scalp texture. PulledBack is the player's and is left out.
CROWD_ROOT = "/Game/Crowd/Character"
CROWD_HAIR_MESHES = [
    "Female/f_001/Hair/Helmets/Hair_S_Coil_Helmet_LOD5",
    "Female/f_002/Hair/Helmets/Hair_S_Pixie_Mesh_LOD5",
    "Female/f_003/Hair/Helmets/Hair_S_LowPonytail_Helmet_LOD5",
    "Female/f_004/Hair/Helmets/Hair_S_Updo_Helmet_LOD5",
    "Female/f_005/Hair/Helmets/Hair_M_BobCurly_Helmet_LOD5",
    "Male/m_001/Hair/Helmets/Hair_S_AfroFade_Helmet_LOD5",
    "Male/m_002/Hair/Helmets/Hair_S_CurlyFade_Helmet_LOD5",
    "Male/m_003/Hair/Helmets/Hair_M_SideSweptFringe_Mesh_LOD5",
    "Male/m_006/Hair/Helmets/Hair_S_HairLoss_Helmet_LOD5",
]
CROWD_BAKED = ["Hair_S_AfroFade", "Hair_S_BuzzCut", "Hair_S_Coil", "Hair_S_HairLoss", "Hair_S_Updo"]

eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
lines = []


def say(text):
    lines.append(text)
    unreal.log("HAIRDEPS " + text)


def exists(package):
    return len(registry.get_assets_by_package_name(package)) > 0


def pattern(package):
    """/Game/A/B -> A/B.* (relative to Content)."""
    return package[len("/Game/"):] + ".*"


def dependencies(package):
    options = unreal.AssetRegistryDependencyOptions()
    for name, value in (("include_hard_package_references", True), ("include_soft_package_references", True),
                        ("include_searchable_names", False), ("include_soft_management_references", False),
                        ("include_hard_management_references", False)):
        try:
            options.set_editor_property(name, value)
        except Exception:  # noqa: BLE001 (left at its default)
            pass
    return [str(name) for name in (registry.get_dependencies(package, options) or [])]


def follow(seeds, missing, closure=None):
    """Every /Game package the present seeds depend on, followed down; the missing ones added to `missing`, the
    present ones (seeds included) to `closure` when given."""
    seen = set()
    todo = [seed for seed in seeds if exists(seed)]
    while todo:
        package = todo.pop()
        if package in seen:
            continue
        seen.add(package)
        for dep in dependencies(package):
            if not dep.startswith("/Game/") or dep in seen:
                continue
            if exists(dep):
                todo.append(dep)
            elif dep not in missing:
                missing[dep] = package
    if closure is not None:
        closure.update(seen)
    return len(seen)


def package_bytes(package):
    """Size on disk of the package's files (.uasset or .umap, .uexp, .ubulk, .uptnl)."""
    base = os.path.join(unreal.Paths.project_content_dir(), *package[len("/Game/"):].split("/"))
    return sum(os.path.getsize(base + ext) for ext in (".uasset", ".umap", ".uexp", ".ubulk", ".uptnl") if os.path.exists(base + ext))


def closure_of(roots):
    """GTP_DEPS_OF: everything those packages need under /Game, to bring them to another copy of the project (the
    Mac), as patterns relative to Content, with the size by top folder. Missing ones are reported, not listed."""
    missing, closure = {}, set()
    for root in roots:
        say("%s: %s" % (root, "present" if exists(root) else "MISSING"))
    follow(roots, missing, closure)
    sizes = {}
    for package in closure:
        top = "/".join(package.split("/")[2:4])
        sizes[top] = sizes.get(top, 0) + package_bytes(package)
    say("closure: %d packages, %.2f GB" % (len(closure), sum(sizes.values()) / 1e9))
    for top, size in sorted(sizes.items(), key=lambda item: -item[1]):
        say("  %s: %.3f GB" % (top, size / 1e9))
    for package, needed_by in sorted(missing.items()):
        say("  missing here too %s (for %s)" % (package, needed_by))
    path = os.path.join(unreal.Paths.project_saved_dir(), "CitySampleCopy", "closure.txt")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("# written by tools/character/crowd_hair_deps.py (GTP_DEPS_OF): packages to bring (relative to Content)\n")
        handle.write("".join(pattern(package) + "\n" for package in sorted(closure)))
    say("closure list written to Saved/CitySampleCopy/closure.txt")


def crowd_hair():
    """GTP_CROWD_HAIR=1: the crowd's hair meshes and baked scalp textures, and what they depend on, missing here."""
    seeds = ["%s/%s" % (CROWD_ROOT, mesh) for mesh in CROWD_HAIR_MESHES]
    seeds += ["%s/%s_baked/%s_baked" % (BAKED_DIR, name, name) for name in CROWD_BAKED]
    missing = {}
    for seed in seeds:
        present = exists(seed)
        kind = "-"
        if present:
            try:
                kind = eal.load_asset(seed).get_class().get_name()
            except Exception as error:  # noqa: BLE001 (reported)
                kind = "not loaded (%r)" % (error,)
        else:
            missing.setdefault(seed, "the crowd's hair")
        say("%s: %s, class %s" % (seed, "present" if present else "missing", kind))
    walked = follow(seeds, missing)
    say("crowd hair: %d seeds, %d packages followed" % (len(seeds), walked))
    say("%d missing" % len(missing))
    for package, needed_by in sorted(missing.items()):
        say("  missing %s (for %s)" % (package, needed_by))
    return sorted(missing)


def main():
    try:
        registry.search_all_assets(True)
    except Exception as error:  # noqa: BLE001 (the commandlet's scan is used)
        say("asset scan not forced (%r)" % (error,))
    head = os.environ.get("GTP_NICOLITO_HEAD", "m_004").strip()
    styles = [s.strip() for s in os.environ.get("GTP_HAIR_STYLES", "BuzzCut,SideSweptFringe,Messy,PulledBack").split(",") if s.strip()]
    if not re.fullmatch(r"m_\d{3}", head) or not all(re.fullmatch(r"[A-Za-z]+", s) for s in styles):
        say("BLOCKED: GTP_NICOLITO_HEAD like m_004, GTP_HAIR_STYLES style names like BuzzCut (got '%s', %s)" % (head, styles))
        return []
    face = "%s/%s/Face/%s_nrw_FaceMesh" % (CROWD_MALE, head, head)
    say("head %s, face %s: %s" % (head, face, "present" if exists(face) else "MISSING"))
    missing = {}
    for style in styles:
        where = CITYSAMPLE_GROOMS.get(style)
        if where is None:
            say("%s: not a male groom in City Sample's listing (%s)" % (style, ", ".join(sorted(CITYSAMPLE_GROOMS))))
            continue
        groom = "%s/%s" % (CROWD_MALE, where)
        groom_name = groom.rsplit("/", 1)[-1]
        binding = "%s/GB_%s_nrw_FaceMesh_%s" % (GROOM_BINDINGS, head, groom_name)
        seeds = [groom, binding]
        if style in BAKED_STYLES:
            seeds.append("%s/%s_baked/%s_baked" % (BAKED_DIR, groom_name, groom_name))
        for seed in seeds:
            if not exists(seed):
                missing.setdefault(seed, "hairstyle " + style)
        walked = follow(seeds, missing)
        state = ", ".join("%s %s" % (seed.rsplit("/", 1)[-1], "present" if exists(seed) else "missing") for seed in seeds)
        say("%s: %s; %d packages followed" % (style, state, walked))
        if exists(binding):
            try:
                target = eal.load_asset(binding).get_editor_property("target_skeletal_mesh")
                say("  binding's face: %s (%s)" % (target.get_path_name() if target else "none",
                                                    "the head's" if target and target.get_path_name().split(".")[0] == face else "NOT the head's"))
            except Exception as error:  # noqa: BLE001 (reported)
                say("  binding's face not read (%r)" % (error,))
    say("%d missing" % len(missing))
    for package, needed_by in sorted(missing.items()):
        say("  missing %s (for %s)" % (package, needed_by))
    return sorted(missing)


def run():
    roots = [r.strip().split(".")[0] for r in os.environ.get("GTP_DEPS_OF", "").split(",") if r.strip()]
    missing = None
    try:
        if roots:
            closure_of(roots)
        elif os.environ.get("GTP_CROWD_HAIR", "").strip() not in ("", "0"):
            try:
                registry.search_all_assets(True)
            except Exception as error:  # noqa: BLE001 (the commandlet's scan is used)
                say("asset scan not forced (%r)" % (error,))
            missing = crowd_hair()
        else:
            missing = main()
    except Exception as error:  # noqa: BLE001 (reported, the report is still written)
        say("BLOCKED: %r" % (error,))
    if not roots:
        copy_dir = os.path.join(unreal.Paths.project_saved_dir(), "CitySampleCopy")
        os.makedirs(copy_dir, exist_ok=True)
        with open(os.path.join(copy_dir, "missing.txt"), "w", encoding="utf-8") as handle:
            handle.write("# written by tools/character/crowd_hair_deps.py: City Sample paths to copy (relative to Content)\n")
            handle.write("".join(pattern(package) + "\n" for package in (missing or [])))
    folder = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "crowd-hair-deps.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/crowd-hair-deps.txt")


run()
