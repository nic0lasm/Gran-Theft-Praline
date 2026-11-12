"""Build an interior streaming level from its spec, inside the Unreal Editor (ADR-0012, step 2).

Run headless like the city scripts (script copied to a path without spaces):
    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript -script=<copy of this file>
Environment:
    GTP_INTERIOR_SPEC   spec JSON (default tools/interiors/fastfood_bellecour.json)
    GTP_INTERIOR_ANCHOR 1 to also place the anchor in the city map (GTP_CITY_FOCUS, as
                        place_props.py); 0 (default) builds the interior level only
    GTP_INTERIOR_MESHES optional JSON {class: "/Game/.../SM_x"} mapping classes to real
                        meshes (local, out of Git); classes without a mesh get a grey box

What it does (idempotent, the level is rebuilt from zero each run):
1. new level /Game/Interiors/L_Interior_<id>, in the interior frame (origin = west corner
   of the front, x along the front, y inward, Unreal axes);
2. shell: floor and ceiling slabs per storey, side and back walls, the glass front with
   the door gap; all grey boxes until the realism thread's meshes exist;
3. one actor per instanced group (class, storey, density level) with a hierarchical
   instanced static mesh component: grey box scaled to the class size (catalog.json) or
   the mapped mesh, cull distances, shadows, collision and the tags read at run time by
   UGTPInteriorStreaming: GTP_MinDensity=<n>, GTP_NoShadowM1, GTP_NoCollision;
4. lights: one shadowed rect light per zone up to the M1 cap, the others unshadowed;
   work station slots: TargetPoints tagged GTP_Station=<station>_<n>, GTP_StationRole=<role>;
5. optional anchor in the city map (TargetPoint tagged GTP_InteriorAnchor and
   GTP_InteriorLevel=<package>) at the spec's anchor transform.
Report: Saved/InteriorBuild/<id>.json (counts per group, missing meshes, timings).
"""

import json
import math
import os
import shutil
import sys
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "interiors"))
import interior_layout as il  # noqa: E402

SPEC = os.environ.get("GTP_INTERIOR_SPEC", os.path.join(REPO, "tools", "interiors", "fastfood_bellecour.json"))
WITH_ANCHOR = os.environ.get("GTP_INTERIOR_ANCHOR", "0") == "1"
MESH_MAP = os.environ.get("GTP_INTERIOR_MESHES", "")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
CITY_MAP = "/Game/Maps/L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
CUBE = "/Engine/BasicShapes/Cube"          # 100 cm cube centred on its origin
GREY = "/Engine/BasicShapes/BasicShapeMaterial"
# Stand-in materials for classes still without a mesh: a brushed steel fryer box reads as kitchen
# equipment, a plain grey cube does not. Created once under STANDIN_DIR (local content, out of Git).
STANDIN_DIR = "/Game/Interiors/StandIn"
STANDIN = {  # family: (base colour, metallic, roughness)
    "Steel": ((0.56, 0.57, 0.58), 1.0, 0.35),
    "Black": ((0.02, 0.02, 0.022), 0.0, 0.3),
    "Paper": ((0.82, 0.80, 0.74), 0.0, 0.85),
    "Brown": ((0.18, 0.09, 0.04), 0.0, 0.7),
    "Wood": ((0.36, 0.22, 0.12), 0.0, 0.55),
}
STANDIN_CLASSES = {
    "Steel": ("fryer", "clamshell_grill", "holding_cabinet", "drink_tower", "shake_machine", "ice_bin", "dishwasher",
              "bun_toaster", "assembly_table", "oil_filter_tray", "utensil_rack", "patty_tray", "bun_tray",
              "condiment_station", "condiment_insert", "tray_return", "safe", "triple_sink", "speed_rack",
              "wire_shelf", "fry_dump", "fry_basket", "fry_scoop", "holding_tray", "sauce_gun", "scraper", "spatula",
              "utensil", "pastry_case", "milk_fridge", "frozen_case", "coffee_machine", "cash_drawer", "locker",
              "wash_basin", "hand_dryer", "vent_grille", "floor_drain", "sprinkler"),
    "Black": ("card_reader", "receipt_printer", "kvs_screen", "order_screen", "pos_terminal", "monitor",
              "wall_menu_screen", "kiosk", "office_chair"),
    "Paper": ("wrapper", "wrapper_kit", "receipt", "napkin", "napkin_kit", "lid", "lid_kit", "sauce_packet",
              "sauce_packet_kit", "straw_box", "straw", "dessert_cup_kit", "cup", "cup_kit", "bag_kit", "fry_bag",
              "burger_box", "burger_box_kit", "fry_carton", "fry_carton_kit", "case_box", "binder", "ceiling_tile"),
    "Brown": ("patty", "bun", "pastry", "fry_single"),
    "Wood": ("table_2", "table_4", "high_table", "desk", "bench", "booth", "booth_bench", "stool", "chair", "high_chair"),
}
LIGHT_BELOW_CEILING_CM = 55.0              # 15 cm under the drop ceiling (tiles at storey - 40 cm)
UPLIGHT_Z_CM = 230.0                       # uplight panels at 2.3 m, above heads, under the 2.6 m+ drop ceiling
UPLIGHT_SHARE = 0.25
MAX_SHADOWED_LIGHTS = 4                    # movable shadowed lights, under the ADR-0012 budget of 6 on M1
# Target illuminance per zone (lux): a fast-food room is lit at 500, the counter and the kitchen
# more (EN 12464-1 orders of magnitude), back rooms less. Exposure inside: UGTPInteriorStreaming.
ZONE_LUX = {"dining_ground": 500, "dining_upper": 500, "kiosks": 600, "queue": 600, "counter": 750, "kitchen": 750,
            "storage": 300, "walk_in": 300, "stairs": 250, "crew_room": 300, "toilets": 250}
WALL_CM = 20.0
DOOR_WIDTH_CM = 200.0

eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log_warning("[Interior] " + msg)


def level_path(spec):
    return "/Game/Interiors/L_Interior_" + spec["id"]


def box_actor(label, folder, center, size, mesh=None):
    """A static mesh actor scaled from the 1 m cube: walls, slabs, glass."""
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*center))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh or eal.load_asset(CUBE))
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


# Shell surfaces without textures: a procedural tile grid (world aligned, so it never stretches
# with the box scale), painted walls with a tiled wainscot, a matt ceiling. Created once under
# STANDIN_DIR; plain static mesh actors, no instancing flag needed.
SHELL_TILE_CM = {"Floor": 60.0, "Wainscot": 20.0}
WAINSCOT_TOP = -50.0 + 120.0 / 3.6   # cube local z of 1.2 m on a 3.6 m storey wall


class _Graph:
    def __init__(self, mat):
        self.mat, self.y = mat, 0
        self.mel = unreal.MaterialEditingLibrary

    def node(self, cls, **props):
        e = self.mel.create_material_expression(self.mat, cls, -900, self.y)
        self.y += 120
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, pin=""):
        for name in ((pin,) if pin else ("", "Input", "X")):
            if self.mel.connect_material_expressions(a, "", b, name):
                return
        raise RuntimeError("cannot connect %s -> %s.%s" % (a.get_name(), b.get_name(), pin))

    def const(self, v):
        if isinstance(v, (int, float)):
            return self.node(unreal.MaterialExpressionConstant, r=float(v))
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(v[0], v[1], v[2], 1.0))

    def op(self, cls, a, b):
        e = self.node(cls)
        self.link(a, e, "A")
        self.link(b, e, "B")
        return e

    def unary(self, cls, a, **props):
        e = self.node(cls, **props)
        self.link(a, e)
        return e

    def lerp(self, a, b, alpha):
        e = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, e, "A")
        self.link(b, e, "B")
        self.link(alpha, e, "Alpha")
        return e

    def joints(self, uv, tile_cm, width=0.035):
        """0 on a tile, 1 on its joint: per axis |frac(uv / tile) - 0.5|, the max of both, sharpened."""
        f = self.unary(unreal.MaterialExpressionFrac, self.op(unreal.MaterialExpressionDivide, uv, self.const(tile_cm)))
        d = self.unary(unreal.MaterialExpressionAbs, self.op(unreal.MaterialExpressionSubtract, f, self.const(0.5)))
        m = self.op(unreal.MaterialExpressionMax,
                    self.unary(unreal.MaterialExpressionComponentMask, d, r=True, g=False, b=False, a=False),
                    self.unary(unreal.MaterialExpressionComponentMask, d, r=False, g=True, b=False, a=False))
        edge = self.op(unreal.MaterialExpressionMultiply,
                       self.op(unreal.MaterialExpressionSubtract, m, self.const(0.5 - width)), self.const(1.0 / width))
        return self.unary(unreal.MaterialExpressionSaturate, edge)


def shell_material(kind):
    """M_Shell_<kind> (Floor, Wall, Ceiling), created when missing; None on failure (grey kept)."""
    path = "%s/M_Shell_%s" % (STANDIN_DIR, kind)
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    mel = unreal.MaterialEditingLibrary
    try:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_Shell_" + kind, STANDIN_DIR, unreal.Material, unreal.MaterialFactoryNew())
        g = _Graph(mat)
        world = g.node(unreal.MaterialExpressionWorldPosition)
        if kind == "Floor":
            # Dark porcelain tiles, 60 cm, light grout: the usual fast-food floor.
            uv = g.unary(unreal.MaterialExpressionComponentMask, world, r=True, g=True, b=False, a=False)
            joint = g.joints(uv, SHELL_TILE_CM["Floor"], 0.02)
            colour = g.lerp(g.const((0.055, 0.05, 0.047)), g.const((0.16, 0.15, 0.14)), joint)
            rough = g.lerp(g.const(0.28), g.const(0.85), joint)
        elif kind == "Wall":
            # Warm paint above, small dark red tiles up to 1.2 m (cube local z), grout between them.
            across = g.op(unreal.MaterialExpressionAdd,
                          g.unary(unreal.MaterialExpressionComponentMask, world, r=True, g=False, b=False, a=False),
                          g.unary(unreal.MaterialExpressionComponentMask, world, r=False, g=True, b=False, a=False))
            up = g.unary(unreal.MaterialExpressionComponentMask, world, r=False, g=False, b=True, a=False)
            uv = g.node(unreal.MaterialExpressionAppendVector)
            g.link(across, uv, "A")
            g.link(up, uv, "B")
            joint = g.joints(uv, SHELL_TILE_CM["Wainscot"], 0.05)
            tiles = g.lerp(g.const((0.23, 0.045, 0.035)), g.const((0.5, 0.48, 0.45)), joint)
            local = g.node(unreal.MaterialExpressionLocalPosition)
            lz = g.unary(unreal.MaterialExpressionComponentMask, local, r=False, g=False, b=True, a=False)
            # 1 above the wainscot top, 0 below, 1 cm blend.
            above = g.unary(unreal.MaterialExpressionSaturate,
                            g.op(unreal.MaterialExpressionMultiply,
                                 g.op(unreal.MaterialExpressionSubtract, lz, g.const(WAINSCOT_TOP)), g.const(2.0)))
            colour = g.lerp(tiles, g.const((0.62, 0.57, 0.5)), above)
            rough = g.lerp(g.lerp(g.const(0.25), g.const(0.85), joint), g.const(0.8), above)
        else:
            colour, rough = g.const((0.78, 0.77, 0.74)), g.const(0.92)
        mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(mat)
        save_in_project(mat)
        return mat
    except Exception as error:
        unreal.log_warning("[InteriorLevel] shell material %s not created: %s" % (kind, error))
        return None


def shell_paint(actor, kind):
    mat = shell_material(kind)
    if mat is not None:
        actor.static_mesh_component.set_material(0, mat)
    return actor


def build_shell(spec):
    frame = spec["frame"]
    width, depth, storey = frame["width"] * 100, frame["depth"] * 100, frame["storey_height"] * 100
    levels = sorted({zone["level"] for zone in spec["zones"]})
    count = 0
    for level in levels:
        z0 = level * storey
        shell_paint(box_actor("Floor_L%d" % level, "Interior/Shell", (width / 2, -depth / 2, z0 - WALL_CM / 2), (width, depth, WALL_CM)), "Floor")
        shell_paint(box_actor("Ceiling_L%d" % level, "Interior/Shell", (width / 2, -depth / 2, z0 + storey - WALL_CM / 2), (width, depth, WALL_CM)), "Ceiling")
        shell_paint(box_actor("WallBack_L%d" % level, "Interior/Shell", (width / 2, -depth, z0 + storey / 2), (width, WALL_CM, storey)), "Wall")
        shell_paint(box_actor("WallWest_L%d" % level, "Interior/Shell", (0, -depth / 2, z0 + storey / 2), (WALL_CM, depth, storey)), "Wall")
        shell_paint(box_actor("WallEast_L%d" % level, "Interior/Shell", (width, -depth / 2, z0 + storey / 2), (WALL_CM, depth, storey)), "Wall")
        count += 5
    # Glass front on the place, ground floor, with the door gap on the rue de la Republique side.
    glass_w = width - DOOR_WIDTH_CM
    front = box_actor("GlassFront", "Interior/Shell", (glass_w / 2, 0, storey / 2), (glass_w, 2.0, storey))
    front.static_mesh_component.set_editor_property("cast_shadow", False)
    front.set_editor_property("tags", [unreal.Name("GTP_Glass")])
    return count + 1


def mesh_for(cls, mapping, missing):
    path = mapping.get(cls)
    if path and eal.does_asset_exist(path):
        return eal.load_asset(path), False
    if path:
        missing.append(cls)
    return eal.load_asset(CUBE), True


def standin_material(cls, cache):
    """The stand-in material of a grey class, created when missing; None keeps the grey cube."""
    family = next((f for f, names in STANDIN_CLASSES.items() if cls in names), None)
    if family is None:
        return None
    if family in cache:
        return cache[family]
    path = "%s/M_StandIn_%s" % (STANDIN_DIR, family)
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
        if mat and not mat.get_editor_property("used_with_instanced_static_meshes"):
            # Created by 994c26a without the flag: set it and rebuild the shaders once.
            mat.set_editor_property("used_with_instanced_static_meshes", True)
            unreal.MaterialEditingLibrary.recompile_material(mat)
            save_in_project(mat)
        cache[family] = mat
        return mat
    mel = unreal.MaterialEditingLibrary
    colour, metallic, roughness = STANDIN[family]
    try:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_StandIn_" + family, STANDIN_DIR, unreal.Material, unreal.MaterialFactoryNew())
        # Placed on instanced components: without the usage flag a cooked game falls back to the
        # default material ("missing usage flag InstancedStaticMeshes").
        mat.set_editor_property("used_with_instanced_static_meshes", True)
        col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        col.set_editor_property("constant", unreal.LinearColor(colour[0], colour[1], colour[2], 1.0))
        mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
        for i, (value, prop) in enumerate(((metallic, unreal.MaterialProperty.MP_METALLIC),
                                           (roughness, unreal.MaterialProperty.MP_ROUGHNESS))):
            c = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 150 + 120 * i)
            c.set_editor_property("r", float(value))
            mel.connect_material_property(c, "", prop)
        mel.recompile_material(mat)
        save_in_project(mat)
    except Exception as error:
        unreal.log_warning("[InteriorLevel] stand-in %s not created: %s" % (family, error))
        mat = None
    cache[family] = mat
    return mat


def save_in_project(asset):
    """Save an asset only when it belongs to the project (/Game): the engine and plug-in content is
    shared by every project on the machine and is never written."""
    path = asset.get_path_name() if asset else ""
    if not path.startswith("/Game/"):
        raise RuntimeError("refusing to save outside the project: " + path)
    return eal.save_loaded_asset(asset)


STANDIN_PARENTS = "/Game/Interiors/StandIn/Parents"   # project copies of plug-in base materials


def project_base(iface, done, report):
    """The base material of a slot as a project asset. Only /Game is ever changed or saved: when the
    base lives in the engine or a plug-in (Interchange's M_GLTF for glTF imports), a copy is made
    under STANDIN_PARENTS and the project's material instance is reparented to it. None when the
    slot is not a project material instance over such a base (left as it is, with a warning)."""
    base = iface.get_base_material()
    if base is None:
        return None
    path = base.get_path_name()
    if path.startswith("/Game/"):
        return base
    if not (isinstance(iface, unreal.MaterialInstanceConstant) and iface.get_path_name().startswith("/Game/")):
        if path not in done:
            done.add(path)
            report.setdefault("warnings", []).append("base material outside the project, left as is: " + path)
        return None
    copy_path = "%s/%s" % (STANDIN_PARENTS, base.get_name())
    copy = eal.load_asset(copy_path) if eal.does_asset_exist(copy_path) else eal.duplicate_asset(path, copy_path)
    if copy is None:
        report.setdefault("warnings", []).append("cannot copy %s into the project" % path)
        return None
    # The instance's direct parent may itself be a plug-in instance: walk up to the first /Game
    # instance whose parent is outside the project and reparent that one.
    node = iface
    while True:
        parent = node.get_editor_property("parent")
        if parent is None or not isinstance(parent, unreal.MaterialInstanceConstant) or not parent.get_path_name().startswith("/Game/"):
            break
        node = parent
    if node.get_editor_property("parent") != copy:
        unreal.MaterialEditingLibrary.set_material_instance_parent(node, copy)
        save_in_project(node)
        report.setdefault("reparented", []).append(node.get_path_name())
    return copy


def flag_for_instancing(mesh, done, report):
    """Every object is an instanced static mesh: a material without the instanced usage flag renders
    as the default checkerboard in a game and a package ("missing usage flag InstancedStaticMeshes"),
    and the downloaded packs never set it. Set it on the base materials of the mesh, once, and save.
    Never outside /Game: an engine or plug-in base is copied into the project first (project_base)."""
    for slot in mesh.get_editor_property("static_materials"):
        iface = slot.get_editor_property("material_interface")
        if iface is None:
            continue
        try:
            base = project_base(iface, done, report)
        except Exception as error:
            report.setdefault("warnings", []).append("base of %s: %s" % (iface.get_path_name(), error))
            continue
        if base is None:
            continue
        path = base.get_path_name()
        if path in done:
            continue
        done.add(path)
        if not path.startswith("/Game/") or base.get_editor_property("used_with_instanced_static_meshes"):
            continue
        try:
            base.set_editor_property("used_with_instanced_static_meshes", True)
            unreal.MaterialEditingLibrary.recompile_material(base)
            save_in_project(base)
            report.setdefault("instancing_flag_set", []).append(path)
        except Exception as error:
            report.setdefault("warnings", []).append("usage flag on %s: %s" % (path, error))


def fit_mesh(mesh, dims):
    """(uniform scale, lift cm) so a downloaded mesh has its class size and rests on its point.
    Free models come in any unit (a hand dryer 36 m wide, a shelf 156 m) and with any pivot (paper
    bags floating). The scale is the geometric mean of the sorted side ratios, capped so the largest
    side stays within 1.3 x the class's; kept at 1 when the mesh is already within 20 %."""
    bounds = mesh.get_bounds()
    ext = bounds.box_extent
    have = sorted([max(ext.x * 2.0, 0.1), max(ext.y * 2.0, 0.1), max(ext.z * 2.0, 0.1)])   # cm
    want = sorted([d * 100.0 for d in dims])
    factor = math.exp(sum(math.log(w / h) for h, w in zip(have, want)) / 3.0)
    factor = min(factor, 1.3 * want[-1] / have[-1])
    if 0.8 <= factor <= 1.25:
        factor = 1.0
    bottom = bounds.origin.z - ext.z
    return factor, -bottom * factor


def hism_actor(label, mesh):
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    actor.set_actor_label(label)
    actor.set_folder_path("Interior/Objects")
    # Sound comes from the explicit emitters of build_audio, never guessed from these names.
    actor.set_editor_property("tags", [unreal.Name("GTP_AudioLoop=None")])
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = sdl.get_object(sdl.get_data(handle))
    comp.set_static_mesh(mesh)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    return comp


def build_objects(spec, catalog, mapping, report):
    rules = catalog["size_rules"]
    groups = {}
    for p in il.generate_all(spec, catalog):
        groups.setdefault((p["class"], p["level"], p["min_density"]), []).append(p)
    missing = []
    standins = {}
    used_standins = {}
    flagged = set()
    for (cls, level, density), rows in sorted(groups.items()):
        info = catalog["classes"][cls]
        rule = rules[info["size"]]
        mesh, grey = mesh_for(cls, mapping, missing)
        comp = hism_actor("%s_L%d_D%d" % (cls, level, density), mesh)
        dims = info.get("dims_m", rule["dims_m"])
        # Grey boxes: the 1 m cube scaled to the class size, resting on its placement point.
        scale = unreal.Vector(dims[0], dims[1], dims[2]) if grey else unreal.Vector(1, 1, 1)
        lift = dims[2] * 50.0 if grey else 0.0
        if not grey:
            flag_for_instancing(mesh, flagged, report)
            factor, lift = fit_mesh(mesh, dims)
            scale = unreal.Vector(factor, factor, factor)
            if factor != 1.0:
                report.setdefault("rescaled", {})[cls] = round(factor, 4)
        if grey:
            standin = standin_material(cls, standins)
            if standin is not None:
                comp.set_material(0, standin)
                used_standins[cls] = standin.get_name()
        for p in rows:
            comp.add_instance(unreal.Transform(
                unreal.Vector(p["x"] * 100.0, -p["y"] * 100.0, p["z"] * 100.0 + lift),
                unreal.Rotator(0.0, 0.0, float(p["yaw"])), scale))
        if rule["cull_m"] > 0:
            comp.set_editor_property("instance_start_cull_distance", int(rule["cull_m"] * 80))
            comp.set_editor_property("instance_end_cull_distance", int(rule["cull_m"] * 100))
        comp.set_editor_property("cast_shadow", bool(rule["shadow_pc"]))
        tags = [unreal.Name("GTP_MinDensity=%d" % density), unreal.Name("GTP_Class=" + cls), unreal.Name("GTP_AudioLoop=None")]
        if not rule["shadow_m1"]:
            tags.append(unreal.Name("GTP_NoShadowM1"))
        if info["size"] == "micro":
            tags.append(unreal.Name("GTP_NoCollision"))
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        else:
            comp.set_collision_profile_name("BlockAll")
        if info["size"] in ("micro", "small"):
            comp.set_editor_property("affect_distance_field_lighting", False)
        comp.set_editor_property("component_tags", tags)
        report["groups"]["%s@L%d@D%d" % (cls, level, density)] = len(rows)
    report["missing_meshes"] = sorted(set(missing))
    report["grey_classes"] = sorted({cls for (cls, _l, _d) in groups if cls not in mapping})
    report["standin_classes"] = used_standins


def build_station_slots(spec, catalog):
    """One TargetPoint per work station, where the staff stands (UGTPKitchenSubsystem reads the tags)."""
    roles = {s["name"]: s.get("role", "crew") for s in spec["stations"]}
    seen = {}
    for p in il.generate_all(spec, catalog):
        tag = p["station"]
        name = tag.rsplit("_", 1)[0]
        if name not in roles or tag in seen:
            continue
        seen[tag] = p
    storey = spec["frame"]["storey_height"] * 100
    for tag, p in sorted(seen.items()):
        level = p["level"]
        # Stand 70 cm in front of the station (towards the counter side, -y in the frame).
        location = unreal.Vector(p["x"] * 100.0, -p["y"] * 100.0 + 70.0, level * storey)
        point = actor_sub.spawn_actor_from_class(unreal.TargetPoint, location)
        point.set_actor_label("Station_" + tag)
        point.set_folder_path("Interior/Stations")
        point.set_editor_property("tags", [unreal.Name("GTP_Station=" + tag), unreal.Name("GTP_StationRole=" + roles[tag.rsplit("_", 1)[0]])])
    return len(seen)


def customer_spots(spec, catalog):
    """Queue spots for UGTPCustomerSubsystem: line k = place 0 in the clear band in front of till k
    (the dining tables start 1.2 m from the counter), then the waiting places in a column of the
    queue zone, front to back. [(line, place, x m, y m)] in the spec frame, everyone facing the
    counter (+y in the spec, yaw -90 in Unreal)."""
    zones = {z["name"]: z["rect"] for z in spec["zones"] if z.get("level", 0) == 0}
    tills = {}
    for p in il.generate_all(spec, catalog):
        tag = p.get("station") or ""
        if tag.startswith("till_") and tag not in tills and p["level"] == 0:
            tills[tag] = p
    counter_y = zones["counter"][1]
    qx0, qy0, qx1, _qy1 = zones["queue"]
    columns = [x for x in (qx0 + 0.6, qx0 + 3.0, qx0 + 4.2, qx0 + 5.4) if x < qx1 - 0.3]   # clear of the bin at x0 + 1.9
    spots = []
    for line, tag in enumerate(sorted(tills)[:len(columns)]):
        spots.append((line, 0, tills[tag]["x"], counter_y - 0.55))
        y = counter_y - 0.6
        place = 1
        while y >= qy0 + 0.4:
            spots.append((line, place, columns[line], y))
            y -= 0.9
            place += 1
    return spots


def build_customer_spots(spec, catalog):
    for line, place, x, y in customer_spots(spec, catalog):
        point = actor_sub.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x * 100.0, -y * 100.0, 0.0),
                                                 unreal.Rotator(0.0, 0.0, -90.0))   # roll, pitch, yaw: facing the counter
        point.set_actor_label("Customer_q%d_%d" % (line, place))
        point.set_folder_path("Interior/Customers")
        point.set_editor_property("tags", [unreal.Name("GTP_CustomerSpot=q%d_%d" % (line, place))])
    return len(customer_spots(spec, catalog))


# Classes that hum, for the place audio (feat/audio-0.9, UGTPPlaceAudio): one emitter per object,
# the audio merges those closer than 1.5 m. Hoods sit above the hot stations.
AUDIO_LOOPS = {"fryer": "Fryer", "clamshell_grill": "Grill", "milk_fridge": "Cold", "ice_bin": "Cold",
               "shake_machine": "Shake", "drink_tower": "Drinks"}
HOOD_OVER = ("fryer", "clamshell_grill")


def audio_point(label, location, tags):
    point = actor_sub.spawn_actor_from_class(unreal.TargetPoint, location)
    point.set_actor_label(label)
    point.set_folder_path("Interior/Audio")
    point.set_editor_property("tags", [unreal.Name(t) for t in tags])
    return point


def build_audio(spec, catalog):
    """Explicit tags for the place audio: loop emitters, one room box per zone, tills."""
    storey = spec["frame"]["storey_height"] * 100
    count = {"loops": 0, "rooms": 0, "tills": 0}
    for p in il.generate_all(spec, catalog):
        loop = AUDIO_LOOPS.get(p["class"])
        if not loop:
            continue
        where = unreal.Vector(p["x"] * 100.0, -p["y"] * 100.0, p["z"] * 100.0 + 90.0)
        audio_point("Audio_%s_%d" % (loop, count["loops"]), where, ["GTP_AudioLoop=" + loop])
        count["loops"] += 1
        if p["class"] in HOOD_OVER:
            hood = unreal.Vector(where.x, where.y, p["level"] * storey + 230.0)
            audio_point("Audio_Hood_%d" % count["loops"], hood, ["GTP_AudioLoop=Hood"])
            count["loops"] += 1
    for zone in spec["zones"]:
        x0, y0, x1, y1 = zone["rect"]
        level = zone.get("level", 0)
        centre = unreal.Vector((x0 + x1) * 50.0, -(y0 + y1) * 50.0, level * storey + storey / 2.0)
        box = actor_sub.spawn_actor_from_class(unreal.TriggerBox, centre)
        box.set_actor_label("AudioRoom_" + zone["name"])
        box.set_folder_path("Interior/Audio")
        box.set_editor_property("tags", [unreal.Name("GTP_AudioRoom"), unreal.Name("GTP_AudioRoomName=" + zone["name"])])
        shape = box.get_component_by_class(unreal.BoxComponent)
        shape.set_box_extent(unreal.Vector((x1 - x0) * 50.0, (y1 - y0) * 50.0, storey / 2.0))
        shape.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        shape.set_editor_property("component_tags", [unreal.Name("GTP_AudioRoom")])
        count["rooms"] += 1
    seen = set()
    for p in il.generate_all(spec, catalog):
        tag = p.get("station", "")
        if not tag.startswith("till_") or tag in seen:
            continue
        seen.add(tag)
        # "Till" in the label and the tag: the audio finds tills by name or by GTP_AudioTill.
        audio_point("Audio_Till_%s" % tag, unreal.Vector(p["x"] * 100.0, -p["y"] * 100.0, p["z"] * 100.0 + 95.0),
                    ["GTP_AudioTill"])
        count["tills"] += 1
    return count


def shot_views(spec):
    """Three stills for shoot_interior.py, placed from the zones: (name, eye m, target m, pitch[, eye z cm])."""
    zones = {z["name"]: z["rect"] for z in spec["zones"]}

    def centre(name):
        x0, y0, x1, y1 = zones[name]
        return (x0 + x1) / 2.0, (y0 + y1) / 2.0

    qx, _qy = centre("queue")
    cx, cy = centre("counter")
    kx0, ky0, kx1, ky1 = zones["kitchen"]
    dx0, dy0, dx1, dy1 = zones["dining_ground"]
    return [
        ("01_counter", (qx, zones["queue"][1] + 0.6), (cx + 2.0, cy), -8.0),
        # From the counter end, above the heads, across the whole kitchen: a view along its axis
        # from the west end met a speed rack at arm's length.
        ("02_kitchen", (kx1 - 0.6, ky0 + 0.3), (kx0 + 2.0, ky1 - 0.5), -20.0, 230.0),
        # From the glass front by the kiosks, above the heads, across the room to the stairs corner:
        # the old corner by the counter and kiosks had a kiosk at arm's length.
        ("03_dining", (dx1 - 1.2, dy0 + 0.8), (dx0 + 1.5, dy1 - 1.0), -14.0, 210.0),
    ]


def build_shot_cameras(spec):
    """CameraActors tagged GTP_Shot=<name> (the frame flips y: UE y = -spec y)."""
    for view in shot_views(spec):
        name, eye, target, pitch = view[:4]
        eye_z = view[4] if len(view) > 4 else 165.0
        yaw = math.degrees(math.atan2(-(target[1] - eye[1]), target[0] - eye[0]))
        cam = actor_sub.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(eye[0] * 100.0, -eye[1] * 100.0, eye_z),
                                               unreal.Rotator(0.0, pitch, yaw))
        cam.set_actor_label("Shot_" + name)
        cam.set_folder_path("Interior/Shots")
        cam.set_editor_property("tags", [unreal.Name("GTP_Shot=" + name)])
        cam.camera_component.set_editor_property("field_of_view", 75.0)
        cam.camera_component.set_editor_property("constrain_aspect_ratio", False)
    return 3


def build_lights(spec):
    storey = spec["frame"]["storey_height"] * 100
    zones = sorted(spec["zones"], key=lambda z: -il.rect_area(z["rect"]))
    for index, zone in enumerate(zones):
        x0, y0, x1, y1 = zone["rect"]
        # Just under the drop ceiling (ceiling tiles and panels sit at storey - 40 cm, interior_layout
        # fixture_z): a light in the tile plane is blocked by the tiles and only lights the slab above.
        center = ((x0 + x1) * 50.0, -(y0 + y1) * 50.0, zone["level"] * storey + storey - LIGHT_BELOW_CEILING_CM)
        light = actor_sub.spawn_actor_from_class(unreal.RectLight, unreal.Vector(*center), unreal.Rotator(0.0, -90.0, 0.0))   # roll, pitch, yaw: facing down
        light.set_actor_label("Light_" + zone["name"])
        light.set_folder_path("Interior/Lights")
        comp = light.get_component_by_class(unreal.RectLightComponent)
        # Movable: a streamed level has no baked lighting, and unbuilt static or stationary lights
        # leave it black. Lumens from the target illuminance of the zone over its floor area.
        comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        comp.set_editor_property("source_width", (x1 - x0) * 60.0)
        comp.set_editor_property("source_height", (y1 - y0) * 60.0)
        comp.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        lux = ZONE_LUX.get(zone["name"], 400)
        comp.set_editor_property("intensity", lux * il.rect_area(zone["rect"]))
        comp.set_editor_property("temperature", 4000.0)
        comp.set_editor_property("use_temperature", True)
        comp.set_editor_property("attenuation_radius", max(x1 - x0, y1 - y0) * 120.0)
        comp.set_editor_property("cast_shadows", index < MAX_SHADOWED_LIGHTS)
        # Uplight: luminaires light the ceiling too, and software Lumen on the M1 bounces little,
        # so a soft unshadowed panel at head height faces up with a quarter of the flux: it lights the
        # drop ceiling and the upper walls evenly instead of a hot band right under the tiles.
        up = actor_sub.spawn_actor_from_class(unreal.RectLight, unreal.Vector(center[0], center[1], zone["level"] * storey + UPLIGHT_Z_CM),
                                              unreal.Rotator(0.0, 90.0, 0.0))   # roll, pitch, yaw: facing up
        up.set_actor_label("Uplight_" + zone["name"])
        up.set_folder_path("Interior/Lights")
        upc = up.get_component_by_class(unreal.RectLightComponent)
        upc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        upc.set_editor_property("source_width", (x1 - x0) * 60.0)
        upc.set_editor_property("source_height", (y1 - y0) * 60.0)
        upc.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        upc.set_editor_property("intensity", UPLIGHT_SHARE * lux * il.rect_area(zone["rect"]))
        upc.set_editor_property("temperature", 4000.0)
        upc.set_editor_property("use_temperature", True)
        upc.set_editor_property("attenuation_radius", max(x1 - x0, y1 - y0) * 100.0)
        upc.set_editor_property("cast_shadows", False)
    return 2 * len(zones)


def anchor_transform(spec):
    anchor = spec["anchor"]
    sx, sy, sz = anchor["shop_ue_cm"]
    ox, oy = anchor["origin_offset_m"]
    dx, dy = anchor["front_direction_m"]
    yaw = math.degrees(math.atan2(-dy, dx))   # local north flips to Unreal -Y
    return unreal.Vector(sx + ox * 100.0, sy - oy * 100.0, sz), unreal.Rotator(0.0, 0.0, yaw)


def place_anchor(spec, report):
    if not level_sub.load_level(CITY_MAP):
        report["errors"].append("cannot open " + CITY_MAP)
        return
    for actor in actor_sub.get_all_level_actors():
        if unreal.Name("GTP_InteriorAnchor") in actor.tags and unreal.Name("GTP_InteriorLevel=" + level_path(spec)) in actor.tags:
            actor_sub.destroy_actor(actor)
    location, rotation = anchor_transform(spec)
    anchor = actor_sub.spawn_actor_from_class(unreal.TargetPoint, location, rotation)
    anchor.set_actor_label("InteriorAnchor_" + spec["id"])
    anchor.set_folder_path("City/Interiors")
    frame = spec["frame"]
    storeys = 1 + max(z.get("level", 0) for z in spec["zones"])
    size = "GTP_InteriorSize=%gx%gx%g" % (frame["width"], frame["depth"], round(frame["storey_height"] * storeys, 2))
    anchor.set_editor_property("tags", [unreal.Name("GTP_InteriorAnchor"), unreal.Name("GTP_InteriorLevel=" + level_path(spec)),
                                        unreal.Name(size)])
    level_sub.save_current_level()
    report["anchor"] = {"map": CITY_MAP, "location": [location.x, location.y, location.z], "yaw": rotation.yaw}


def remove_previous(path, report):
    """The level is generated, never edited by hand, so a rebuild starts from zero.
    The editor API refuses to delete a map package the asset registry already holds, even in a
    fresh commandlet, so the generated files are removed from disk before anything loads them:
    the .umap and its one-file-per-actor folders. Only files under Content/Interiors are touched."""
    if not eal.does_asset_exist(path):
        return True
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    package, name = path.rsplit("/", 1)
    relative = package[len("/Game/"):]
    if not relative.startswith("Interiors"):
        report["errors"].append("refusing to delete outside /Game/Interiors: " + path)
        return False
    targets = [os.path.join(content, relative, name + ".umap")]
    for folder in ("__ExternalActors__", "__ExternalObjects__"):
        targets.append(os.path.join(content, folder, relative, name))
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    unreal.SystemLibrary.collect_garbage()
    for target in targets:
        if os.path.isdir(target):
            shutil.rmtree(target)
        elif os.path.isfile(target):
            os.remove(target)
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game/" + relative], True)
    try:
        registry.scan_files_synchronous([targets[0]], True)   # forget the removed file
    except Exception as error:   # older API: the path scan above is all there is
        log("scan_files_synchronous unavailable: %s" % error)
    unreal.SystemLibrary.collect_garbage()
    # The disk is the verdict: the registry can keep a removed map for a while (PC 12/11), and
    # the new level is saved over that path anyway.
    if any(os.path.exists(target) for target in targets):
        report["errors"].append("cannot delete previous " + path)
        return False
    if eal.does_asset_exist(path):
        log("previous %s deleted from disk; the asset registry still lists it" % path)
    else:
        log("previous %s deleted from disk" % path)
    return True


def write(report, t0, spec_id):
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "InteriorBuild")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, spec_id + ".json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1)
    log("report written, %d groups, %d errors" % (len(report.get("groups", {})), len(report["errors"])))


def main():
    t0 = time.time()
    spec = il.load_json(SPEC)
    catalog = il.load_json(il.DEFAULT_CATALOG)
    mapping = il.load_json(MESH_MAP) if MESH_MAP and os.path.exists(MESH_MAP) else {}
    report = {"level": level_path(spec), "errors": [], "groups": {}}
    if not remove_previous(level_path(spec), report):
        return write(report, t0, spec["id"])
    if not level_sub.new_level(level_path(spec)):
        report["errors"].append("cannot create " + level_path(spec))
        return write(report, t0, spec["id"])
    report["shell_actors"] = build_shell(spec)
    build_objects(spec, catalog, mapping, report)
    report["lights"] = build_lights(spec)
    report["station_slots"] = build_station_slots(spec, catalog)
    report["customer_spots"] = build_customer_spots(spec, catalog)
    report["audio"] = build_audio(spec, catalog)
    report["shot_cameras"] = build_shot_cameras(spec)
    level_sub.save_current_level()
    if WITH_ANCHOR:
        place_anchor(spec, report)
    write(report, t0, spec["id"])


main()
