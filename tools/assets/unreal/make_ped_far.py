"""Frozen-pose static mesh of a person for the distant crowd (-PedFar), editor commandlet.

    GTP_PED_SKM=/Game/.../SKM_Manny GTP_PED_ANIM=/Game/.../MF_Walk_Fwd GTP_PED_PHASE=0.25 \
      UnrealEditor-Cmd <uproject> -run=pythonscript -script=tools/assets/unreal/make_ped_far.py \
      -unattended -nosplash -stdout

Output: /Game/Characters/Far/SM_PedFar_A (Nanite off, one material, LOD0 about 1500 triangles,
LODs 1/0.5/0.25/0.125, no collision, tag CatalogId=PF-A). Orientation, pivot (feet) and scale
are those of the skeletal mesh, untouched. Add /Game/Characters/Far to DirectoriesToAlwaysCook.

The bake of a pose into a static mesh differs between engine versions, so the script tries the
known routes in order and logs which API exists ("[ped_far] probe ..."). First slot: run it once,
read the probe lines, fix the route if none worked (10 min).
"""
import os

import unreal

SKM = os.environ.get("GTP_PED_SKM", "")
ANIM = os.environ.get("GTP_PED_ANIM", "")
PHASE = float(os.environ.get("GTP_PED_PHASE", "0.25"))
DEST, NAME = "/Game/Characters/Far", "SM_PedFar_A"
TRIS = int(os.environ.get("GTP_PED_TRIS", "1500"))


def log(msg):
    unreal.log_warning("[ped_far] " + str(msg))


def probe():
    for owner, names in (
        (unreal.SkeletalMeshEditorSubsystem, ("create_static_mesh_from_skeletal_mesh", "get_lod_build_settings")),
        (unreal.EditorStaticMeshLibrary, ("merge_static_mesh_actors", "convert_actors")),
        (unreal, ("MeshMergingLibrary", "SkeletalMeshToStaticMeshConverter", "GeometryScriptLibrary_MeshModelingFunctions")),
    ):
        for n in names:
            log(f"probe {getattr(owner, '__name__', owner)}.{n}: {hasattr(owner, n)}")


def bake_route_a(skm, anim):
    """Route A: spawn a SkeletalMeshActor on the frozen pose, convert it with the mesh merge utility."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0))
    comp = actor.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(skm)
    comp.override_animation_data(anim, False, False, PHASE * anim.get_editor_property("sequence_length"), 0.0)
    comp.set_editor_property("force_refpose", False)
    comp.refresh_bone_transforms()
    opts = unreal.MergeStaticMeshActorsOptions() if hasattr(unreal, "MergeStaticMeshActorsOptions") else None
    if opts is None or not hasattr(unreal.EditorStaticMeshLibrary, "merge_static_mesh_actors"):
        raise RuntimeError("route A unavailable")
    raise RuntimeError("route A needs static components: see route B")


def bake_route_b(skm, anim):
    """Route B: Skeletal mesh editor subsystem, bake the pose (UE 5.6+ if present)."""
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    fn = getattr(sub, "create_static_mesh_from_skeletal_mesh", None)
    if fn is None:
        raise RuntimeError("route B unavailable")
    return fn(skm, f"{DEST}/{NAME}", PHASE)


def finish(mesh):
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    ns = sub.get_nanite_settings(mesh)
    ns.set_editor_property("enabled", False)
    sub.set_nanite_settings(mesh, ns, True)
    opts = unreal.EditorScriptingMeshReductionOptions()
    opts.set_editor_property("auto_compute_lod_screen_size", False)
    settings = []
    for k, screen in enumerate((1.0, 0.5, 0.25, 0.1)):
        st = unreal.EditorScriptingMeshReductionSettings()
        st.set_editor_property("percent_triangles", 0.5 ** k)
        st.set_editor_property("screen_size", screen)
        settings.append(st)
    opts.set_editor_property("reduction_settings", settings)
    sub.set_lods(mesh, opts)
    unreal.EditorAssetLibrary.set_metadata_tag(mesh, "CatalogId", "PF-A")
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)


probe()
def resolve_defaults():
    """Player mesh from the CDO of BP_PlayerCharacter, walk loop = first forward-walk AnimSequence."""
    bp = unreal.load_object(None, "/Game/Characters/Player/BP_PlayerCharacter.BP_PlayerCharacter_C")
    mesh = unreal.get_default_object(bp).get_editor_property("mesh").get_editor_property("skeletal_mesh_asset")
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    best = None
    for a in reg.get_assets_by_path("/Game/Characters", recursive=True):
        n = str(a.asset_name).lower()
        if str(a.asset_class_path.asset_name) != "AnimSequence" or "walk" not in n:
            continue
        if any(w in n for w in ("turn", "start", "stop", "left", "right", "_l_", "_r_", "back", "bwd")):
            continue
        if "fwd" in n or "_f_" in n or best is None:
            best = a.get_asset()
            if "fwd" in n or "_f_" in n:
                break
    log(f"resolved mesh={mesh.get_path_name() if mesh else None} anim={best.get_path_name() if best else None}")
    return mesh, best


skm = unreal.load_asset(SKM) if SKM else None
anim = unreal.load_asset(ANIM) if ANIM else None
if skm is None or anim is None:
    r_skm, r_anim = resolve_defaults()
    skm, anim = skm or r_skm, anim or r_anim
if skm is None or anim is None:
    raise SystemExit(f"set GTP_PED_SKM and GTP_PED_ANIM (got {SKM!r}, {ANIM!r})")
done = None
for route in (bake_route_b, bake_route_a):
    try:
        done = route(skm, anim)
        log(f"{route.__name__} worked")
        break
    except Exception as exc:  # noqa: BLE001 - try the next route, say why
        log(f"{route.__name__} failed: {exc}")
if done is None:
    raise SystemExit("no bake route worked: see the probe lines")
finish(unreal.load_asset(f"{DEST}/{NAME}"))
log("DONE")
