"""List the crowd content of the Game Animation Sample project (read only, nothing is changed).

  UnrealEditor-Cmd GameAnimationSample.uproject -run=pythonscript \
    -script="<repo>/tools/pedestrians/survey_crowd_content.py" -unattended -nosplash -nullrhi

Lines "CROWD ...": per class the number of assets under /Game/CitySampleCrowd and /Game/MCO_Mocap_Basics, the skeletons
and skeletal meshes (with triangle counts at LOD0 when readable), the Mass entity configs, the animation sequences and the
Blueprints. Output also to Saved/crowd_survey.json (GTP_CROWD_OUT).
"""
import json
import os

import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
out = {}
for root in ("/Game/CitySampleCrowd", "/Game/MCO_Mocap_Basics"):
    by_class = {}
    for d in registry.get_assets_by_path(root, recursive=True):
        by_class.setdefault(str(d.asset_class_path.asset_name), []).append(str(d.package_name))
    out[root] = by_class
    for cls, names in sorted(by_class.items()):
        unreal.log("CROWD %s %s: %d" % (root, cls, len(names)))
        if cls in ("Skeleton", "SkeletalMesh", "MassEntityConfigAsset", "StateTree", "Blueprint", "AnimBlueprint", "IKRigDefinition", "IKRetargeter"):
            for n in names[:40]:
                unreal.log("CROWD   %s %s" % (cls, n))
        if cls == "SkeletalMesh":
            for n in names[:20]:
                try:
                    mesh = unreal.load_asset(n)
                    unreal.log("CROWD   mesh %s skeleton %s" % (n, mesh.skeleton.get_path_name() if mesh.skeleton else "none"))
                except Exception as err:
                    unreal.log("CROWD   mesh %s unreadable: %s" % (n, err))
path = os.environ.get("GTP_CROWD_OUT") or os.path.join(unreal.Paths.project_saved_dir(), "crowd_survey.json")
with open(path, "w") as f:
    json.dump(out, f, indent=1)
unreal.log("CROWD written %s" % path)
