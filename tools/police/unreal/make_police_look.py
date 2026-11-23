"""Make what the police wear, inside the Unreal Editor (no UI): the car's livery and the officers' navy blue.

Run once per clone, before a cook (Epic's content is out of Git, so are the assets made here):
    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/police/unreal/make_police_look.py

Video of candidate 62: the police car is City Sample's saloon as Epic painted it, with our two lights on the roof, and
nothing says "police" on it. The officers wear the crowd's garments in their own colours.

1. /Game/Vehicles/Epic/GTP/Police/M_GTP_PoliceBand: a deferred decal, plain blue. AGTPPoliceDirector hangs one on
   each police car, projected across the car at door height: a blue band on both sides, on the body's own shape.
2. /Game/Vehicles/Epic/GTP/Police/MI_GTP_PolicePaint_<slot>: for each material slot of the police car's mesh that is
   body paint, a child of Epic's material with its colour parameters set to white. The director puts them on the car.
3. /Game/Crowd/GTP/Police/MI_GTP_Uniform_<garment>_<slot>: for each material slot of the officers' garments, a
   child of Epic's material with its colour parameters set to navy blue (black for the shoes). AGTPPoliceOfficer
   puts them on the garments.

The colour parameters of Epic's materials are not known in the repository. A parameter is taken when its name is one
of COLOUR_NAMES, or holds one of COLOUR_WORDS and none of NOT_COLOUR. When a material has none, nothing is made for
that slot and the game keeps Epic's material: read the report, add the name, run again. The report lists every slot,
its material and all its parameters: Saved/PoliceLook/police_look_report.txt. It also gives the bones from the head
to the root of the officers' body (the vest needs the name of the upper spine bone) and says whether the head and
the run loop named in DefaultGame.ini are there.

Assets that exist are made again from their parent (same path). Nothing is deleted, nothing of Epic's is changed.
GTP_POLICE_PAINT=R,G,B, GTP_POLICE_BAND=R,G,B and GTP_POLICE_NAVY=R,G,B change the colours (linear, 0 to 1).
"""

import os
import re
import traceback

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

CAR_DIR = "/Game/Vehicles/Epic/GTP/Police"
CAR_MESH = "/Game/Vehicles/Epic/GTP/Mesh/SKM_GTP_Police13"
UNIFORM_DIR = "/Game/Crowd/GTP/Police"
CROWD_MESHES = "/Game/Crowd/Character/Male/NormalWeight/Meshes"
BODY = "m_tal_nrw_body"
# The garments of [/Script/GranTheftPraline.GTPPoliceOfficer] in DefaultGame.ini, and the colour each one takes.
GARMENTS = {"m_tal_nrw_buttonDown": "navy", "m_tal_nrw_jeansLong_belt": "navy", "m_tal_nrw_oxfords": "black"}
FACE = "/Game/Crowd/Character/Male/m_002/Face/m_002_nrw_FaceMesh"
RUN_LOOP = "/Game/MetaHumans/GTP/AnimsCrowd/CS_M_Neutral_Run_Loop_F_R_20"

# Slots of the car that are body paint, by slot or material name.
PAINT_SLOT = re.compile(r"paint|body|exterior|carrosserie", re.IGNORECASE)
NOT_PAINT_SLOT = re.compile(r"glass|window|light|lamp|tire|tyre|wheel|interior|chrome|plastic|rubber|plate|siren|"
                            r"under|brake|caliper|mirror|grill|seat|trim|decal|damage", re.IGNORECASE)
# Vector parameters that are the colour of the surface.
COLOUR_NAMES = {"color", "colour", "basecolor", "base color", "basecolour", "base colour", "tint", "paint color",
                "paintcolor", "paint tint", "painttint", "body color", "bodycolor", "car color", "carcolor",
                "albedo tint", "albedotint", "diffuse color", "diffusecolor", "main color", "maincolor",
                "fabric color", "fabriccolor", "cloth color", "clothcolor", "primary color", "primarycolor",
                "color tint", "colortint", "base tint", "basetint"}
COLOUR_WORDS = re.compile(r"colou?r|tint|paint", re.IGNORECASE)
NOT_COLOUR = re.compile(r"dirt|dust|rust|flake|fresnel|emissive|specular|spec|sss|subsurface|fuzz|sheen|rim[ _]?light|scratch|"
                        r"wet|mask|bbox|vector|light|glow|damage|burn|mud|clear ?coat|second|makeup|lip|eye|skin|"
                        r"hair|shadow|ambient|occlusion|pattern", re.IGNORECASE)

lines = []


def say(text):
    for line in str(text).splitlines() or [""]:
        lines.append(line)
        unreal.log("POLICELOOK " + line)


def colour_from_env(name, default):
    text = os.environ.get(name, "").strip()
    try:
        r, g, b = (float(part) for part in text.split(","))
        return unreal.LinearColor(r, g, b, 1.0)
    except Exception:  # noqa: BLE001 (unset or malformed: the default)
        if text:
            say("%s='%s' is not R,G,B: default kept" % (name, text))
        return unreal.LinearColor(default[0], default[1], default[2], 1.0)


PAINT = colour_from_env("GTP_POLICE_PAINT", (0.82, 0.82, 0.82))
BAND = colour_from_env("GTP_POLICE_BAND", (0.0, 0.06, 0.42))
NAVY = colour_from_env("GTP_POLICE_NAVY", (0.010, 0.016, 0.050))
BLACK = unreal.LinearColor(0.008, 0.008, 0.008, 1.0)


def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def make_band():
    """A plain blue deferred decal (as tools/combat/unreal/make_blood_material.py makes the blood pool)."""
    path = CAR_DIR + "/M_GTP_PoliceBand"
    if eal.does_asset_exist(path):
        say("band: %s already there, left as it is (delete it to change its colour)" % path)
        return
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_GTP_PoliceBand", CAR_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        say("band: NOT made (the asset could not be created)")
        return
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    colour = node(material, unreal.MaterialExpressionConstant3Vector, -400, -200, constant=BAND)
    rough = node(material, unreal.MaterialExpressionConstant, -400, -80, r=0.45)
    opacity = node(material, unreal.MaterialExpressionConstant, -400, 20, r=1.0)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    say("band: %s made, saved=%s" % (path, eal.save_loaded_asset(material, False)))


def slots_of(mesh):
    """(index, slot name, material) for each material slot of a skeletal mesh."""
    found = []
    try:
        materials = mesh.get_editor_property("materials")
    except Exception as error:  # noqa: BLE001 (reported)
        say("  materials not read (%r)" % error)
        return found
    for index, slot in enumerate(materials):
        try:
            name = str(slot.get_editor_property("material_slot_name"))
            material = slot.get_editor_property("material_interface")
        except Exception as error:  # noqa: BLE001 (reported)
            say("  slot %d not read (%r)" % (index, error))
            continue
        found.append((index, name, material))
    return found


def parameters(material):
    """Names of the scalar, vector and texture parameters of a material, each list empty when it cannot be read."""
    out = {}
    for kind, names in (("scalar", mel.get_scalar_parameter_names), ("vector", mel.get_vector_parameter_names),
                        ("texture", mel.get_texture_parameter_names)):
        try:
            out[kind] = [str(name) for name in names(material)]
        except Exception as error:  # noqa: BLE001 (reported)
            say("    %s parameters not read (%r)" % (kind, error))
            out[kind] = []
    return out


def colour_parameters(vector_names):
    taken = []
    for name in vector_names:
        low = name.lower().replace("_", " ").strip()
        if low in COLOUR_NAMES or low.replace(" ", "") in COLOUR_NAMES:
            taken.append(name)
        elif COLOUR_WORDS.search(name) and not NOT_COLOUR.search(name):
            taken.append(name)
    return taken


def describe(index, name, material):
    if material is None:
        say("  slot %d '%s': no material" % (index, name))
        return {"scalar": [], "vector": [], "texture": []}
    parent = None
    try:
        parent = material.get_editor_property("parent")
    except Exception:  # noqa: BLE001 (a base material has no parent)
        pass
    say("  slot %d '%s': %s (%s, parent %s)" % (index, name, material.get_path_name(), material.get_class().get_name(),
                                               parent.get_path_name() if parent else "-"))
    found = parameters(material)
    for kind in ("vector", "scalar", "texture"):
        say("    %s: %s" % (kind, ", ".join(found[kind]) or "-"))
    return found


def make_instance(folder, name, parent, names, colour):
    """A child of `parent` at folder/name with the vector parameters `names` set to `colour`."""
    path = folder + "/" + name
    instance = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    made = "made again"
    if instance is None:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        made = "made"
    if instance is None:
        say("    %s NOT made (the asset could not be created)" % path)
        return False
    mel.clear_all_material_instance_parameters(instance)
    mel.set_material_instance_parent(instance, parent)
    for parameter in names:
        mel.set_material_instance_vector_parameter_value(instance, parameter, colour)
    mel.update_material_instance(instance)
    say("    %s %s: %s = (%.3f, %.3f, %.3f), saved=%s" % (path, made, ", ".join(names), colour.r, colour.g, colour.b,
                                                         eal.save_loaded_asset(instance, False)))
    return True


def make_paint():
    say("== police car paint (%s)" % CAR_MESH)
    mesh = unreal.load_asset(CAR_MESH) if eal.does_asset_exist(CAR_MESH) else None
    if mesh is None:
        say("  the mesh is not there (tools/vehicles/unreal/make_epic_vehicles.py makes it): no paint made")
        return
    made = 0
    for index, name, material in slots_of(mesh):
        found = describe(index, name, material)
        if material is None:
            continue
        label = "%s %s" % (name, material.get_name())
        if not PAINT_SLOT.search(label) or NOT_PAINT_SLOT.search(label):
            say("    not taken for body paint")
            continue
        names = colour_parameters(found["vector"])
        if not names:
            say("    body paint, but none of its vector parameters reads as its colour: NOTHING MADE for this slot")
            continue
        made += 1 if make_instance(CAR_DIR, "MI_GTP_PolicePaint_%d" % index, material, names, PAINT) else 0
    say("  %d paint instance(s) made" % made)


def make_uniform():
    say("== officers' garments")
    for garment, shade in GARMENTS.items():
        path = "%s/%s" % (CROWD_MESHES, garment)
        say(" %s" % path)
        mesh = unreal.load_asset(path) if eal.does_asset_exist(path) else None
        if mesh is None:
            say("  not there: the officer goes without it")
            continue
        for index, name, material in slots_of(mesh):
            found = describe(index, name, material)
            if material is None:
                continue
            names = colour_parameters(found["vector"])
            if not names:
                say("    none of its vector parameters reads as its colour: NOTHING MADE for this slot")
                continue
            make_instance(UNIFORM_DIR, "MI_GTP_Uniform_%s_%d" % (garment, index), material, names,
                          NAVY if shade == "navy" else BLACK)


def report_body():
    say("== officers' body")
    for label, path in (("body", "%s/%s" % (CROWD_MESHES, BODY)), ("head", FACE), ("run loop", RUN_LOOP)):
        say("  %s %s: %s" % (label, path, "there" if eal.does_asset_exist(path) else "NOT THERE"))
    try:
        heads = sorted(set(path.rsplit(".", 1)[-1] for path in eal.list_assets("/Game/Crowd/Character/Male", recursive=True)
                           if path.rsplit(".", 1)[-1].endswith("_nrw_FaceMesh")))
        say("  heads of the pack: %s" % (", ".join(heads) or "none"))
    except Exception as error:  # noqa: BLE001 (reported)
        say("  heads not listed (%r)" % error)
    try:
        loops = sorted(path.rsplit(".", 1)[-1] for path in eal.list_assets("/Game/MetaHumans/GTP/AnimsCrowd", recursive=False))
        say("  loops baked on the body's skeleton: %s" % (", ".join(loops) or "none"))
    except Exception as error:  # noqa: BLE001 (reported)
        say("  loops not listed (%r)" % error)
    # The bones from the head down to the root, read through an animation of that skeleton.
    try:
        loops = [path for path in eal.list_assets("/Game/MetaHumans/GTP/AnimsCrowd", recursive=False)]
        animation = None
        for path in loops:
            animation = unreal.load_asset(path.rsplit(".", 1)[0])
            if isinstance(animation, unreal.AnimSequence):
                break
            animation = None
        if animation is None:
            say("  bones: no animation of the body's skeleton to read them through")
        else:
            chain = unreal.AnimationLibrary.find_bone_path_to_root(animation, "head")
            say("  bones from head to root (through %s): %s" % (animation.get_name(), ", ".join(str(bone) for bone in chain) or "none"))
    except Exception as error:  # noqa: BLE001 (reported)
        say("  bones not read (%r)" % error)


def main():
    for step in (make_band, make_paint, make_uniform, report_body):
        try:
            step()
        except Exception:  # noqa: BLE001 (one step failing must not hide the others' report)
            say("FAILED in %s:\n%s" % (step.__name__, traceback.format_exc()))
    folder = os.path.join(unreal.Paths.project_saved_dir(), "PoliceLook")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, "police_look_report.txt"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    say("report written to Saved/PoliceLook/police_look_report.txt")


main()
