"""List the package closure of the Game Animation Sample character.

Run inside the Game Animation Sample project (read only), for example:
  UnrealEditor-Cmd GameAnimationSample.uproject -run=pythonscript \
    -script="<repo>/tools/character/gasp_dependency_closure.py" -unattended -nosplash -nullrhi

Walks the asset registry (hard and soft package dependencies) from the roots
below and writes the sorted list of /Game packages and of their files
(relative to Content/) to the JSON file named by
the GTP_CLOSURE_OUT environment variable (default: Saved/gasp_closure.json in
the sample project). Engine and plugin packages are listed apart: they are not
copied, the matching plugins are enabled instead.
"""

import json
import os

import unreal

ROOTS = [
    "/Game/Blueprints/SandboxCharacter_CMC",
    "/Game/Blueprints/SandboxCharacter_CMC_ABP",
    "/Game/Blueprints/GM_Sandbox",
    "/Game/Blueprints/PC_Sandbox",
    "/Game/Blueprints/Cameras/CameraAsset_SandboxCharacter",
    "/Game/Blueprints/Cameras/CameraDirector_SandboxCharacter",
]
INPUT_DIR = "/Game/Input"

# Soft references into these folders are not followed: they are the sample's
# alternative characters (visual overrides of GM_Sandbox), unused by our game.
# Hard references are always followed (traversal needs LevelBlock_Traversable).
SKIP_SOFT_PREFIXES = (
    "/Game/Characters/Echo/",
    "/Game/Characters/Paragon/",
    "/Game/Characters/UE4_Mannequin/",
    "/Game/MetaHumans/",
    "/Game/Levels/",
    "/Game/Movies/",
    "/Game/Blueprints/RetargetedCharacters/",
)
PACKAGE_EXTENSIONS = (".uasset", ".umap", ".uexp", ".ubulk", ".uptnl")


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()

    roots = list(ROOTS)
    for data in registry.get_assets_by_path(INPUT_DIR, recursive=True):
        roots.append(str(data.package_name))

    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False,
    )

    hard_only = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=False,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False,
    )

    seen = set()
    external = set()
    edges = []
    missing = []
    stack = list(roots)
    while stack:
        name = stack.pop()
        if name in seen:
            continue
        if not name.startswith("/Game/"):
            external.add(name)
            continue
        seen.add(name)
        deps = registry.get_dependencies(name, options)
        if deps is None:
            missing.append(name)
            continue
        hard = set(str(d) for d in (registry.get_dependencies(name, hard_only) or []))
        for dep in deps:
            dep = str(dep)
            if dep.startswith("/Script/"):
                continue
            kind = "hard" if dep in hard else "soft"
            if dep.startswith("/Game/"):
                edges.append([name, dep, kind])
            if kind == "soft" and dep.startswith(SKIP_SOFT_PREFIXES):
                continue
            if dep not in seen:
                stack.append(dep)

    content_dir = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    files = []
    for name in sorted(seen):
        rel = name[len("/Game/"):]
        for ext in PACKAGE_EXTENSIONS:
            if os.path.exists(os.path.join(content_dir, rel + ext)):
                files.append(rel + ext)

    out = os.environ.get("GTP_CLOSURE_OUT") or os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "gasp_closure.json")
    with open(out, "w") as f:
        json.dump({"roots": roots, "packages": sorted(seen), "external": sorted(external),
                   "no_registry_entry": sorted(missing), "files": files, "edges": edges}, f, indent=1)
    unreal.log_warning("[Closure] %d /Game packages, %d external, written to %s" % (len(seen), len(external), out))


main()
