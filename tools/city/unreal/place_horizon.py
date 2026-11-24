"""Place the far view of tools/city/horizon.py in the street map (Unreal Editor, headless).

Gate 76 video, 60 s: from the quay the far bank ended on a dark blue band under the sky, like a
sea, and the far volumes stood alone on it. This step puts in the map, in the outliner folder
City/Horizon: a ground to the real horizon, 1 m under the lowest terrain; the rows of far
blocks and the towers of the Part-Dieu, with the procedural Lyon facade (M_City_Horizon_<n>,
built by apply_city_materials.py: run the materials step first); the two hills; the basilica
of Fourviere and its metal tower, on the edge of the hill; and, where a square runs past the
end of the map's ground (place Bellecour), that ground carried on, the fronts round the square
and two rows of trees (GTP_SQUARE_SETTING=0 leaves them out). Engine basic shapes only (cube,
plane, sphere, cylinder, cone): no mesh to import, about 250 actors of a few triangles that the
renderer batches by material. No collision, no shadow, not in the ray tracing scene.

Idempotent: the folder is emptied first. GTP_HORIZON=0 empties it and places nothing (undo).
Saves the map. Report: Saved/CityImport/horizon_report.json.
"""

import json
import os
import sys
import time

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import horizon  # noqa: E402  pure Python, shared with the tests

ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
CAMEL = "".join(p.capitalize() for p in FOCUS.split("-"))
ROOT = "/Game/Environment/City/" + "".join(p.capitalize() for p in ZONE.split("-"))
MAT_DIR = ROOT + "/Materials"
MAP = "/Game/Maps/L_City_" + CAMEL
FOLDER = "City/Horizon"
TERRAIN_FOLDER = "City/Terrain"
ENABLED = os.environ.get("GTP_HORIZON", "1") != "0"
SEED = int(os.environ.get("GTP_HORIZON_SEED", "7"))
FALLBACK_BASE_CM = -1500.0     # when the map has no terrain actor to measure
BELOW_TERRAIN_CM = 100.0
HILL_FOOT_M = 6.0              # a block on a slope goes this far into the hill
MONUMENT_MATERIALS = {"stone": "M_City_HorizonStone", "iron": "M_City_HorizonIron"}
REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
BUILT_FOLDERS = ("City/Backdrop", "City/Buildings", "City/Landmarks")   # real volumes: no stand-in on them
SQUARE_SETTING = os.environ.get("GTP_SQUARE_SETTING", "1") != "0"
GROUND_MATERIAL = "M_City_Ground"
APRON_BELOW_CM = 3.0           # the carried-on ground lies this far under the level measured on the map
LEVEL_GRID_CM = 800.0          # the square's level: one trace every 8 m on the part the map holds
LEVEL_MIN_HITS = 8
LEVEL_REACH_CM = (6000.0, 15000.0)     # street furniture this close to the outline tells the level
GROUND_FOLDERS = ("City/Props", "City/Route")
TREE_SUNK_CM = 10.0
TREE_CLEAR_M = 8.0             # no tree this close to the line the flight drives (broad crowns)
ROUTE_MAPLES = os.environ.get("GTP_ROUTE_MAPLES", "1") != "0"
ROUTE_FOLDER = "City/Route"
TREE_CULL_CM = 60000.0

eal = unreal.EditorAssetLibrary


def quiet(comp, report):
    """Far scenery: seen, and nothing else."""
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for prop, value in (("cast_shadow", False), ("visible_in_ray_tracing", False),
                        ("affect_dynamic_indirect_lighting", False), ("affect_distance_field_lighting", False),
                        ("generate_overlap_events", False), ("can_ever_affect_navigation", False)):
        try:
            comp.set_editor_property(prop, value)
        except Exception:  # noqa: BLE001  a property renamed between engine versions: noted, not fatal
            report["skipped_properties"] = sorted(set(report.get("skipped_properties", [])) | {prop})


def solid(comp):
    """What stands round a square is close enough to walk or drive into."""
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    comp.set_collision_profile_name("BlockAll")


def trace_z(world, x, y, z_top, z_bottom):
    """Height of what a downward trace meets at (x, y), or None (as place_props.py reads it)."""
    res = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, z_top), unreal.Vector(x, y, z_bottom),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if isinstance(res, tuple):
        res = res[1] if res[0] else None
    return None if res is None else res.to_tuple()[4].z


def ground_points():
    """(x, y, z) of every instance of the street furniture and of the route dressing: the generator
    set each one on the terrain from the LiDAR model, and the lanes step reads them the same way."""
    out = []
    for actor in actor_sub_().get_all_level_actors():
        if not str(actor.get_folder_path()).startswith(GROUND_FOLDERS):
            continue
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            for i in range(comp.get_instance_count()):
                t = comp.get_instance_transform(i, True).translation
                out.append((t.x, t.y, t.z))
    return out


def square_level(world, squares, ring, tiles, base, points):
    """(level in cm, how it was found) of a square.

    Prep 81: every downward trace went through the terrain (in the headless editor its tiles
    answer no trace, as the pits step had found) and stopped on the far ground of this very
    folder, 6.7 m under the street: the fronts stood 6.7 m too low and the trees were buried to
    their crowns. The lanes file carries no height either. The level is now read, in this order:
    on the street furniture standing inside the outline or within 60 m of it, then 150 m; on the
    traces that stop above the far ground; on the lowest top of the terrain tiles under it."""
    for reach in LEVEL_REACH_CM:
        zs = squares.heights_near(ring, points, reach)
        if len(zs) >= LEVEL_MIN_HITS:
            return zs[len(zs) // 2], "furniture (%d within %d m)" % (len(zs), reach / 100.0)
    x0, y0, x1, y1 = squares.bounds([ring])
    top = max(t[5] for t in tiles) + 500.0
    hits = []
    y = y0
    while y <= y1:
        x = x0
        while x <= x1:
            if any(t[0] <= x <= t[2] and t[1] <= y <= t[3] for t in tiles) and squares.distance([ring], (x, y)) < -300.0:
                z = trace_z(world, x, y, top, base - 100.0)
                if z is not None and z > base + 50.0:
                    hits.append(z)
            x += LEVEL_GRID_CM
        y += LEVEL_GRID_CM
    if len(hits) >= LEVEL_MIN_HITS:
        hits.sort()
        return hits[len(hits) // 2], "trace (%d hits)" % len(hits)
    under = [t[5] for t in tiles if t[0] <= x1 and t[2] >= x0 and t[1] <= y1 and t[3] >= y0]
    if under:
        return min(under) - 50.0, "terrain bounds (%d furniture, %d traces)" % (len(zs), len(hits))
    return 0.0, "nothing measured"


def square_settings(report, place, tiles, built, base):  # noqa: C901
    """Where a square of landmarks.json runs past the map's ground (place Bellecour): the ground
    carried on at the square's level with the ground material, which draws the square's surface
    in world space; the fronts round the square; two rows of trees. See horizon.square_setting."""
    import route_trees
    import squares
    out = report.setdefault("square_settings", [])
    found = squares.load(os.path.join(REPO, "assets", "geo", "processed", ZONE, "landmarks.json"))
    if not found or not tiles:
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    ground_m = (min(t[0] for t in tiles) / 100.0, min(t[1] for t in tiles) / 100.0,
                max(t[2] for t in tiles) / 100.0, max(t[3] for t in tiles) / 100.0)
    report["ground_box_m"] = [round(v, 1) for v in ground_m]
    line, points = [], None
    circuit = os.path.join(REPO, "GranTheftPraline", "Content", "Test", "Circuits", "L_City_" + CAMEL + ".circuit.csv")
    if os.path.exists(circuit):
        with open(circuit, encoding="utf-8") as f:
            line = horizon.circuit_line(f.read())
    for q in found:
        name = "".join(w.capitalize() for w in q["key"].split("-"))
        st = horizon.square_setting([(x / 100.0, y / 100.0) for x, y in q["ring"]], ground_m, SEED,
                                    [tuple(v / 100.0 for v in b) for b in built])
        info = {"key": q["key"], "box_m": st["box"], "aprons": 0, "fronts": 0, "trees": 0}
        out.append(info)
        if not st["aprons"]:
            info["skipped"] = "on the map's ground"
            continue
        if points is None:
            points = ground_points()
        level, how = square_level(world, squares, q["ring"], tiles, base, points)
        info["level_cm"], info["level_from"] = round(level, 1), how
        for i, a in enumerate(st["aprons"]):
            # a cube, 1 m thick, its top the ground: the engine's cube has a collision box for certain
            actor = place("Cube", "Horizon_%s_Ground_%d" % (name, i),
                          (a["x"] * 100.0, a["y"] * 100.0, level - APRON_BELOW_CM - 50.0),
                          (a["width"], a["depth"], 1.0), GROUND_MATERIAL)
            solid(actor.static_mesh_component)
            info["aprons"] += 1
        for i, b in enumerate(st["fronts"]):
            top = level + b["top"] * 100.0
            actor = place("Cube", "Horizon_%s_Front_%02d" % (name, i),
                          (b["x"] * 100.0, b["y"] * 100.0, 0.5 * (base + top)),
                          (b["width"], b["depth"], (top - base) / 100.0), "M_City_Horizon_%d" % b["colour"], b["yaw"])
            solid(actor.static_mesh_component)
            actor.static_mesh_component.set_editor_property("cast_shadow", True)    # the sun is behind them
            info["fronts"] += 1
        trees = horizon.keep_clear(st["trees"], line, TREE_CLEAR_M)
        info["trees_left_off_the_line"] = len(st["trees"]) - len(trees)
        paths = ([route_trees.MAPLES["square"]] if ROUTE_MAPLES else []) + route_trees.ROUTE_TREES["mature"].split("|")
        mesh = next((eal.load_asset(p) for p in paths if eal.does_asset_exist(p)), None)
        if mesh is None:
            info["trees_skipped"] = "no leafy tree mesh in the project"
            continue
        actor = actor_sub_().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0))
        actor.set_actor_label("Horizon_%s_Trees" % name)
        actor.set_folder_path(FOLDER)
        try:
            actor.set_editor_property("is_spatially_loaded", False)    # seen from across the square
        except Exception:  # noqa: BLE001  plain level: no such property to set
            pass
        sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        sdl = unreal.SubobjectDataBlueprintFunctionLibrary
        root = sds.k2_gather_subobject_data_for_instance(actor)[0]
        handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
        comp = sdl.get_object(sdl.get_data(handle))
        comp.set_static_mesh(mesh)
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        comp.set_collision_profile_name("BlockAll")
        comp.set_editor_property("instance_start_cull_distance", int(0.8 * TREE_CULL_CM))
        comp.set_editor_property("instance_end_cull_distance", int(TREE_CULL_CM))
        tall = mesh_height(mesh)
        size = route_trees.SQUARE_TREE_CM / tall if tall > 0.0 else 1.0    # whatever the mesh, trees of 16 m
        info["tree_mesh"], info["tree_mesh_height_cm"] = mesh.get_name(), round(tall)
        for t in trees:
            # at the level of the square, a hand deep: no trace tells the ground here (see square_level)
            k = t["scale"] * size
            comp.add_instance(unreal.Transform(unreal.Vector(t["x"] * 100.0, t["y"] * 100.0, level - TREE_SUNK_CM),
                                               unreal.Rotator(0.0, 0.0, t["yaw"]), unreal.Vector(k, k, k)), True)
            info["trees"] += 1


def mesh_height(mesh):
    box = mesh.get_bounding_box()
    return box.max.z - box.min.z


def route_maples():
    """The trees of the route (folder City/Route) become Norway maples where the pack is in the
    project: same places, same turns, the mean height kept (route_trees.rescale). The base map
    keeps its alders, so this runs at each candidate; a machine without the pack keeps them."""
    import route_trees
    out = {"on": ROUTE_MAPLES, "swapped": [], "missing": []}
    if not ROUTE_MAPLES:
        return out
    for actor in actor_sub_().get_all_level_actors():
        if not str(actor.get_folder_path()).startswith(ROUTE_FOLDER):
            continue
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            old = comp.get_editor_property("static_mesh")
            kind = route_trees.kind_of(old.get_name()) if old is not None else None
            if kind is None or comp.get_instance_count() == 0:
                continue
            path = route_trees.MAPLES[kind]
            if not eal.does_asset_exist(path):
                if path not in out["missing"]:
                    out["missing"].append(path)
                continue
            new = eal.load_asset(path)
            if new is None:
                out["missing"].append(path + " (does not load)")
                continue
            found = [comp.get_instance_transform(i, True) for i in range(comp.get_instance_count())]
            mean = sum(abs(t.scale3d.z) for t in found) / len(found)
            old_tall, new_tall = mesh_height(old), mesh_height(new)
            k = route_trees.rescale(kind, old_tall, new_tall, mean)
            comp.clear_instances()
            comp.set_static_mesh(new)
            try:
                comp.set_editor_property("override_materials", [])     # the alder's, if any were set
            except Exception:  # noqa: BLE001
                pass
            for t in found:
                s = t.scale3d
                comp.add_instance(unreal.Transform(t.translation, t.rotation.rotator(),
                                                   unreal.Vector(s.x * k, s.y * k, s.z * k)), True)
            out["swapped"].append({"actor": actor.get_actor_label(), "from": old.get_name(), "to": new.get_name(),
                                   "trees": len(found), "kept": comp.get_instance_count(),
                                   "mean_height_cm": [round(old_tall * mean), round(new_tall * mean * k)],
                                   "scale_factor": round(k, 3)})
    out["trees"] = sum(e["trees"] for e in out["swapped"])
    return out


def actor_sub_():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def main():  # noqa: C901
    t0 = time.time()
    report = {"map": MAP, "errors": [], "removed": 0, "blocks": 0, "towers": 0, "hills": 0, "skirt": 0,
              "monuments": 0}
    try:
        level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not level_sub.load_level(MAP):
            report["errors"].append("cannot open " + MAP)
            raise StopIteration
        lowest, tiles, built = None, [], []
        for a in actor_sub.get_all_level_actors():
            folder = str(a.get_folder_path())
            if folder.startswith(FOLDER):
                actor_sub.destroy_actor(a)
                report["removed"] += 1
            elif folder.startswith(TERRAIN_FOLDER):
                origin, extent = a.get_actor_bounds(False)
                bottom = origin.z - extent.z
                lowest = bottom if lowest is None else min(lowest, bottom)
                tiles.append((origin.x - extent.x, origin.y - extent.y, origin.x + extent.x, origin.y + extent.y,
                              bottom, origin.z + extent.z))
            elif folder.startswith(BUILT_FOLDERS):
                origin, extent = a.get_actor_bounds(False)
                built.append((origin.x - extent.x, origin.y - extent.y, origin.x + extent.x, origin.y + extent.y))
        if not ENABLED:
            report["skipped"] = "GTP_HORIZON=0"
            if report["removed"]:
                level_sub.save_current_level()
            raise StopIteration
        base = (lowest if lowest is not None else FALLBACK_BASE_CM) - BELOW_TERRAIN_CM
        report["base_cm"] = round(base, 1)
        report["terrain_measured"] = lowest is not None

        shapes = {n: eal.load_asset("/Engine/BasicShapes/" + n) for n in ("Cube", "Plane", "Sphere")}
        names = ["M_City_Horizon_%d" % i for i in range(horizon.COLOURS)] + \
                ["M_City_HorizonGround", "M_City_HorizonWood", "M_City_HorizonTown"]
        mats = {n: eal.load_asset(MAT_DIR + "/" + n) if eal.does_asset_exist(MAT_DIR + "/" + n) else None
                for n in names}
        missing = [n for n, m in shapes.items() if m is None] + [n for n, m in mats.items() if m is None]
        if missing:
            report["errors"].append("missing (run the materials step first): " + ", ".join(missing))
            raise StopIteration
        # The monuments are a later addition: a project whose materials step is older, or an engine
        # without these two shapes, still gets the rest of the far view.
        for n in ("Cylinder", "Cone"):
            shapes[n] = eal.load_asset("/Engine/BasicShapes/" + n)
        for n in list(MONUMENT_MATERIALS.values()) + [GROUND_MATERIAL]:
            mats[n] = eal.load_asset(MAT_DIR + "/" + n) if eal.does_asset_exist(MAT_DIR + "/" + n) else None

        def place(shape, label, loc_cm, scale, mat, yaw=0.0):
            a = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc_cm),
                                                 unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            a.set_actor_label(label)
            a.set_folder_path(FOLDER)
            comp = a.static_mesh_component
            comp.set_static_mesh(shapes[shape])
            comp.set_material(0, mats[mat])
            a.set_actor_scale3d(unreal.Vector(*scale))
            quiet(comp, report)
            try:
                a.set_editor_property("is_spatially_loaded", False)    # a partitioned map must not unload it
            except Exception:  # noqa: BLE001  plain level: no such property to set
                pass
            return a

        plan = horizon.layout(SEED)
        # Basic shapes are 100 cm across, centred: a scale of 1 is 1 m.
        side = 2.0 * plan["skirt"]["half"]
        place("Plane", "Horizon_Ground", (0.0, 0.0, base), (side, side, 1.0), "M_City_HorizonGround")
        report["skirt"] = 1
        for h in plan["hills"]:
            height_cm = h["top"] * 100.0 - base
            place("Sphere", "Horizon_Hill_" + h["name"], (h["x"] * 100.0, h["y"] * 100.0, base),
                  (2.0 * h["half_x"], 2.0 * h["half_y"], 2.0 * height_cm / 100.0),
                  "M_City_HorizonWood" if h["wooded"] else "M_City_HorizonTown")
            report["hills"] += 1
        for i, b in enumerate(plan["blocks"]):
            foot = (b["ground"] - HILL_FOOT_M) * 100.0 if b["ground"] > 0.0 else base
            top = b["top"] * 100.0
            place("Cube", "Horizon_%s_%03d" % (b["kind"].capitalize(), i),
                  (b["x"] * 100.0, b["y"] * 100.0, 0.5 * (foot + top)),
                  (b["width"], b["depth"], (top - foot) / 100.0), "M_City_Horizon_%d" % b["colour"], b["yaw"])
            report["towers" if b["kind"] == "tower" else "blocks"] += 1
        for m in plan["monuments"]:
            mat = MONUMENT_MATERIALS[m["material"]]
            if shapes.get(m["shape"]) is None or mats.get(mat) is None:
                report.setdefault("monuments_skipped", []).append(m["name"])
                continue
            foot = (m["ground"] - horizon.MONUMENT_FOOT_M) * 100.0
            top = m["top"] * 100.0
            place(m["shape"], "Horizon_" + m["name"], (m["x"] * 100.0, m["y"] * 100.0, 0.5 * (foot + top)),
                  (m["width"], m["depth"], (top - foot) / 100.0), mat, m["yaw"])
            report["monuments"] += 1
        if SQUARE_SETTING and mats.get(GROUND_MATERIAL) is not None:
            try:
                square_settings(report, place, tiles, built, base)
            except Exception as e:  # noqa: BLE001  the far view is placed: a square's setting never loses it
                report["square_setting_error"] = repr(e)
        try:
            report["route_maples"] = route_maples()
        except Exception as e:  # noqa: BLE001  the far view is placed: the route keeps the trees it has left
            report["route_maples_error"] = repr(e)
        level_sub.save_current_level()
    except StopIteration:
        pass
    except Exception as e:  # noqa: BLE001  report, never leave the editor on a dialog
        report["errors"].append(repr(e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "horizon_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    unreal.log_warning("[Horizon] " + json.dumps(report))


main()
