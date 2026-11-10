"""Lists walk, idle and run animations and skeletal meshes of the project, to fill in
[/Script/GranTheftPraline.GTPPedestrianSubsystem] in DefaultGame.ini.

Run in the editor:  UnrealEditor <uproject> -ExecutePythonScript=<copy of this file without spaces in its path>
"""

import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
wanted = ("walk", "idle", "run", "jog")
rows = []
for data in registry.get_assets_by_path("/Game/Characters", recursive=True):
    cls = str(data.asset_class_path.asset_name)
    name = str(data.asset_name)
    if cls in ("AnimSequence", "AnimMontage") and any(w in name.lower() for w in wanted):
        rows.append(("anim", cls, str(data.get_soft_object_path())))
    elif cls == "SkeletalMesh":
        rows.append(("mesh", cls, str(data.get_soft_object_path())))
for kind, cls, path in sorted(rows):
    unreal.log_warning(f"PEDESTRIAN_ASSET {kind} {cls} {path}")
unreal.log_warning(f"PEDESTRIAN_ASSET total {len(rows)}")
