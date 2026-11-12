"""Read-only survey of the MetaHuman part of the Game Animation Sample (run in the sample project).

Finds what the sample ships to drive a MetaHuman with the GASP animation (the MetaHumans folder,
the retargeted character blueprints) and measures its weight, so that the PC (low disk) knows what
copying it costs before anything is copied.

  UnrealEditor-Cmd GameAnimationSample.uproject -run=pythonscript \
    -script="<repo>/tools/character/gasp_metahuman_survey.py" -unattended -nosplash -nullrhi

Output: lines "SURVEY ..." in the log and Saved/gasp_metahuman_survey.json (or GTP_SURVEY_OUT).
Sizes are the .uasset/.uexp/.ubulk bytes under Content/ of the sample.
"""
import json
import os

import unreal

FOLDERS = ["/Game/MetaHumans", "/Game/Blueprints/RetargetedCharacters", "/Game/Characters"]
KEYWORDS = ("metahuman", "kellan", "retarget")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
content = unreal.Paths.project_content_dir()


def size_of(package):
    base = os.path.join(content, package[len("/Game/"):])
    total = 0
    for ext in (".uasset", ".uexp", ".ubulk", ".uptnl", ".umap"):
        if os.path.exists(base + ext):
            total += os.path.getsize(base + ext)
    return total


rows = []
for folder in FOLDERS:
    for data in registry.get_assets_by_path(folder, recursive=True):
        package = str(data.package_name)
        rows.append({"package": package, "class": str(data.asset_class_path.asset_name), "bytes": size_of(package)})

by_folder = {}
for r in rows:
    parts = r["package"].split("/")
    key = "/".join(parts[:4]) if len(parts) > 4 else "/".join(parts[:-1])
    by_folder[key] = by_folder.get(key, 0) + r["bytes"]
for key in sorted(by_folder, key=by_folder.get, reverse=True)[:25]:
    unreal.log("SURVEY folder %s : %.1f MB" % (key, by_folder[key] / 1048576.0))

blueprints = [r for r in rows if r["class"] in ("Blueprint", "AnimBlueprint") and any(k in r["package"].lower() for k in KEYWORDS)]
for r in blueprints:
    unreal.log("SURVEY blueprint %s (%s) %.1f MB" % (r["package"], r["class"], r["bytes"] / 1048576.0))
meshes = [r for r in rows if r["class"] == "SkeletalMesh" and any(k in r["package"].lower() for k in KEYWORDS)]
for r in meshes:
    unreal.log("SURVEY skeletal mesh %s %.1f MB" % (r["package"], r["bytes"] / 1048576.0))

total = sum(r["bytes"] for r in rows if "metahuman" in r["package"].lower() or "retargeted" in r["package"].lower())
unreal.log("SURVEY total MetaHuman + RetargetedCharacters: %.2f GB" % (total / 1073741824.0))
out = os.environ.get("GTP_SURVEY_OUT", os.path.join(unreal.Paths.project_saved_dir(), "gasp_metahuman_survey.json"))
with open(out, "w") as f:
    json.dump({"by_folder": by_folder, "blueprints": blueprints, "meshes": meshes, "rows": rows}, f, indent=1)
unreal.log("SURVEY written %s" % out)
