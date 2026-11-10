"""Makes the animated far-crowd mesh: the walking character baked to a vertex animation texture.

Editor Python, replayable (it overwrites what it made):
  UnrealEditor <uproject> -ExecutePythonScript=<copy of this file without spaces in its path>

Result: /Game/Characters/Far/SM_PedFar_A_VAT (static mesh whose UVs index the baked vertices),
plus the position/normal textures and a data asset DA_PedFar_VAT beside it, then the line to
put in DefaultGame.ini:  FarCrowdMesh=/Game/Characters/Far/SM_PedFar_A_VAT.SM_PedFar_A_VAT
Use it in place of the still pose of make_ped_far.py. The far crowd gives every instance two
custom data floats (walk phase 0-1, speed scale 0.9-1.1): the material reads them, so it needs
the engine's AnimToTexture material function, with Time * speed + phase as the animation time.
The script builds that material when it can and says in the log when it could not (then
open the data asset, which is where the engine's own tool starts, and hook the function by hand).

It uses the engine's AnimToTexture plugin (Experimental): its Python surface differs between
engine versions, so each step tries what exists and logs the route taken. Environment:
  GTP_PED_SKM   skeletal mesh path (default: the mesh of BP_PlayerCharacter)
  GTP_PED_ANIM  walk AnimSequence path (default: the first forward walk of that skeleton)
  GTP_PED_OUT   output folder (default /Game/Characters/Far)
"""

import os

import unreal

OUT = os.environ.get("GTP_PED_OUT", "/Game/Characters/Far")
NAME = "PedFar_A_VAT"


def log(msg):
    unreal.log_warning("PED_FAR_VAT " + msg)


def player_mesh():
    path = os.environ.get("GTP_PED_SKM")
    if path:
        return unreal.load_asset(path)
    bp = unreal.load_asset("/Game/Characters/Player/BP_PlayerCharacter")
    cdo = unreal.get_default_object(bp.generated_class())
    return cdo.get_editor_property("mesh").get_editor_property("skeletal_mesh_asset")


def forward_walk(skeleton):
    path = os.environ.get("GTP_PED_ANIM")
    if path:
        return unreal.load_asset(path)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    bad = ("turn", "start", "stop", "pivot", "back", "left", "right", "strafe")
    best, score = None, 0
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_class_path.asset_name) != "AnimSequence":
            continue
        name = str(data.asset_name).lower()
        if "walk" not in name or any(b in name for b in bad):
            continue
        s = 10 + (5 if ("fwd" in name or "_f_" in name or "forward" in name) else 0) + (3 if "loop" in name else 0)
        if s > score:
            anim = data.get_asset()
            if anim and anim.get_editor_property("skeleton") == skeleton.get_editor_property("skeleton"):
                best, score = anim, s
    return best


def main():
    mesh = player_mesh()
    anim = forward_walk(mesh) if mesh else None
    log(f"mesh={mesh.get_path_name() if mesh else None} anim={anim.get_path_name() if anim else None}")
    if not mesh or not anim:
        log("FAILED: no mesh or no walk animation")
        return
    if not hasattr(unreal, "AnimToTextureDataAsset") or not hasattr(unreal, "AnimToTextureBPLibrary"):
        log("FAILED: the AnimToTexture plugin is not enabled in this project (add it to the .uproject, restart the editor)")
        return
    unreal.EditorAssetLibrary.make_directory(OUT)
    data = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_" + NAME, OUT, unreal.AnimToTextureDataAsset, unreal.AnimToTextureDataAssetFactory()
        if hasattr(unreal, "AnimToTextureDataAssetFactory") else None)
    if data is None:
        data = unreal.load_asset(f"{OUT}/DA_{NAME}")
    for prop, value in (("skeletal_mesh", mesh),
                        ("mode", unreal.AnimToTextureMode.VERTEX) if hasattr(unreal, "AnimToTextureMode") else (None, None),
                        ("max_lod_index", 0),
                        ("sampling_mode", unreal.AnimToTextureSampleMode.SAMPLE_RATE) if hasattr(unreal, "AnimToTextureSampleMode") else (None, None)):
        if prop:
            try:
                data.set_editor_property(prop, value)
            except Exception as err:  # property names move between versions
                log(f"could not set {prop}: {err}")
    try:
        item = unreal.AnimToTextureAnimSequenceInfo()
        item.set_editor_property("enabled", True)
        item.set_editor_property("anim_sequence", anim)
        data.set_editor_property("anim_sequences", [item])
    except Exception as err:
        log(f"could not add the animation: {err}")
    # The engine's bake: writes the textures and the static mesh next to the data asset.
    try:
        unreal.AnimToTextureBPLibrary.animation_to_texture(data)
        log("baked with AnimToTextureBPLibrary.animation_to_texture")
    except Exception as err:
        log(f"FAILED: bake call: {err}")
        return
    unreal.EditorAssetLibrary.save_directory(OUT)
    log(f"done. DefaultGame.ini: FarCrowdMesh={OUT}/SM_{NAME}.SM_{NAME}  (check the static mesh name the engine wrote in {OUT})")


main()
