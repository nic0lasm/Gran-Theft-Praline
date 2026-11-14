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


def follow(seeds, missing):
    """Every /Game package the present seeds depend on, followed down; the missing ones added to `missing`."""
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
    return len(seen)


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
    missing = []
    try:
        missing = main()
    except Exception as error:  # noqa: BLE001 (reported, the report is still written)
        say("BLOCKED: %r" % (error,))
    copy_dir = os.path.join(unreal.Paths.project_saved_dir(), "CitySampleCopy")
    os.makedirs(copy_dir, exist_ok=True)
    with open(os.path.join(copy_dir, "missing.txt"), "w", encoding="utf-8") as handle:
        handle.write("# written by tools/character/crowd_hair_deps.py: City Sample paths to copy (relative to Content)\n")
        handle.write("".join(pattern(package) + "\n" for package in missing))
    folder = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "crowd-hair-deps.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/crowd-hair-deps.txt, list to Saved/CitySampleCopy/missing.txt")


run()
