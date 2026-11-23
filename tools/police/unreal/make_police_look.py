"""Make what the police wear, inside the Unreal Editor (no UI): the car's livery and the officers' navy blue.

Run once per clone, before a cook (Epic's content is out of Git, so are the assets made here):
    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=tools/police/unreal/make_police_look.py

The police car is vehicle13 of City Sample Vehicles with our two lights on the roof, and nothing says "police" on it.
The officers wear the crowd's garments in their own colours.

1. /Game/Vehicles/Epic/GTP/Police/M_GTP_PoliceBand: a deferred decal, plain blue. AGTPPoliceDirector hangs one on
   each police car, projected across the car at door height: a blue band on both sides, on the body's own shape.
2. /Game/Vehicles/Epic/GTP/Police/MI_GTP_PolicePaint_<slot>: for each material slot of the police car's mesh that is
   body paint, a child of Epic's material with its colour parameters set to white. The director puts them on the car.
3. /Game/Crowd/GTP/Police/MI_GTP_Uniform_<garment>_<slot>: for each material slot of the officers' garments, a
   child of Epic's material with its colour parameters set to navy blue (black for the shoes). AGTPPoliceOfficer
   puts them on the garments.

Run of 23/11: the police car's mesh has one slot, 'lambert1', with no material in it. So for a slot without a
material the script now looks at what the car's Blueprint puts on it and at the mesh of Epic's it was copied from
(named by Saved/VehicleImport/epic_vehicles_report.json), and reports both. When nothing at all is found, that slot is
drawn with the engine's grey default material, and only then the script makes a paint of its own for it:
/Game/Vehicles/Epic/GTP/Police/M_GTP_PolicePlain, white, with the windows' height and the bottom of the car darkened
(one slot covers the whole car: glass, pillars and tyres cannot be told apart, only heights can). The heights are
shares of the mesh's box: GTP_POLICE_GLASS=low,high (default 0.60,0.90) and GTP_POLICE_SKIRT=share (default 0.12).
GTP_POLICE_MASK=0 makes it white all over. The report gives its pixel shader instructions: 0 means that it did not
compile (gate 67: cooked like that, it logged LogMaterial errors in the game); it is then made again without the mask.

The colour parameters of Epic's materials are not known in the repository. A parameter is taken when its name is one
of COLOUR_NAMES, or holds one of COLOUR_WORDS and none of NOT_COLOUR. When a material has none, nothing is made for
that slot and the game keeps Epic's material: read the report, add the name, run again. The report lists every slot,
its material and all its parameters: Saved/PoliceLook/police_look_report.txt. It also gives the bones from the head
to the root of the officers' body (the vest needs the name of the upper spine bone) and says whether the head and
the run loop named in DefaultGame.ini are there.

Assets that exist are made again from their parent (same path). Nothing is deleted, nothing of Epic's is changed.
GTP_POLICE_PAINT=R,G,B, GTP_POLICE_BAND=R,G,B and GTP_POLICE_NAVY=R,G,B change the colours (linear, 0 to 1).
"""

import json
import os
import re
import traceback

import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

CAR_DIR = "/Game/Vehicles/Epic/GTP/Police"
CAR_MESH = "/Game/Vehicles/Epic/GTP/Mesh/SKM_GTP_Police13"
CAR_BLUEPRINT = "/Game/Vehicles/Epic/GTP/BP_GTP_Police13"
CAR_NAME = "Police13"  # its entry in Saved/VehicleImport/epic_vehicles_report.json
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


def make_plain_instance(index, mesh, plain):
    """MI_GTP_PolicePaint_<index>, a child of the plain paint with the heights of this mesh."""
    bounds = mesh.get_bounds()
    bottom = bounds.origin.z - bounds.box_extent.z
    height = 2.0 * bounds.box_extent.z
    say("    mesh box: bottom %.0f cm, height %.0f cm, length %.0f, width %.0f (in the mesh's space)"
        % (bottom, height, 2.0 * bounds.box_extent.x, 2.0 * bounds.box_extent.y))
    if not make_instance(CAR_DIR, "MI_GTP_PolicePaint_%d" % index, plain, ["Colour"], PAINT):
        return False
    instance = unreal.load_asset("%s/MI_GTP_PolicePaint_%d" % (CAR_DIR, index))
    values = {"GlassLow": bottom + height * GLASS[0], "GlassHigh": bottom + height * GLASS[1], "SkirtHigh": bottom + height * SKIRT}
    for name, value in values.items():
        mel.set_material_instance_scalar_parameter_value(instance, name, value)
    mel.update_material_instance(instance)
    say("    heights, cm above the mesh's origin: %s; saved=%s" % (
        ", ".join("%s %.0f" % (name, value) for name, value in values.items()), eal.save_loaded_asset(instance, False)))
    return True


def blueprint_materials():
    """What the car's Blueprint puts on its body: {slot: material}, read on its class defaults. Reported."""
    found = {}
    try:
        cls = eal.load_blueprint_class(CAR_BLUEPRINT)
        body = unreal.get_default_object(cls).get_editor_property("mesh")
        shown = None
        for name in ("skeletal_mesh_asset", "skeletal_mesh"):
            try:
                shown = body.get_editor_property(name)
                break
            except Exception:  # noqa: BLE001 (the property's name changed across versions)
                continue
        say("  Blueprint %s draws %s" % (CAR_BLUEPRINT, shown.get_path_name() if shown else "NO MESH"))
        overrides = list(body.get_editor_property("override_materials"))
        say("  its body has %d override material(s)%s" % (len(overrides), ":" if overrides else ""))
        for index, material in enumerate(overrides):
            say("    %d: %s" % (index, material.get_path_name() if material else "-"))
            if material is not None:
                found[index] = material
    except Exception as error:  # noqa: BLE001 (reported)
        say("  Blueprint %s not read (%r)" % (CAR_BLUEPRINT, error))
    return found


def epic_source():
    """The mesh of Epic's our car was copied from, named by make_epic_vehicles.py's report: {slot: material}.
    Also lists the skeletal meshes next to it: if ours is a stand-in, the one to take is among them."""
    found = {}
    path = os.path.join(unreal.Paths.project_saved_dir(), "VehicleImport", "epic_vehicles_report.json")
    try:
        with open(path, encoding="utf-8") as handle:
            entry = json.load(handle).get("cars", {}).get(CAR_NAME, {})
    except Exception as error:  # noqa: BLE001 (reported)
        say("  Saved/VehicleImport/epic_vehicles_report.json not read (%r): Epic's mesh not looked at" % error)
        return found
    for key in ("epic", "mode", "epic_kind", "mesh_from", "components_from", "blueprint_meshes", "mesh_candidates", "mesh_source"):
        if key in entry:
            say("  vehicles report, %s: %s" % (key, entry[key]))
    source = str(entry.get("mesh_source") or "")
    if not source:
        say("  the vehicles report names no mesh of Epic's for %s" % CAR_NAME)
        return found
    package = source.split(".")[0]
    mesh = unreal.load_asset(package) if eal.does_asset_exist(package) else None
    if mesh is None:
        say("  Epic's mesh %s is not there" % package)
    else:
        say("  Epic's mesh %s:" % package)
        for index, name, material in slots_of(mesh):
            say("    slot %d '%s': %s" % (index, name, material.get_path_name() if material else "no material"))
            if material is not None:
                found[index] = material
    try:
        # The folder of Epic's Blueprint for that car holds its meshes; without it, the mesh's own folder.
        folder = str(entry.get("epic") or package).split(".")[0].rsplit("/", 1)[0]
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        say("  skeletal meshes under %s:" % folder)
        listed = 0
        for data in registry.get_assets_by_path(folder, recursive=True):
            if str(data.asset_class_path.asset_name) != "SkeletalMesh":
                continue
            listed += 1
            if listed > 16:
                say("    (more, not listed)")
                break
            other = unreal.load_asset(str(data.package_name))
            slots = slots_of(other) if other is not None else []
            say("    %s: %d slot(s), %d with a material (%s)" % (
                data.package_name, len(slots), sum(1 for _, _, material in slots if material is not None),
                ", ".join(name for _, name, _ in slots[:8])))
    except Exception as error:  # noqa: BLE001 (reported)
        say("  skeletal meshes next to Epic's not listed (%r)" % error)
    return found


def make_paint():
    say("== police car paint (%s)" % CAR_MESH)
    mesh = unreal.load_asset(CAR_MESH) if eal.does_asset_exist(CAR_MESH) else None
    if mesh is None:
        say("  the mesh is not there (tools/vehicles/unreal/make_epic_vehicles.py makes it): no paint made")
        return
    slots = slots_of(mesh)
    from_blueprint = blueprint_materials()
    from_epic = epic_source() if any(material is None for _, _, material in slots) else {}
    made = 0
    plain = None
    for index, name, material in slots:
        found = describe(index, name, material)
        if material is None:
            # Not on the mesh: what the Blueprint puts on that slot, else what Epic's own mesh has there.
            for where, materials in (("the Blueprint's override", from_blueprint), ("Epic's mesh", from_epic)):
                if materials.get(index) is not None:
                    material = materials[index]
                    say("    taken from %s:" % where)
                    found = describe(index, name, material)
                    break
        if material is None:
            say("    nothing draws this slot but the engine's grey default material: plain paint of our own")
            plain = plain or make_plain()
            if plain is not None:
                made += 1 if make_plain_instance(index, mesh, plain) else 0
            continue
        label = "%s %s" % (name, material.get_name())
        # A mesh of one slot is the whole car: its only material is the paint, whatever its name.
        if len(slots) > 1 and (not PAINT_SLOT.search(label) or NOT_PAINT_SLOT.search(label)):
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
