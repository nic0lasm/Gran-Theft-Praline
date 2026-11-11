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
MAX_SHADOWED_LIGHTS = 6                    # ADR-0012 budget on M1
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


def build_shell(spec):
    frame = spec["frame"]
    width, depth, storey = frame["width"] * 100, frame["depth"] * 100, frame["storey_height"] * 100
    levels = sorted({zone["level"] for zone in spec["zones"]})
    count = 0
    for level in levels:
        z0 = level * storey
        box_actor("Floor_L%d" % level, "Interior/Shell", (width / 2, -depth / 2, z0 - WALL_CM / 2), (width, depth, WALL_CM))
        box_actor("Ceiling_L%d" % level, "Interior/Shell", (width / 2, -depth / 2, z0 + storey - WALL_CM / 2), (width, depth, WALL_CM))
        box_actor("WallBack_L%d" % level, "Interior/Shell", (width / 2, -depth, z0 + storey / 2), (width, WALL_CM, storey))
        box_actor("WallWest_L%d" % level, "Interior/Shell", (0, -depth / 2, z0 + storey / 2), (WALL_CM, depth, storey))
        box_actor("WallEast_L%d" % level, "Interior/Shell", (width, -depth / 2, z0 + storey / 2), (WALL_CM, depth, storey))
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


def hism_actor(label, mesh):
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    actor.set_actor_label(label)
    actor.set_folder_path("Interior/Objects")
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
    for (cls, level, density), rows in sorted(groups.items()):
        info = catalog["classes"][cls]
        rule = rules[info["size"]]
        mesh, grey = mesh_for(cls, mapping, missing)
        comp = hism_actor("%s_L%d_D%d" % (cls, level, density), mesh)
        dims = info.get("dims_m", rule["dims_m"])
        # Grey boxes: the 1 m cube scaled to the class size, resting on its placement point.
        scale = unreal.Vector(dims[0], dims[1], dims[2]) if grey else unreal.Vector(1, 1, 1)
        lift = dims[2] * 50.0 if grey else 0.0
        for p in rows:
            comp.add_instance(unreal.Transform(
                unreal.Vector(p["x"] * 100.0, -p["y"] * 100.0, p["z"] * 100.0 + lift),
                unreal.Rotator(0.0, 0.0, float(p["yaw"])), scale))
        if rule["cull_m"] > 0:
            comp.set_editor_property("instance_start_cull_distance", int(rule["cull_m"] * 80))
            comp.set_editor_property("instance_end_cull_distance", int(rule["cull_m"] * 100))
        comp.set_editor_property("cast_shadow", bool(rule["shadow_pc"]))
        tags = [unreal.Name("GTP_MinDensity=%d" % density), unreal.Name("GTP_Class=" + cls)]
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


def build_lights(spec):
    storey = spec["frame"]["storey_height"] * 100
    zones = sorted(spec["zones"], key=lambda z: -il.rect_area(z["rect"]))
    for index, zone in enumerate(zones):
        x0, y0, x1, y1 = zone["rect"]
        center = ((x0 + x1) * 50.0, -(y0 + y1) * 50.0, zone["level"] * storey + storey - 40.0)
        light = actor_sub.spawn_actor_from_class(unreal.RectLight, unreal.Vector(*center), unreal.Rotator(0.0, -90.0, 0.0))   # roll, pitch, yaw: facing down
        light.set_actor_label("Light_" + zone["name"])
        light.set_folder_path("Interior/Lights")
        comp = light.get_component_by_class(unreal.RectLightComponent)
        comp.set_editor_property("source_width", (x1 - x0) * 60.0)
        comp.set_editor_property("source_height", (y1 - y0) * 60.0)
        comp.set_editor_property("intensity", 40.0)
        comp.set_editor_property("attenuation_radius", max(x1 - x0, y1 - y0) * 120.0)
        comp.set_editor_property("cast_shadows", index < MAX_SHADOWED_LIGHTS)
    return len(zones)


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
    anchor.set_editor_property("tags", [unreal.Name("GTP_InteriorAnchor"), unreal.Name("GTP_InteriorLevel=" + level_path(spec))])
    level_sub.save_current_level()
    report["anchor"] = {"map": CITY_MAP, "location": [location.x, location.y, location.z], "yaw": rotation.yaw}


def remove_previous(path, report):
    """The level is generated, never edited by hand, so a rebuild starts from zero.
    A loaded level cannot be deleted: switch to an unsaved blank map first, then
    delete the map and its one-file-per-actor folder."""
    if not eal.does_asset_exist(path):
        return True
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    eal.delete_asset(path)
    package, name = path.rsplit("/", 1)
    external = "/Game/__ExternalActors__" + package[len("/Game"):] + "/" + name
    for folder in (external, external.replace("__ExternalActors__", "__ExternalObjects__")):
        if eal.does_directory_exist(folder):
            eal.delete_directory(folder)
    if eal.does_asset_exist(path):
        report["errors"].append("cannot delete previous " + path + " (open in another editor?)")
        return False
    log("previous %s deleted" % path)
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
    level_sub.save_current_level()
    if WITH_ANCHOR:
        place_anchor(spec, report)
    write(report, t0, spec["id"])


main()
