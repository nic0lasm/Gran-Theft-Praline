"""Why the crowd's head material cooks without shaders, and the fix (docs/gameplay/pedestrians-mass.md, "Head material").

The cook of candidate 40 fails /Game/Crowd/Character/Shared/Materials/MetaHuman/M_Crowd_Head_v2 for PCD3D_SM6:
"Shader TBasePassCSFNoLightMapPolicy, VF FNaniteVertexFactory: Shader is using too many SRVs: 65 (only 64
supported)". One failed permutation leaves the whole material without a shader map, and the packaged game then logs
"Loading a material resource None with an invalid ShaderMap" and "Tried to access an uncooked shader map ID" as soon
as anything of that hierarchy loads (the crowd's heads, Nicolito's head).

The failing permutation is the Nanite one. Our pedestrians and the player draw these heads as plain skinned meshes,
so the material does not need it: without "Used with Nanite" the permutation is not compiled and the material cooks.

Read only unless GTP_APPLY=1. It reports:
  - the material's usage flags and the number of textures it samples;
  - who references it, followed up to the skeletal meshes (which meshes pull it in at run time);
  - whether those meshes have Nanite enabled (a Nanite mesh would set the usage flag again in the editor).
With GTP_APPLY=1 it clears "Used with Nanite" and "Automatically set usage in editor" on the material, recompiles and
saves it, and turns Nanite off on the skeletal meshes of the hierarchy that have it on (saved too).

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/pedestrians/fix_crowd_head_material.py -unattended -nosplash

Env: GTP_HEAD_MATERIAL (default the path above; comma-separated for several), GTP_APPLY=1.
Report: Saved/GTPPedestrians/crowd-head-material.txt (lines also in the log, prefixed "HEADMAT").
The assets are Epic's and stay outside Git: run it again on any fresh copy of the pack.
"""
import os

import unreal

DEFAULT = "/Game/Crowd/Character/Shared/Materials/MetaHuman/M_Crowd_Head_v2"
USAGES = ("used_with_skeletal_mesh", "used_with_morph_targets", "used_with_nanite", "used_with_clothing",
          "used_with_static_lighting", "used_with_instanced_static_meshes", "used_with_hair_strands",
          "used_with_geometry_cache", "used_with_niagara_mesh_particles", "used_with_editor_compositing")

eal = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
apply_changes = os.environ.get("GTP_APPLY", "0") not in ("", "0")
lines = []


def say(text):
    lines.append(text)
    unreal.log("HEADMAT " + text)


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:  # noqa: BLE001 (the property does not exist in this engine version)
        return default


def referencers(package):
    options = unreal.AssetRegistryDependencyOptions()
    for name, value in (("include_hard_package_references", True), ("include_soft_package_references", False),
                        ("include_searchable_names", False), ("include_soft_management_references", False),
                        ("include_hard_management_references", False)):
        try:
            options.set_editor_property(name, value)
        except Exception:  # noqa: BLE001
            pass
    return [str(name) for name in (registry.get_referencers(package, options) or [])]


def class_of(package):
    assets = registry.get_assets_by_package_name(package)
    if not assets:
        return ""
    try:
        return str(assets[0].asset_class_path.asset_name)
    except Exception:  # noqa: BLE001 (older API)
        return str(prop(assets[0], "asset_class", ""))


def users_of(package):
    """Everything that hard-references the package, followed up; returns ({package: class}, [skeletal meshes])."""
    seen, meshes = {}, []
    todo = [package]
    while todo:
        current = todo.pop()
        for user in referencers(current):
            if user in seen or not user.startswith("/Game/"):
                continue
            kind = class_of(user)
            seen[user] = kind
            if kind == "SkeletalMesh":
                meshes.append(user)  # a mesh ends the chain: what references a mesh is not using the material itself
            elif kind in ("Material", "MaterialInstanceConstant"):
                todo.append(user)
    return seen, sorted(meshes)


def nanite_enabled(mesh):
    settings = prop(mesh, "nanite_settings")
    if settings is None:
        return None
    try:
        return bool(settings.get_editor_property("enabled"))
    except Exception:  # noqa: BLE001
        return None


def fix(path):
    material = eal.load_asset(path)
    if not material:
        say("%s: MISSING" % path)
        return
    say("%s: %s" % (path, material.get_class().get_name()))
    if not isinstance(material, unreal.Material):
        say("  not a base material: give the base material of the hierarchy (GTP_HEAD_MATERIAL)")
        return
    say("  usages: " + ", ".join("%s=%s" % (name[len("used_with_"):], prop(material, name, "?")) for name in USAGES))
    say("  automatically set usage in editor: %s" % prop(material, "automatically_set_usage_in_editor", "?"))
    try:
        textures = unreal.MaterialEditingLibrary.get_used_textures(material)
        say("  textures sampled: %d" % len(textures))
    except Exception as error:  # noqa: BLE001
        say("  textures sampled: unknown (%s)" % error)

    users, meshes = users_of(path)
    instances = sorted(p for p, kind in users.items() if kind in ("Material", "MaterialInstanceConstant"))
    say("  material instances under it: %d" % len(instances))
    for instance in instances[:60]:
        say("    %s" % instance)
    say("  skeletal meshes that use the hierarchy: %d" % len(meshes))
    nanite_meshes = []
    for package in meshes:
        mesh = eal.load_asset(package)
        enabled = nanite_enabled(mesh) if mesh else None
        say("    %s (Nanite %s)" % (package, {True: "ON", False: "off", None: "unknown"}[enabled]))
        if enabled:
            nanite_meshes.append((package, mesh))

    if not apply_changes:
        say("  read only (GTP_APPLY=1 to clear Used with Nanite%s)" % (
            " and turn Nanite off on %d meshes" % len(nanite_meshes) if nanite_meshes else ""))
        return
    for package, mesh in nanite_meshes:
        settings = mesh.get_editor_property("nanite_settings")
        settings.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", settings)
        say("    Nanite off: %s, saved %s" % (package, eal.save_loaded_asset(mesh, only_if_is_dirty=False)))
    material.modify()
    material.set_editor_property("automatically_set_usage_in_editor", False)
    material.set_editor_property("used_with_nanite", False)
    unreal.MaterialEditingLibrary.recompile_material(material)
    say("  Used with Nanite cleared, usage no longer set automatically, recompiled, saved %s" % eal.save_loaded_asset(
        material, only_if_is_dirty=False))
    say("  now: nanite=%s, automatic usage=%s" % (prop(material, "used_with_nanite", "?"),
                                              prop(material, "automatically_set_usage_in_editor", "?")))


for target in [p.strip() for p in os.environ.get("GTP_HEAD_MATERIAL", DEFAULT).split(",") if p.strip()]:
    fix(target)

report_dir = os.path.join(unreal.Paths.project_saved_dir(), "GTPPedestrians")
os.makedirs(report_dir, exist_ok=True)
with open(os.path.join(report_dir, "crowd-head-material.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(lines) + "\n")
say("report: %s" % os.path.join(report_dir, "crowd-head-material.txt"))
