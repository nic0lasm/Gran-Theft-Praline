"""Package closure of the Game Animation Sample's MetaHuman character (run in the sample, read only).

  UnrealEditor-Cmd GameAnimationSample.uproject -run=pythonscript \
    -script="<repo>/tools/character/gasp_metahuman_closure.py" -unattended -nosplash -nullrhi

Roots: every Blueprint / AnimBlueprint under /Game/MetaHumans plus /Game/Blueprints/RetargetedCharacters
(GTP_MH_ROOTS = comma separated packages to use instead). Follows hard AND soft references, but never
into /Game/Levels, /Game/Movies, /Game/Characters/Echo, Paragon, UE4_Mannequin (not needed).
Lines "CLOSURE ...": total per top folder, and the list of packages that are OUTSIDE /Game/MetaHumans
(those are what the MetaHuman pulls from the rest of the sample: check them against what the game has).
Writes Saved/gasp_metahuman_closure.json (GTP_CLOSURE_OUT): {"packages": [...], "files": ["Content-relative", ...], "bytes": N}.
"""
import json
import os

import unreal

SKIP = ("/Game/Levels/", "/Game/Movies/", "/Game/Characters/Echo/", "/Game/Characters/Paragon/", "/Game/Characters/UE4_Mannequin/")
EXT = (".uasset", ".umap", ".uexp", ".ubulk", ".uptnl")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
content = unreal.Paths.project_content_dir()

roots = [r for r in os.environ.get("GTP_MH_ROOTS", "").split(",") if r]
if not roots:
    for folder in ("/Game/MetaHumans", "/Game/Blueprints/RetargetedCharacters"):
        for d in registry.get_assets_by_path(folder, recursive=True):
            if str(d.asset_class_path.asset_name) in ("Blueprint", "AnimBlueprint"):
                roots.append(str(d.package_name))
unreal.log("CLOSURE roots %d" % len(roots))

opts = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)

seen, stack = set(), list(roots)
while stack:
    name = stack.pop()
    if name in seen or not name.startswith("/Game/"):
        continue
    if any(name.startswith(p) for p in SKIP):
        continue
    seen.add(name)
    for dep in registry.get_dependencies(name, opts) or []:
        stack.append(str(dep))

files, total, by_top = [], 0, {}
for name in sorted(seen):
    base = os.path.join(content, name[len("/Game/"):])
    size = 0
    for ext in EXT:
        if os.path.exists(base + ext):
            files.append(name[len("/Game/"):] + ext)
            size += os.path.getsize(base + ext)
    total += size
    top = "/".join(name.split("/")[:3])
    by_top[top] = by_top.get(top, 0) + size
for top in sorted(by_top, key=by_top.get, reverse=True):
    unreal.log("CLOSURE folder %s : %.1f MB" % (top, by_top[top] / 1048576.0))
outside = [n for n in sorted(seen) if not n.startswith("/Game/MetaHumans/")]
for n in outside:
    unreal.log("CLOSURE outside-MetaHumans %s" % n)
unreal.log("CLOSURE total %d packages, %.1f MB, %d outside /Game/MetaHumans" % (len(seen), total / 1048576.0, len(outside)))
out = os.environ.get("GTP_CLOSURE_OUT", os.path.join(unreal.Paths.project_saved_dir(), "gasp_metahuman_closure.json"))
with open(out, "w") as f:
    json.dump({"packages": sorted(seen), "files": files, "bytes": total}, f, indent=1)
unreal.log("CLOSURE written %s" % out)
