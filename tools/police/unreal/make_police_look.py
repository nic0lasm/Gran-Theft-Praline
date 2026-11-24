"""Make what the police wear, inside the Unreal Editor (no UI): the car's livery and the officers' navy blue.

Run once per clone, before a cook (Epic's content is out of Git, so are the assets made here), and after
tools/vehicles/unreal/make_epic_vehicles.py, which makes the police car:
    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/police/unreal/make_police_look.py

The police car is vehicle13 of City Sample Vehicles with our two lights on the roof, and nothing says "police" on it.
The officers wear the crowd's garments in their own colours.

1. /Game/Vehicles/Epic/GTP/Police/M_GTP_PoliceBand and M_GTP_PoliceStripe: deferred decals, plain blue and plain
   red. AGTPPoliceDirector hangs them on each police car, projected across the car at door height: a blue band
   with a thin red stripe above it on both sides, on the body's own shape.
2. /Game/Vehicles/Epic/GTP/Police/MI_GTP_PolicePaint_<mesh>_<slot>: for each material slot that is body paint, on
   each mesh that draws the car, a child of Epic's material with its colour parameters set to white. The meshes are
   the car's body and the look parts its Blueprint lists (a City Sample car's body is a rig without geometry; its
   exterior and its glass are drawn by look parts). The director puts each child on its slot.
3. /Game/Crowd/GTP/Police/MI_GTP_Uniform_<garment>_<slot>: for each material slot of the officers' garments, a
   child of Epic's material with its colour parameters set to navy blue (black for the shoes). AGTPPoliceOfficer
   puts them on the garments.

A child that only changes colours has no shader of its own: it is cooked with its parent's. The navy children have
loaded without an error since gate 66. A paint of our own (M_GTP_PolicePlain, 23/11) did not: it logged "invalid
ShaderMap" at gates 67 and 68, and its pixel shader instructions read 0 in this commandlet whatever its nodes, so
that figure cannot tell here whether it compiles. It is no longer made; GTP_POLICE_PLAIN=1 makes it again for a
slot that has no material (GTP_POLICE_MASK=0: white all over; GTP_POLICE_GLASS, GTP_POLICE_SKIRT: its heights).

The colour parameters of Epic's materials are not known in the repository. A parameter is taken when its name is one
of COLOUR_NAMES, or holds one of COLOUR_WORDS and none of NOT_COLOUR. When a material has none, nothing is made for
that slot and the game keeps Epic's material: read the report, add the name, run again. The report lists every slot,
its material and all its parameters: Saved/PoliceLook/police_look_report.txt. It also gives the bones from the head
to the root of the officers' body and says whether the head and the run loop named in DefaultGame.ini are there.

Assets that exist are made again from their parent (same path). Nothing of Epic's is changed. Nothing is deleted,
unless GTP_POLICE_CLEAN=1 asks for the two assets of the paint of 23/11, which the game no longer loads.
GTP_POLICE_PAINT=R,G,B, GTP_POLICE_BAND=R,G,B, GTP_POLICE_STRIPE=R,G,B and GTP_POLICE_NAVY=R,G,B change the colours
(linear, 0 to 1); the band and the stripe keep theirs once made.
"""

import os
import re
import traceback

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

CAR_DIR = "/Game/Vehicles/Epic/GTP/Police"
CAR_MESH = "/Game/Vehicles/Epic/GTP/Mesh/SKM_GTP_Police13"
CAR_BLUEPRINT = "/Game/Vehicles/Epic/GTP/BP_GTP_Police13"
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
COLOUR_WORDS = re.compile(r"colou?r|tint|paint|albedo|diffuse", re.IGNORECASE)
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
STRIPE = colour_from_env("GTP_POLICE_STRIPE", (0.55, 0.0, 0.02))
NAVY = colour_from_env("GTP_POLICE_NAVY", (0.010, 0.016, 0.050))
BLACK = unreal.LinearColor(0.008, 0.008, 0.008, 1.0)
DARK = unreal.LinearColor(0.012, 0.014, 0.018, 1.0)


def shares_from_env(name, default):
    text = os.environ.get(name, "").strip()
    try:
        values = tuple(float(part) for part in text.split(","))
        if len(values) == len(default):
            return values
    except Exception:  # noqa: BLE001 (unset or malformed: the default)
        pass
    if text:
        say("%s='%s' is not %d number(s): default kept" % (name, text, len(default)))
    return default


GLASS = shares_from_env("GTP_POLICE_GLASS", (0.60, 0.90))
SKIRT = shares_from_env("GTP_POLICE_SKIRT", (0.12,))[0]


def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def make_decal(name, colour, what):
    """A plain deferred decal of one colour (as tools/combat/unreal/make_blood_material.py makes the blood pool)."""
    path = CAR_DIR + "/" + name
    if eal.does_asset_exist(path):
        say("%s: %s already there, left as it is (delete it to change its colour)" % (what, path))
        return
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, CAR_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        say("%s: NOT made (the asset could not be created)" % what)
        return
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    tone = node(material, unreal.MaterialExpressionConstant3Vector, -400, -200, constant=colour)
    rough = node(material, unreal.MaterialExpressionConstant, -400, -80, r=0.45)
    opacity = node(material, unreal.MaterialExpressionConstant, -400, 20, r=1.0)
    mel.connect_material_property(tone, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    say("%s: %s made, saved=%s" % (what, path, eal.save_loaded_asset(material, False)))


def make_band():
    """The blue band, and the thin red stripe that goes above it: the same material but for its colour (the band
    has been cooked and loaded without an error since gate 66)."""
    make_decal("M_GTP_PoliceBand", BAND, "band")
    make_decal("M_GTP_PoliceStripe", STRIPE, "stripe")


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


def link(source, target, pin, output=""):
    """Connects two nodes, the pin given by name or by its possible names (as make_blood_material.py does)."""
    names = pin if isinstance(pin, tuple) else ((pin,) if pin else ("", "Input", "X"))
    for name in names:
        if mel.connect_material_expressions(source, output, target, name):
            return
    raise RuntimeError("cannot connect %s to %s.%s" % (source.get_name(), target.get_name(), pin))


def enum_value(enum, ending):
    """The value of a Python enum whose name is `ending` after its prefix. Several names can end the same way
    (..._WORLD, ..._PERIODIC_WORLD, ..._TRANSLATED_WORLD): the shortest is the plain one. Gate 67: the first in
    alphabetical order was taken, the material read its height from a space that has no way back to the mesh's,
    did not compile, and the cooked game logged "invalid ShaderMap" when the first police car loaded it."""
    names = [name for name in dir(enum)
             if not name.startswith("_") and (name.upper() == ending.upper() or name.upper().endswith("_" + ending.upper()))]
    if not names:
        raise RuntimeError("%s has no value ending with %s" % (enum.__name__, ending))
    name = min(names, key=len)
    say("  %s: %s taken among %s" % (enum.__name__, name, ", ".join(sorted(names))))
    return getattr(enum, name)


def pixel_instructions(material):
    """Pixel shader instructions of a material, compiled if need be: 0 when it does not compile. None: not read."""
    try:
        return int(mel.get_statistics(material).num_pixel_shader_instructions)
    except Exception as error:  # noqa: BLE001 (reported)
        say("  plain paint: shader instructions not read (%r)" % error)
        return None


def height_mask(material):
    """1 at the windows' height and at the bottom of the car, 0 elsewhere; second value: 1 at the windows only.
    The height is read in the car's own space (cm above the mesh's origin), so it holds on slopes and on a car on its
    side."""
    world = node(material, unreal.MaterialExpressionWorldPosition, -1900, 300)
    local = node(material, unreal.MaterialExpressionTransformPosition, -1700, 300)
    local.set_editor_property("transform_source_type", enum_value(unreal.MaterialPositionTransformSource, "WORLD"))
    local.set_editor_property("transform_type", enum_value(unreal.MaterialPositionTransformSource, "LOCAL"))
    link(world, local, "")
    height = node(material, unreal.MaterialExpressionComponentMask, -1500, 300, r=False, g=False, b=True, a=False)
    link(local, height, "")
    low = node(material, unreal.MaterialExpressionScalarParameter, -1500, 450, parameter_name="GlassLow", default_value=85.0)
    high = node(material, unreal.MaterialExpressionScalarParameter, -1500, 550, parameter_name="GlassHigh", default_value=130.0)
    skirt = node(material, unreal.MaterialExpressionScalarParameter, -1500, 650, parameter_name="SkirtHigh", default_value=17.0)
    soft = node(material, unreal.MaterialExpressionScalarParameter, -1500, 750, parameter_name="Soft", default_value=1.5)

    def ramp(first, second, y):
        """saturate((first - second) / Soft): 0 below, 1 above, over Soft cm."""
        difference = node(material, unreal.MaterialExpressionSubtract, -1250, y)
        link(first, difference, "A")
        link(second, difference, "B")
        ratio = node(material, unreal.MaterialExpressionDivide, -1100, y)
        link(difference, ratio, "A")
        link(soft, ratio, "B")
        out = node(material, unreal.MaterialExpressionSaturate, -950, y)
        link(ratio, out, "")
        return out

    glass = node(material, unreal.MaterialExpressionMultiply, -780, 350)
    link(ramp(height, low, 300), glass, "A")
    link(ramp(high, height, 420), glass, "B")
    both = node(material, unreal.MaterialExpressionAdd, -620, 450)
    link(glass, both, "A")
    link(ramp(skirt, height, 560), both, "B")
    dark = node(material, unreal.MaterialExpressionSaturate, -480, 450)
    link(both, dark, "")
    return dark, glass


def make_plain():
    """A paint of our own for a slot that has no material: white, windows' height and bottom of the car dark.
    Made again at every run. Returns the material, or None."""
    path = CAR_DIR + "/M_GTP_PolicePlain"
    material = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if material is not None:
        mel.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_GTP_PolicePlain", CAR_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        say("  plain paint: NOT made (the asset could not be created)")
        return None
    # Without this a cooked game draws the grey default material on a skeletal mesh.
    for how in (lambda: material.set_editor_property("used_with_skeletal_mesh", True),
                lambda: mel.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)):
        try:
            how()
        except Exception as error:  # noqa: BLE001 (reported; the other way may do it)
            say("  plain paint: one way of marking it for skeletal meshes failed (%r)" % error)
    try:
        say("  plain paint: used with skeletal meshes = %s" % material.get_editor_property("used_with_skeletal_mesh"))
    except Exception as error:  # noqa: BLE001 (reported)
        say("  plain paint: skeletal mesh use not read back (%r)" % error)
    masked = white_paint(material, os.environ.get("GTP_POLICE_MASK", "1").strip() != "0")
    mel.recompile_material(material)
    count = pixel_instructions(material)
    say("  plain paint: %s, %s pixel shader instruction(s)" % (masked, "unread" if count is None else count))
    if count == 0 and masked.startswith("white, dark"):
        # A material that does not compile is cooked without shaders, and the game logs errors when it loads it.
        say("  plain paint: it did NOT COMPILE with the height mask: made again white all over")
        mel.delete_all_material_expressions(material)
        masked = white_paint(material, False)
        mel.recompile_material(material)
        count = pixel_instructions(material)
        say("  plain paint: %s, %s pixel shader instruction(s)" % (masked, "unread" if count is None else count))
    if count == 0:
        say("  plain paint: STILL NOT COMPILED: do not let the game load it (LiveryPaintFolder in DefaultGame.ini)")
    say("  plain paint: %s made, %s, saved=%s" % (path, masked, eal.save_loaded_asset(material, False)))
    return material


def white_paint(material, with_mask):
    """The nodes of the plain paint. Returns what was made, in words."""
    colour = node(material, unreal.MaterialExpressionVectorParameter, -480, -200, parameter_name="Colour", default_value=PAINT)
    rough = node(material, unreal.MaterialExpressionConstant, -480, 0, r=0.32)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if not with_mask:
        return "white all over"
    try:
        dark, glass = height_mask(material)
        dark_colour = node(material, unreal.MaterialExpressionVectorParameter, -480, -60, parameter_name="Dark", default_value=DARK)
        mixed = node(material, unreal.MaterialExpressionLinearInterpolate, -220, -120)
        link(colour, mixed, "A")
        link(dark_colour, mixed, "B")
        link(dark, mixed, "Alpha")
        shiny = node(material, unreal.MaterialExpressionConstant, -480, 100, r=0.1)
        gloss = node(material, unreal.MaterialExpressionLinearInterpolate, -220, 60)
        link(rough, gloss, "A")
        link(shiny, gloss, "B")
        link(glass, gloss, "Alpha")
        # Last, so that a failure above leaves the plain white connected.
        mel.connect_material_property(mixed, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(gloss, "", unreal.MaterialProperty.MP_ROUGHNESS)
        return "white, dark at the windows' height and at the bottom"
    except Exception:  # noqa: BLE001 (reported; the car is then white all over)
        say("  plain paint: height mask FAILED:\n%s" % traceback.format_exc())
        return "white all over (the height mask failed, see above)"


def make_plain_instance(index, mesh, plain, instance_name):
    """A child of the plain paint with the heights of this mesh, for its slot `index`."""
    bounds = mesh.get_bounds()
    bottom = bounds.origin.z - bounds.box_extent.z
    height = 2.0 * bounds.box_extent.z
    say("    mesh box: bottom %.0f cm, height %.0f cm, length %.0f, width %.0f (in the mesh's space)"
        % (bottom, height, 2.0 * bounds.box_extent.x, 2.0 * bounds.box_extent.y))
    if not make_instance(CAR_DIR, instance_name, plain, ["Colour"], PAINT):
        return False
    instance = unreal.load_asset("%s/%s" % (CAR_DIR, instance_name))
    values = {"GlassLow": bottom + height * GLASS[0], "GlassHigh": bottom + height * GLASS[1], "SkirtHigh": bottom + height * SKIRT}
    for name, value in values.items():
        mel.set_material_instance_scalar_parameter_value(instance, name, value)
    mel.update_material_instance(instance)
    say("    heights, cm above the mesh's origin: %s; saved=%s" % (
        ", ".join("%s %.0f" % (name, value) for name, value in values.items()), eal.save_loaded_asset(instance, False)))
    return True


def drawing_meshes():
    """The meshes that draw the police car: its body and, for a City Sample car, the look parts its Blueprint
    lists (AGTPVehicle::LookMeshes, set by tools/vehicles/unreal/make_epic_vehicles.py: the body is then City
    Sample's rig, without geometry)."""
    meshes = []
    body = unreal.load_asset(CAR_MESH) if eal.does_asset_exist(CAR_MESH) else None
    if body is None:
        say("  the body %s is not there (tools/vehicles/unreal/make_epic_vehicles.py makes it)" % CAR_MESH)
    else:
        meshes.append(body)
    try:
        cls = eal.load_blueprint_class(CAR_BLUEPRINT)
        looks = [mesh for mesh in unreal.get_default_object(cls).get_editor_property("look_meshes") if mesh is not None]
        say("  Blueprint %s lists %d look part(s): %s" % (CAR_BLUEPRINT, len(looks), ", ".join(mesh.get_name() for mesh in looks) or "none"))
        meshes.extend(looks)
    except Exception as error:  # noqa: BLE001 (reported)
        say("  look parts of %s not read (%r)" % (CAR_BLUEPRINT, error))
    return meshes


def is_paint(slot_name, material_name):
    """Body paint, told by the slot's name; by the material's name only when the slot's says nothing (a material
    may carry a colour's name, 'lightGrey', that the words for lamps would refuse)."""
    if PAINT_SLOT.search(slot_name):
        return not NOT_PAINT_SLOT.search(slot_name)
    if NOT_PAINT_SLOT.search(slot_name):
        return False
    return bool(PAINT_SLOT.search(material_name)) and not NOT_PAINT_SLOT.search(material_name)


def make_paint():
    """MI_GTP_PolicePaint_<mesh>_<slot> for each paint slot of the meshes that draw the car: a child of Epic's own
    paint with its colour parameters set to white. A child that only changes colours brings no shader of its own:
    it is cooked with its parent's (as the officers' navy children, loaded without an error since gate 66). The
    paint of our own of 23/11 (M_GTP_PolicePlain) logged errors at gates 67 and 68 and is no longer made, unless
    GTP_POLICE_PLAIN=1 asks for it on a slot that has no material."""
    say("== police car paint")
    plain_asked = os.environ.get("GTP_POLICE_PLAIN", "0").strip() == "1"
    made = []
    plain = None
    for mesh in drawing_meshes():
        slots = slots_of(mesh)
        say(" %s: %d slot(s)" % (mesh.get_path_name(), len(slots)))
        try:
            bounds = mesh.get_bounds()
            say("  box %.0f x %.0f x %.0f cm" % (2 * bounds.box_extent.x, 2 * bounds.box_extent.y, 2 * bounds.box_extent.z))
            drawn = max(bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z) >= 50.0
        except Exception as error:  # noqa: BLE001 (reported)
            say("  box not read (%r)" % error)
            drawn = True
        for index, name, material in slots:
            found = describe(index, name, material)
            instance = "MI_GTP_PolicePaint_%s_%d" % (mesh.get_name(), index)
            if material is None:
                if not drawn:
                    say("    no material and no geometry (a rig): nothing to paint")
                elif not plain_asked:
                    say("    no material: left as it is (GTP_POLICE_PLAIN=1 gives it a plain white of our own)")
                else:
                    plain = plain or make_plain()
                    if plain is not None and make_plain_instance(index, mesh, plain, instance):
                        made.append(instance)
                continue
            if not is_paint(name, material.get_name()):
                say("    not body paint")
                continue
            names = colour_parameters(found["vector"])
            if not names:
                say("    BODY PAINT, but none of its vector parameters reads as its colour: NOTHING MADE, the car keeps "
                    "Epic's colour (give the parameter's name, read above, to COLOUR_NAMES)")
                continue
            if make_instance(CAR_DIR, instance, material, names, PAINT):
                made.append(instance)
    say("  %d paint instance(s) made: %s" % (len(made), ", ".join(made) or "none"))
    say("  the game puts each on its slot when LiveryPaintFolder is %s (DefaultGame.ini); its line: 'police: livery on'" % CAR_DIR)


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


OLD_PAINT = (CAR_DIR + "/MI_GTP_PolicePaint_0", CAR_DIR + "/M_GTP_PolicePlain")


def old_paint():
    """The paint of our own of 23/11: the game no longer asks for it, so it is cooked and never loaded. Left where
    it is unless GTP_POLICE_CLEAN=1 asks for these two assets, by their exact paths, to be deleted."""
    say("== paint of 23/11 (no longer loaded by the game)")
    clean = os.environ.get("GTP_POLICE_CLEAN", "0").strip() == "1" and os.environ.get("GTP_POLICE_PLAIN", "0").strip() != "1"
    for path in OLD_PAINT:
        if not eal.does_asset_exist(path):
            say("  %s: not there" % path)
        elif not clean:
            say("  %s: still there, never loaded (GTP_POLICE_CLEAN=1 deletes it)" % path)
        else:
            deleted = eal.delete_asset(path)
            say("  %s: delete asked (%s), there afterwards: %s" % (path, deleted, eal.does_asset_exist(path)))


def main():
    for step in (make_band, make_paint, old_paint, make_uniform, report_body):
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
