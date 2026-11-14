"""Builds Nicolito's visual from the City Sample crowd head the project lead chose (docs/gameplay/player-character.md,
"Nicolito's face").

The visual is a copy of the Game Animation Sample's BP_Kellan, the MetaHuman already posed at run time from the
pawn's hidden mannequin (retarget, weapon clips, grooms handling): only its meshes change, BP_Kellan itself is never
touched. Run in the GTAL editor (the game module must be built: it calls UGTPAnimGraphTools):

    GTP_NICOLITO_HEAD=m_004 UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript
        -script=tools/character/create_nicolito_visual.py -unattended -nosplash

Env:
  GTP_NICOLITO_HEAD  the chosen head, as named in the portraits (Nicolito_m_004.png -> m_004). Required.
  GTP_NICOLITO_MODE  face (default): the head changes, Kellan's body and outfit stay -> BP_Nicolito.
                     full: also the crowd's normal-weight male body and outfit (crewneck, jeans, loafers)
                     -> BP_Nicolito_Full.
  GTP_NICOLITO_GROOMS  Kellan's grooms put back with a new binding to the chosen face, comma-separated component
                     names (default: Eyebrows; e.g. Eyebrows,Hair,Mustache). The others stay empty.
  GTP_NICOLITO_SKIN  the crowd skin atlas tile (0 to 11) of the face and the body. Default: the chosen head's own
                     tile (m_004 reads 3), the skin seen on the portraits.
  GTP_NICOLITO_OUTFIT  full mode only: the crowd garments, e.g. Torso=crewneck,Legs=jeans,Feet=loafers (the default).
The crowd's synthesized skin takes its colour from one tile of an atlas (FaceColorAtlas, BodyColorAtlas, 4x4 tiles,
12 used) chosen by the material parameter AtlasSelector; City Sample gives the head and the body the same tile
(CrowdBlueprintLibrary, SkinTextureDefinition.TextureAtlasIndex). Each head's own material reads its own tile, the
body's shared material reads tile 0: every face and body slot that reads another tile than GTP_NICOLITO_SKIN gets a
material instance of its own material reading that tile (/Game/MetaHumans/GTP/Nicolito/Materials, nothing else
changes; the crowd's materials are not touched).
Both are under /Game/MetaHumans/GTP/Nicolito (a cooked folder). Kellan's grooms are bound to his own face: they are
emptied (the game mode hides a groom without an asset, without tick), then those of GTP_NICOLITO_GROOMS get a binding
to the new face (/Game/MetaHumans/GTP/Nicolito/Grooms, through the HairStrands GroomBlueprintLibrary, transferred
from the face the groom was made on: MetaHuman faces share one topology). Without his hair Nicolito is bald until
his hair is chosen.
The face keeps Kellan's face animation only when the new head has the skeleton it was made for; otherwise the face
follows the body's head and neck (the game mode does it), without facial animation.

Report: Saved/GTPCombat/nicolito-visual.txt (lines also in the log, prefixed "NICOLITO"): Kellan's components, every
change, the skeleton checks, the components after. Try it without touching the config:
    -PlayerVisual=/Game/MetaHumans/GTP/Nicolito/BP_Nicolito.BP_Nicolito_C
then PlayerVisualClass in DefaultGame.ini once a capture shows it right. The Mac's copy of the project needs the same
run (Epic's and City Sample's content is out of Git).
"""
import os
import re

import unreal

KELLAN = "/Game/MetaHumans/Kellan/BP_Kellan"
OUT_DIR = "/Game/MetaHumans/GTP/Nicolito"
CROWD_MALE = "/Game/Crowd/Character/Male"
OUTFIT = {"Torso": "crewneck", "Legs": "jeans", "Feet": "loafers"}
GROOM_DIR = OUT_DIR + "/Grooms"
MATERIAL_DIR = OUT_DIR + "/Materials"
SKIN_PARAMETER = "AtlasSelector"
SKIN_TILES = 12

eal = unreal.EditorAssetLibrary
lines = []


def say(text):
    for line in str(text).splitlines() or [""]:
        lines.append(line)
        unreal.log("NICOLITO " + line)


def components(bp):
    """Component name -> dict of the described fields (UGTPAnimGraphTools::DescribeBlueprintComponents)."""
    found = {}
    for line in unreal.GTPAnimGraphTools.describe_blueprint_components(bp).splitlines():
        fields = dict(token.split("=", 1) for token in line.split() if "=" in token)
        if "name" in fields:
            found[fields["name"]] = fields
    return found


def load_mesh(path):
    return eal.load_asset(path) if path not in ("", "-") and eal.does_asset_exist(path) else None


def mesh_skeleton(mesh):
    for read in (lambda: mesh.skeleton, lambda: mesh.get_editor_property("skeleton")):
        try:
            return read()
        except Exception:  # noqa: BLE001 (the other way)
            pass
    return None


def object_path(package):
    return "%s.%s" % (package, package.rsplit("/", 1)[-1])


def set_asset(bp, component, prop, asset):
    report = unreal.GTPAnimGraphTools.set_blueprint_component_asset(bp, component, prop, asset)
    say("  " + report)
    return not report.startswith("error")


def dump_material(label, path):
    """The skin's parameters (names and values), to match the body's skin to the face's."""
    if path in ("-", ""):
        return
    material = unreal.load_asset(path)
    if material is None:
        say("  %s: %s not loaded" % (label, path))
        return
    mel = unreal.MaterialEditingLibrary
    instance = isinstance(material, unreal.MaterialInstance)
    parent = material.get_editor_property("parent") if instance else None
    say("  %s: %s (%s, parent %s)" % (label, path, material.get_class().get_name(), parent.get_path_name() if parent else "-"))
    for kind, names, read in (
            ("scalar", mel.get_scalar_parameter_names, "scalar"),
            ("vector", mel.get_vector_parameter_names, "vector"),
            ("texture", mel.get_texture_parameter_names, "texture")):
        try:
            found = names(material)
        except Exception as error:  # noqa: BLE001 (reported)
            say("    %s parameters not read (%r)" % (kind, error))
            continue
        values = []
        for name in found:
            value = "?"
            try:
                if instance:
                    value = getattr(mel, "get_material_instance_%s_parameter_value" % read)(material, name)
                else:
                    value = getattr(mel, "get_material_default_%s_parameter_value" % read)(material, name)
            except Exception:  # noqa: BLE001 (value not readable this way)
                pass
            if hasattr(value, "get_path_name"):
                value = value.get_path_name()
            values.append("%s=%s" % (name, value))
        say("    %s: %s" % (kind, "; ".join(values) or "-"))


def rebind(bp, component, fields, face, head, kellan_face_path):
    """Puts Kellan's groom of that component back with a binding to the new face (MetaHuman faces share one
    topology: the binding transfers the groom from the face it was made on). A failure leaves it empty."""
    groom_path = fields.get("GroomAsset", "-")
    if groom_path == "-":
        say("  %s: no groom in BP_Kellan" % component)
        return
    # UGroomBlueprintLibrary is exposed to Python as GroomLibrary (its ScriptName) on 5.8.
    create = None
    for library_name in ("GroomLibrary", "GroomBlueprintLibrary"):
        library = getattr(unreal, library_name, None)
        create = getattr(library, "create_new_groom_binding_asset_with_path", None) if library else None
        if create is not None:
            break
    if create is None:
        say("  %s NOT BOUND: create_new_groom_binding_asset_with_path missing from unreal.GroomLibrary and unreal.GroomBlueprintLibrary, left empty" % component)
        return
    groom = unreal.load_asset(groom_path)
    old_binding = unreal.load_asset(fields["BindingAsset"]) if fields.get("BindingAsset", "-") != "-" else None
    source_mesh = None
    for prop in ("target_skeletal_mesh", "source_skeletal_mesh"):
        if source_mesh is None and old_binding is not None:
            try:
                source_mesh = old_binding.get_editor_property(prop)
            except Exception:  # noqa: BLE001 (not exposed on this engine)
                source_mesh = None
    if source_mesh is None and kellan_face_path != "-":
        source_mesh = unreal.load_asset(kellan_face_path)
    package = "%s/%s_%s_Binding" % (GROOM_DIR, groom.get_name(), head)
    if eal.does_asset_exist(package):
        binding = eal.load_asset(package)
        say("  %s: binding %s reused" % (component, package))
    else:
        try:
            binding = create(package, groom, face, 100, source_mesh, 0)
        except Exception as error:  # noqa: BLE001 (reported, the groom stays empty)
            say("  %s NOT BOUND: binding not created (%r), left empty" % (component, error))
            return
        if binding is None:
            say("  %s NOT BOUND: no binding returned, left empty" % component)
            return
        say("  %s: binding %s created (groom %s, on %s, transferred from %s), saved %s" % (
            component, package, groom.get_name(), face.get_name(), source_mesh.get_name() if source_mesh else "nothing",
            eal.save_loaded_asset(binding, False)))
    set_asset(bp, component, "GroomAsset", groom)
    set_asset(bp, component, "BindingAsset", binding)


def slots(fields):
    """(slot index, slot name, material path) of a described component's mesh, in slot order."""
    mats = fields.get("mats", "-")
    if mats in ("-", ""):
        return []
    found = []
    for index, entry in enumerate(mats.split(";")):
        name, _, path = entry.partition(":")
        found.append((index, name, path))
    return found


def atlas_tile(material):
    """The skin atlas tile a crowd skin material instance reads, None for any other material."""
    if not isinstance(material, unreal.MaterialInstance):
        return None
    mel = unreal.MaterialEditingLibrary
    try:
        if SKIN_PARAMETER not in [str(name) for name in mel.get_scalar_parameter_names(material)]:
            return None
        return int(round(mel.get_material_instance_scalar_parameter_value(material, SKIN_PARAMETER)))
    except Exception:  # noqa: BLE001 (not a skin material)
        return None


def head_tile(fields):
    """The tile the chosen head's own skin material reads (its first slot named head..., else the first skin slot)."""
    tiles = []
    for _, name, path in slots(fields):
        material = unreal.load_asset(path) if path not in ("-", "") else None
        tile = atlas_tile(material) if material else None
        if tile is not None:
            tiles.append((not name.lower().startswith("head_shader"), tile))
    return sorted(tiles)[0][1] if tiles else None


def skin_instance(source, tile, tag):
    """A material instance of `source` (a crowd skin instance) that only changes the atlas tile it reads."""
    name = "MI_Nicolito_%s_%s_Atlas%d" % (tag, source.get_name(), tile)
    package = "%s/%s" % (MATERIAL_DIR, name)
    if eal.does_asset_exist(package):
        instance, made = eal.load_asset(package), "reused"
    else:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        made = "created"
    if instance is None:
        return None, "not created", False
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_parent(instance, source)
    mel.set_material_instance_scalar_parameter_value(instance, SKIN_PARAMETER, float(tile))
    mel.update_material_instance(instance)
    return instance, made, eal.save_loaded_asset(instance, False)


def component_template(bp, name):
    """The blueprint's own template of component `name` (what the construction script spawns)."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        if str(lib.get_variable_name(data)) != name:
            continue
        getter = getattr(lib, "get_object_for_blueprint", None)
        template = getter(data, bp) if getter else lib.get_object(data)
        if template is not None:
            return template
    return None


def set_skin(bp, component, fields, tile, tag):
    """Every slot of `component` whose skin material reads another tile than `tile` gets an instance reading it."""
    template = None
    for slot, slot_name, path in slots(fields):
        material = unreal.load_asset(path) if path not in ("-", "") else None
        current = atlas_tile(material) if material else None
        if current is None:
            continue
        if current == tile:
            say("  %s slot %d %s: %s already reads tile %d" % (component, slot, slot_name, material.get_name(), tile))
            continue
        if template is None:
            template = component_template(bp, component)
            if template is None:
                say("  %s: its template was not found (SubobjectDataSubsystem), skin NOT changed" % component)
                return False
        instance, made, saved = skin_instance(material, tile, tag)
        if instance is None:
            say("  %s slot %d %s: instance of %s NOT created, skin NOT changed" % (component, slot, slot_name, material.get_name()))
            return False
        overrides = list(template.get_editor_property("override_materials"))
        while len(overrides) <= slot:
            overrides.append(None)
        overrides[slot] = instance
        try:
            template.set_editor_property("override_materials", overrides)
        except Exception as error:  # noqa: BLE001 (the component's own call instead)
            say("  %s: override_materials not set as a property (%r), set_material instead" % (component, error))
            template.set_material(slot, instance)
        say("  %s slot %d %s: %s read tile %d, now %s (%s, saved %s) reads tile %d" % (
            component, slot, slot_name, material.get_name(), current, instance.get_name(), made, saved, tile))
    if template is not None:
        after = template.get_editor_property("override_materials")
        say("  %s overrides now: %s" % (component, "; ".join(
            "%d:%s" % (index, material.get_name() if material else "-") for index, material in enumerate(after)) or "none"))
    return True


def bone_names(skeleton):
    """The skeleton's bone names (its reference pose), an empty set when not readable."""
    try:
        pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
        return set(str(name) for name in unreal.AnimPoseExtensions.get_bone_names(pose))
    except Exception as error:  # noqa: BLE001 (reported by the caller as "not read")
        say("    bones of %s not read (%r)" % (skeleton.get_name() if skeleton else "none", error))
        return set()


def describe_rig(label, mesh, against, against_label):
    """What could keep a part from following the body: bones the body lacks, cloth, a post-process blueprint."""
    if mesh is None:
        return
    skeleton = mesh_skeleton(mesh)
    own = bone_names(skeleton) if skeleton else set()
    missing = sorted(own - against) if own and against else []
    details = []
    for prop in ("mesh_clothing_assets", "post_process_anim_blueprint"):
        try:
            value = mesh.get_editor_property(prop)
        except Exception:  # noqa: BLE001 (not exposed on this engine)
            value = "?"
        if prop == "mesh_clothing_assets" and value != "?":
            value = "%d (%s)" % (len(value), ", ".join(item.get_name() for item in value if item) or "-")
        elif hasattr(value, "get_name"):
            value = value.get_name()
        details.append("%s=%s" % (prop, value or "-"))
    say("  %s %s: skeleton %s, %d bones, %d not in %s%s; %s" % (
        label, mesh.get_name(), skeleton.get_name() if skeleton else "none", len(own), len(missing), against_label,
        (": " + ", ".join(missing[:60]) + (" ..." if len(missing) > 60 else "")) if missing else "",
        "; ".join(details)))


def main():
    head = os.environ.get("GTP_NICOLITO_HEAD", "").strip()
    mode = os.environ.get("GTP_NICOLITO_MODE", "face").strip().lower() or "face"
    if not re.fullmatch(r"m_\d{3}", head) or mode not in ("face", "full"):
        say("BLOCKED: GTP_NICOLITO_HEAD must name a head like m_004 (got '%s'), GTP_NICOLITO_MODE face or full (got '%s')" % (head, mode))
        return
    skin = os.environ.get("GTP_NICOLITO_SKIN", "").strip()
    if skin and (not skin.isdigit() or int(skin) >= SKIN_TILES):
        say("BLOCKED: GTP_NICOLITO_SKIN must be an atlas tile from 0 to %d (got '%s')" % (SKIN_TILES - 1, skin))
        return
    outfit = dict(OUTFIT)
    for item in os.environ.get("GTP_NICOLITO_OUTFIT", "").split(","):
        if item.strip():
            component, _, garment = item.partition("=")
            if component.strip() not in OUTFIT or not re.fullmatch(r"[a-z0-9_]+", garment.strip()):
                say("BLOCKED: GTP_NICOLITO_OUTFIT takes Torso=, Legs=, Feet= with a crowd garment name (got '%s')" % item.strip())
                return
            outfit[component.strip()] = garment.strip()
    if not hasattr(unreal, "GTPAnimGraphTools") or not hasattr(unreal.GTPAnimGraphTools, "describe_blueprint_components"):
        say("BLOCKED: unreal.GTPAnimGraphTools.describe_blueprint_components missing (build the game module first)")
        return
    face_path = "%s/%s/Face/%s_nrw_FaceMesh" % (CROWD_MALE, head, head)
    face = load_mesh(face_path)
    kellan = eal.load_asset(KELLAN) if eal.does_asset_exist(KELLAN) else None
    if not face or not kellan:
        say("BLOCKED: head %s %s, %s %s" % (face_path, "found" if face else "MISSING", KELLAN, "found" if kellan else "MISSING"))
        return
    say("== Kellan's components (%s)" % KELLAN)
    source = components(kellan)
    for fields in source.values():
        say("  " + " ".join("%s=%s" % item for item in fields.items()))

    name = "BP_Nicolito" if mode == "face" else "BP_Nicolito_Full"
    out = "%s/%s" % (OUT_DIR, name)
    if eal.does_asset_exist(out):
        say("== reusing %s (the same changes are applied again)" % out)
    elif not eal.duplicate_asset(KELLAN, out):
        say("BLOCKED: %s not copied to %s" % (KELLAN, out))
        return
    else:
        say("== %s copied from %s" % (out, KELLAN))
    bp = eal.load_asset(out)
    ok = True

    say("== head %s (%s mode)" % (head, mode))
    kellan_face = source.get("Face", {})
    if not kellan_face:
        say("BLOCKED: BP_Kellan has no component named Face")
        return
    ok &= set_asset(bp, "Face", "SkeletalMeshAsset", face)
    face_skeleton_path = components(bp).get("Face", {}).get("skeleton", "-")
    # Kellan's own skin materials, if the blueprint overrides them, must not land on the new face.
    ok &= set_asset(bp, "Face", "OverrideMaterials", None)
    anim_skeleton = kellan_face.get("animSkeleton", "-")
    if anim_skeleton != "-" and anim_skeleton != face_skeleton_path:
        say("  face skeleton %s, Kellan's face animation made for %s: different, the face follows the body instead" % (face_skeleton_path, anim_skeleton))
        ok &= set_asset(bp, "Face", "AnimClass", None)
    else:
        say("  face skeleton %s, Kellan's face animation made for %s: kept" % (face_skeleton_path, anim_skeleton))
        anim = kellan_face.get("anim", "-")
        if anim != "-":
            ok &= set_asset(bp, "Face", "AnimClass", unreal.load_class(None, anim))

    say("== body and outfit")
    if mode == "full":
        body = load_mesh("%s/NormalWeight/Meshes/m_tal_nrw_body" % CROWD_MALE)
        if not body:
            say("BLOCKED: no m_tal_nrw_body under %s/NormalWeight/Meshes" % CROWD_MALE)
            return
        ok &= set_asset(bp, "Body", "SkeletalMeshAsset", body)
        ok &= set_asset(bp, "Body", "OverrideMaterials", None)
        body_skeleton = components(bp).get("Body", {}).get("skeleton", "-")
        body_anim_skeleton = source.get("Body", {}).get("animSkeleton", "-")
        # "-": a template blueprint (ABP_GenericRetarget, the sample's), made for no skeleton, runs on any.
        same = body_skeleton != "-" and body_anim_skeleton in (body_skeleton, "-")
        say("  body skeleton %s, the body's retarget blueprint made for %s: %s" % (
            body_skeleton, "any skeleton (template)" if body_anim_skeleton == "-" else body_anim_skeleton,
            "fits" if same else "DIFFERENT, the body would stay in its reference pose"))
        ok &= same
        for component, garment in outfit.items():
            if component not in source:
                say("  %s: no such component in BP_Kellan, %s not worn" % (component, garment))
                continue
            mesh = load_mesh("%s/NormalWeight/Meshes/m_tal_nrw_%s" % (CROWD_MALE, garment))
            if not mesh:
                say("  %s: m_tal_nrw_%s missing, component emptied" % (component, garment))
            ok &= set_asset(bp, component, "SkeletalMeshAsset", mesh)
            ok &= set_asset(bp, component, "OverrideMaterials", None)
    else:
        say("  Kellan's body and outfit kept")

    say("== skin tone (crowd skin atlas tile)")
    current = components(bp)
    own_tile = head_tile(current.get("Face", {}))
    if own_tile is None:
        say("  the head's materials read no atlas tile: skin left as it is")
    else:
        tile = int(skin) if skin else own_tile
        say("  %s's own material reads tile %d; face%s set to tile %d%s" % (
            head, own_tile, " and body" if mode == "full" else "", tile, " (GTP_NICOLITO_SKIN)" if skin else " (the head's own)"))
        ok &= set_skin(bp, "Face", current.get("Face", {}), tile, head)
        if mode == "full":
            ok &= set_skin(bp, "Body", current.get("Body", {}), tile, "body")
        else:
            say("  face mode: Kellan's body keeps its own skin (not an atlas)")

    if mode == "full":
        try:
            say("== what could keep a part from following the body (leader pose by bone name, runtime retarget)")
            body_mesh = load_mesh(components(bp).get("Body", {}).get("mesh", "-"))
            body_skeleton = mesh_skeleton(body_mesh) if body_mesh else None
            body_bones = bone_names(body_skeleton) if body_skeleton else set()
            say("  crowd body skeleton %s: %d bones" % (body_skeleton.get_name() if body_skeleton else "not read", len(body_bones)))
            kellan_body = load_mesh(source.get("Body", {}).get("mesh", "-"))
            if kellan_body:
                # The retarget was set up on Kellan's body: bones of his skeleton the crowd body lacks can stop it.
                describe_rig("Kellan's body", kellan_body, body_bones, "the crowd body's skeleton")
            for component in outfit:
                describe_rig(component, load_mesh(components(bp).get(component, {}).get("mesh", "-")), body_bones, "the crowd body's skeleton")
            folder = "%s/NormalWeight/Meshes" % CROWD_MALE
            garments = sorted(set(path.rsplit(".", 1)[-1] for path in eal.list_assets(folder, recursive=False)
                                  if path.rsplit(".", 1)[-1].startswith("m_tal_nrw_")))
            say("  crowd garments for this body (%s): %s" % (folder, ", ".join(name[len("m_tal_nrw_"):] for name in garments) or "none"))
        except Exception as error:  # noqa: BLE001 (a diagnosis only: never blocks the save)
            say("  diagnosis stopped (%r)" % (error,))

    say("== grooms (bound to Kellan's face)")
    for component, fields in source.items():
        if "Groom" in fields.get("class", ""):
            ok &= set_asset(bp, component, "GroomAsset", None)
            if "BindingAsset" in fields:
                ok &= set_asset(bp, component, "BindingAsset", None)
    kept = [name.strip() for name in os.environ.get("GTP_NICOLITO_GROOMS", "Eyebrows").split(",") if name.strip()]
    say("== Kellan's grooms bound again, to the new face: %s" % (", ".join(kept) or "none"))
    for component in kept:
        rebind(bp, component, source.get(component, {}), face, head, source.get("Face", {}).get("mesh", "-"))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    say("== %s after the changes" % out)
    after = components(bp)
    for fields in after.values():
        say("  " + " ".join("%s=%s" % item for item in fields.items()))
    say("== skin: the face's and the body's first materials (to match the hands and neck to the face)")
    for component in ("Face", "Body"):
        mats = after.get(component, {}).get("mats", "-")
        first = mats.split(";")[0].split(":", 1)[-1] if mats != "-" else "-"
        dump_material(component, first)
    kellan_body = source.get("Body", {}).get("mats", "-")
    if mode == "full" and kellan_body != "-":
        dump_material("Kellan's body", kellan_body.split(";")[0].split(":", 1)[-1])
    if not ok:
        say("NOT SAVED: a step failed or a skeleton differs (above); close the editor without saving")
        return
    saved = eal.save_loaded_asset(bp, False)
    say("saved %s: %s" % (out, saved))
    say("try: -PlayerVisual=%s_C" % object_path(out))


def run():
    try:
        main()
    except Exception as error:  # noqa: BLE001 (reported, the report is still written)
        say("BLOCKED: %r" % (error,))
    folder = os.path.join(unreal.Paths.project_saved_dir(), "GTPCombat")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "nicolito-visual.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/GTPCombat/nicolito-visual.txt")


run()
