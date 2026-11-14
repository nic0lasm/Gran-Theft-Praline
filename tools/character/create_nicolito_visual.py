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
    return eal.load_asset(path) if eal.does_asset_exist(path) else None


def object_path(package):
    return "%s.%s" % (package, package.rsplit("/", 1)[-1])


def set_asset(bp, component, prop, asset):
    report = unreal.GTPAnimGraphTools.set_blueprint_component_asset(bp, component, prop, asset)
    say("  " + report)
    return not report.startswith("error")


def rebind(bp, component, fields, face, head, kellan_face_path):
    """Puts Kellan's groom of that component back with a binding to the new face (MetaHuman faces share one
    topology: the binding transfers the groom from the face it was made on). A failure leaves it empty."""
    groom_path = fields.get("GroomAsset", "-")
    if groom_path == "-":
        say("  %s: no groom in BP_Kellan" % component)
        return
    library = getattr(unreal, "GroomBlueprintLibrary", None)
    create = getattr(library, "create_new_groom_binding_asset_with_path", None) if library else None
    if create is None:
        say("  %s NOT BOUND: unreal.GroomBlueprintLibrary.create_new_groom_binding_asset_with_path missing, left empty" % component)
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


def main():
    head = os.environ.get("GTP_NICOLITO_HEAD", "").strip()
    mode = os.environ.get("GTP_NICOLITO_MODE", "face").strip().lower() or "face"
    if not re.fullmatch(r"m_\d{3}", head) or mode not in ("face", "full"):
        say("BLOCKED: GTP_NICOLITO_HEAD must name a head like m_004 (got '%s'), GTP_NICOLITO_MODE face or full (got '%s')" % (head, mode))
        return
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
        same = body_skeleton != "-" and body_skeleton == body_anim_skeleton
        say("  body skeleton %s, the body's retarget blueprint made for %s: %s" % (
            body_skeleton, body_anim_skeleton, "same" if same else "DIFFERENT, the body would stay in its reference pose"))
        ok &= same
        for component, garment in OUTFIT.items():
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
    for fields in components(bp).values():
        say("  " + " ".join("%s=%s" % item for item in fields.items()))
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
