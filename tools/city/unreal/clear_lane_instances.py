"""Take out of the street map the placed objects that stand on a traffic lane (Unreal Editor,
headless, like place_props.py). Gate 75: the stolen car stood 25 s against a white block on lane
13, rue de la Bourse (StaticMeshActor_27); three more objects stood on the flight's lanes.

Rule and geometry: tools/city/lane_clear.py (clear_lanes.py's rule on the map itself): an
instance whose footprint comes within GTP_LANE_CLEAR_M (1.5) of a lane centre line, or of the
link from a lane's end to the next lane, is removed, not moved: beside a lane there is a facade,
a doorway or another object, and a moved object would no longer stand where it was surveyed.

What is looked at: every instanced static mesh component (HISM, ISM) of the map, with collision,
taller than 15 cm (manhole covers and grates stay), whose lowest point is less than 2.5 m above
the lane (an overhead sign stays). Footprint: the mesh box turned by the instance's yaw; a tree
(mesh path with "Tree" or "Alder", not a grate) by a 40 cm trunk. Removed: the street furniture
and detail sets (City/Props, City/Route, City/Details, tag GTP_Set_*) and the actors named in
GTP_LANE_ACTORS (none by default). The facade kit (City/Facades) and the signs are only ever
reported, even when named: the kit has one actor per module mesh over the whole city, so gate 75's
StaticMeshActor_27 is every ShopClosed_B module of the map. Its white block was one module turned
round (fix_facade_flips.py), not an object to cut.

Where the filmed flight ends (rows marked "end" in Content/Test/Circuits/<Map>.circuit.csv), a
tree of those sets whose trunk is within GTP_END_TREE_CLEAR_M (8) of the driven line goes too:
the arrest is played there with the chase camera turning round the stopped car, and leaves have
no collision (gate 78, 80 s: the camera inside a tree). Such an instance has "lane": "end" in the
report. GTP_END_TREE_CLEAR_M=0 turns that off.

The flight does not drive the lane centres but the line of the circuit file (column
line_ue_cm, approach and lap), up to 3.9 m off them: gate 83, 19 s, the car hit a tree of the quay
at 96 km/h, "1.8 m off its lane, the obstacle 2.5 m off lane 20". So the same objects also go
within GTP_CIRCUIT_CLEAR_M (2.5: half a car and 1.3 m of driving off the line) of every line of
that file, rows joined to the next; such an instance has "lane": "circuit" in the report, which
gives circuit_lines, circuit_clear_cm and circuit_removed. GTP_CIRCUIT_CLEAR_M=0 turns that off.

Plain actors are looked at as well as instances (a static mesh actor with collision, at most
30 m wide: the octagonal kiosk of rue de la Republique stood 0.1 m off the driven line and
stopped the car at gates 85 and 86): same rule, same sets, the actor is destroyed; it is listed
in removed_actors and in removed_instances with "index": -1. One that belongs to no set (a
landmark) is only reported, with its folder and the middle of its box; buildings, facades and
the ground are not looked at this way, their box being no plan of them.

Before all that, a landmark the base map holds in the wrong place is moved (lane_clear.
LANDMARK_MOVES: the carrousel of place de la Republique, 15 m north, off the carriageway and
onto the square), and what stood where it now stands is taken out like what stands on a lane
("lane": "landmark"). The report gives landmarks_moved; GTP_LANDMARK_MOVES=0 leaves it.

After all that, what is left of the leafy trees of the sets (route_trees.is_leafy: the alders of
the route, whole trees with the collision of their own shape) stops nothing any more, and one
hidden post a tree stands for its trunk (actor Route_TreeTrunks, City/Route). Gate 91: the
stems of those alders lean over the carriageway and held the stolen car up six times while
their trunks stood clear of its line. The report gives tree_trunks (trees, posts, the actors
that carried them); GTP_TREE_TRUNKS=0 leaves the trees as they are.

GTP_LANE_CLEAR=0: report only, nothing removed, map not saved. Lanes: GTP_LANES_CSV, else
Content/City/Traffic/<Map>.lanes.csv (the file the game reads). Map: GTP_CITY_FOCUS as
place_props.py. Report: Saved/CityImport/lane_clear_report.json (removed per set and per mesh,
each removed instance with its lane and gap, what was only reported) and one log line
"[LaneClear] ...".
"""

import json
import os
import sys
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "city"))
import lane_clear as lc  # noqa: E402
import route_trees  # noqa: E402

FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
MAP_NAME = "L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))
MAP = "/Game/Maps/" + MAP_NAME
LANES = os.environ.get("GTP_LANES_CSV") or os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "City", "Traffic",
    MAP_NAME + ".lanes.csv")
APPLY = os.environ.get("GTP_LANE_CLEAR", "1") != "0"
CLEAR_CM = float(os.environ.get("GTP_LANE_CLEAR_M", "1.5")) * 100.0
NAMED = [n for n in os.environ.get("GTP_LANE_ACTORS", "").split(",") if n]
SET_FOLDERS = ("City/Props", "City/Route", "City/Details")
REPORT_ONLY_FOLDERS = ("City/Facades", "City/Signs")
MIN_HEIGHT_CM = 15.0
BOX_IS_NO_FOOTPRINT = ("City/Buildings", "City/Backdrop", "City/Terrain", "City/Horizon", "City/Riverside",
                       "City/CellProxies", "City/PitCovers", "City/Facades", "City/Signs")
MOVES = os.environ.get("GTP_LANDMARK_MOVES", "1") != "0"
ACTOR_MAX_CM = 3000.0       # a plain actor wider than this is ground, a building or a merged set
CIRCUIT = os.environ.get("GTP_CIRCUIT_CSV") or os.path.join(
    unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), "Test", "Circuits",
    MAP_NAME + ".circuit.csv")
CIRCUIT_CM = float(os.environ.get("GTP_CIRCUIT_CLEAR_M", str(lc.CIRCUIT_CLEAR_CM / 100.0))) * 100.0
END_TREE_CM = float(os.environ.get("GTP_END_TREE_CLEAR_M", str(lc.END_TREE_CLEAR_CM / 100.0))) * 100.0

TRUNKS = os.environ.get("GTP_TREE_TRUNKS", "1") != "0"
TRUNKS_LABEL = "Route_TreeTrunks"
TRUNKS_FOLDER = "City/Route"
CUBE = "/Engine/BasicShapes/Cube.Cube"       # 100 cm, centred on its origin

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log_warning("[LaneClear] " + msg)


def blocks(comp):
    try:
        return comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION
    except Exception:  # noqa: BLE001
        return True


def post_component():
    """One actor, one instanced cube that is not drawn in the game and stops everything (as the
    lids of cover_pits.py). Raises when it cannot be hidden: a white post in every tree is worse
    than the stems."""
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    sdl = unreal.SubobjectDataBlueprintFunctionLibrary
    actor = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    try:
        actor.set_actor_label(TRUNKS_LABEL)
        actor.set_folder_path(TRUNKS_FOLDER)
        root = sds.k2_gather_subobject_data_for_instance(actor)[0]
        handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
        comp = sdl.get_object(sdl.get_data(handle))
        comp.set_static_mesh(eal.load_asset(CUBE))
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        comp.set_collision_profile_name("BlockAll")
        try:
            comp.set_hidden_in_game(True)
        except Exception:  # noqa: BLE001  the property, when the call is not there
            comp.set_editor_property("hidden_in_game", True)
        for prop in ("cast_shadow", "visible_in_ray_tracing", "affect_distance_field_lighting"):
            try:
                comp.set_editor_property(prop, False)
            except Exception:  # noqa: BLE001  hidden already: these only spare the renderer
                pass
    except Exception:
        actor_sub.destroy_actor(actor)
        raise
    return actor, comp


def tree_trunks(actors, info):
    """The leafy trees of the sets (route_trees.is_leafy) stop nothing any more, and a hidden
    post stands for each trunk. Gate 91: the stems of the route's alders lean over the
    carriageway and held the stolen car up six times, their trunks clear of its line. The posts
    are laid first: if they fail, the trees are left as they were. True when the map changed."""
    posts, comps = [], []
    for actor in actors:
        if which_set(actor) is None or actor.get_actor_label() == TRUNKS_LABEL:
            continue
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            mesh = comp.get_editor_property("static_mesh")
            if mesh is None or not blocks(comp) or not route_trees.is_leafy(mesh.get_path_name()):
                continue
            box = mesh.get_bounding_box()
            tall = box.max.z - box.min.z
            count = comp.get_instance_count()
            for i in range(count):
                t = comp.get_instance_transform(i, True)
                posts.append(route_trees.trunk_post(t.translation.x, t.translation.y, t.translation.z,
                                                    t.scale3d.z, tall * abs(t.scale3d.z)))
            comps.append(comp)
            key = mesh.get_path_name().split(".")[0].rsplit("/", 1)[-1]
            info["by_mesh"][key] = info["by_mesh"].get(key, 0) + count
            info["actors"].append({"actor": actor.get_name(), "label": actor.get_actor_label(), "mesh": key,
                                   "trees": count})
    info["components"] = len(comps)
    info["trees"] = len(posts)
    if not posts or not APPLY:
        return False
    actor, post = post_component()
    try:
        for x, y, z, side, height in posts:
            post.add_instance(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0),
                                               unreal.Vector(side / 100.0, side / 100.0, height / 100.0)), True)
    except Exception:
        actor_sub.destroy_actor(actor)
        raise
    info["posts"] = post.get_instance_count()
    info["post_actor"] = actor.get_name()
    for comp in comps:
        comp.modify()
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        info["without_collision"] = info.get("without_collision", 0) + 1
    return True


def which_set(actor):
    folder = str(actor.get_folder_path())
    for f in SET_FOLDERS:
        if folder.startswith(f):
            return f.split("/")[1].lower()
    for t in actor.tags:
        if str(t).startswith("GTP_Set_"):
            return str(t)[8:]
    if actor.get_name() in NAMED and not folder.startswith(REPORT_ONLY_FOLDERS):
        return "named:" + actor.get_name()
    return None


def main():  # noqa: C901
    t0 = time.time()
    report = {"map": MAP, "lanes_file": LANES, "clear_cm": CLEAR_CM, "apply": APPLY, "named": NAMED,
              "removed": {}, "removed_by_mesh": {}, "removed_instances": [], "removed_actors": [],
              "reported_only": [], "landmarks_moved": [],
              "tree_trunks": {"on": TRUNKS, "components": 0, "trees": 0, "by_mesh": {}, "actors": []},
              "named_found": [], "errors": []}
    try:
        if not os.path.exists(LANES):
            report["errors"].append("no lanes file " + LANES)
            raise StopIteration
        lanes = lc.read_lanes(LANES)
        index = lc.LaneIndex(lanes, CLEAR_CM)
        report["lanes"] = len(lanes)
        driven = lc.read_lines(CIRCUIT) if CIRCUIT_CM > 0.0 else []
        circuit = lc.circuit_index(driven, CIRCUIT_CM) if driven else None
        report["circuit_lines"] = len(driven)
        report["circuit_clear_cm"] = CIRCUIT_CM
        report["circuit_removed"] = 0
        end_lines = lc.read_end_lines(CIRCUIT) if END_TREE_CM > 0.0 else []
        report["end_lines"] = len(end_lines)
        report["end_tree_clear_cm"] = END_TREE_CM
        if not level_sub.load_level(MAP):
            report["errors"].append("cannot open " + MAP)
            raise StopIteration
        actors = list(actor_sub.get_all_level_actors())
        moved_boxes = []
        for actor in actors if MOVES else []:
            if not isinstance(actor, unreal.StaticMeshActor) or actor.get_actor_label() not in lc.LANDMARK_MOVES:
                continue
            comp = actor.static_mesh_component
            mesh = comp.get_editor_property("static_mesh") if comp is not None else None
            if mesh is None:
                continue
            box = mesh.get_bounding_box()
            lo, hi = box.min, box.max
            t = actor.get_actor_transform()
            sc = t.scale3d
            x, y, yaw = t.translation.x, t.translation.y, t.rotation.rotator().yaw
            was = lc.footprint_box(x, y, yaw, (lo.x, lo.y), (hi.x, hi.y), (sc.x, sc.y))
            centre = (sum(p[0] for p in was) / 4.0, sum(p[1] for p in was) / 4.0)
            move = lc.landmark_move(actor.get_actor_label(), centre)
            entry = {"actor": actor.get_name(), "label": actor.get_actor_label(),
                     "from": [round(centre[0]), round(centre[1])], "moved": move is not None}
            if move is not None:
                dx, dy, dz = move
                entry["to"] = [round(centre[0] + dx), round(centre[1] + dy)]
                entry["raised_cm"] = dz
                moved_boxes.append(lc.footprint_box(x + dx, y + dy, yaw, (lo.x, lo.y), (hi.x, hi.y), (sc.x, sc.y)))
                if APPLY:
                    actor.modify()
                    actor.set_actor_location(unreal.Vector(x + dx, y + dy, t.translation.z + dz), False, False)
            report["landmarks_moved"].append(entry)
        for actor in actors:
            folder = str(actor.get_folder_path())
            if actor.get_name() in NAMED:
                report["named_found"].append({"name": actor.get_name(), "label": actor.get_actor_label(),
                                              "folder": folder})
            owner = which_set(actor)
            only_report = owner is None
            for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
                mesh = comp.get_editor_property("static_mesh")
                if mesh is None or not blocks(comp):
                    continue
                box = mesh.get_bounding_box()
                lo, hi = box.min, box.max
                path = mesh.get_path_name()
                tree = route_trees.is_tree(path)
                hits = []
                for i in range(comp.get_instance_count()):
                    t = comp.get_instance_transform(i, True)
                    s = t.scale3d
                    if (hi.z - lo.z) * abs(s.z) < MIN_HEIGHT_CM:
                        continue
                    x, y, z = t.translation.x, t.translation.y, t.translation.z
                    yaw = t.rotation.rotator().yaw
                    shape = ((x, y), lc.TRUNK_RADIUS_CM) if tree else \
                        lc.footprint_box(x, y, yaw, (lo.x, lo.y), (hi.x, hi.y), (s.x, s.y))
                    near = index.nearest(shape, z_low=z + lo.z * s.z)
                    if not near and circuit is not None:
                        c = circuit.nearest(shape)
                        if c:
                            near = ("circuit", c[1])
                    if not near and moved_boxes and lc.under((x, y), moved_boxes):
                        near = ("landmark", 0.0)        # where a landmark now stands
                    if not near and tree and end_lines:
                        g = lc.end_gap(x, y, end_lines)
                        if g < END_TREE_CM:
                            near = ("end", g)
                    if near:
                        hits.append((i, near, (round(x), round(y), round(z))))
                if not hits:
                    continue
                entry = [{"actor": actor.get_name(), "label": actor.get_actor_label(), "folder": folder,
                          "mesh": path.split(".")[0], "index": i, "lane": lane, "gap_cm": round(g),
                          "xyz": list(xyz)} for i, (lane, g), xyz in hits]
                if only_report or folder.startswith(REPORT_ONLY_FOLDERS):
                    report["reported_only"].extend(entry)
                    continue
                report["removed_instances"].extend(entry)
                report["circuit_removed"] += sum(1 for e in entry if e["lane"] == "circuit")
                report["removed"][owner] = report["removed"].get(owner, 0) + len(hits)
                key = path.split(".")[0].rsplit("/", 1)[-1]
                report["removed_by_mesh"][key] = report["removed_by_mesh"].get(key, 0) + len(hits)
                if APPLY:
                    comp.modify()
                    # highest index first: removing an instance moves the last one into its place
                    for i in sorted((h[0] for h in hits), reverse=True):
                        comp.remove_instance(i)
        # Plain actors too (one mesh, not instances): gates 85 and 86, the stolen car stood against
        # the octagonal kiosk of rue de la Republique (StaticMeshActor_766, 0.1 m off the driven
        # line) and was put back on its lane at place de l'Hopital (StaticMeshActor_781, 0.8 m).
        doomed = []
        for actor in actors:
            if not isinstance(actor, unreal.StaticMeshActor):
                continue
            comp = actor.static_mesh_component
            mesh = comp.get_editor_property("static_mesh") if comp is not None else None
            if mesh is None or isinstance(comp, unreal.InstancedStaticMeshComponent) or not blocks(comp):
                continue
            folder = str(actor.get_folder_path())
            if folder.startswith(BOX_IS_NO_FOOTPRINT):
                continue                    # a building's box is not its plan: 11 false alarms at prep 88
            box = mesh.get_bounding_box()
            lo, hi = box.min, box.max
            t = actor.get_actor_transform()
            s = t.scale3d
            wide = max((hi.x - lo.x) * abs(s.x), (hi.y - lo.y) * abs(s.y))
            if (hi.z - lo.z) * abs(s.z) < MIN_HEIGHT_CM or wide > ACTOR_MAX_CM:
                continue                    # flat on the ground, or a tile, a block, a merged set
            x, y, z = t.translation.x, t.translation.y, t.translation.z
            shape = lc.footprint_box(x, y, t.rotation.rotator().yaw, (lo.x, lo.y), (hi.x, hi.y), (s.x, s.y))
            near = index.nearest(shape, z_low=z + lo.z * s.z)
            if not near and circuit is not None:
                c = circuit.nearest(shape)
                if c:
                    near = ("circuit", c[1])
            if not near:
                continue
            path = mesh.get_path_name()
            entry = {"actor": actor.get_name(), "label": actor.get_actor_label(), "folder": folder,
                     "mesh": path.split(".")[0], "index": -1, "lane": near[0], "gap_cm": round(near[1]),
                     "xyz": [round(sum(p[0] for p in shape) / 4.0), round(sum(p[1] for p in shape) / 4.0),
                             round(z + lo.z * s.z)],          # the middle of its box: such a mesh is often
                     "wide_cm": round(wide)}                  # modelled in place, its actor at the origin
            owner = which_set(actor)
            if owner is None or folder.startswith(REPORT_ONLY_FOLDERS):
                report["reported_only"].append(entry)
                continue
            report["removed_instances"].append(entry)
            report["removed_actors"].append(entry)
            report["circuit_removed"] += 1 if near[0] == "circuit" else 0
            report["removed"][owner] = report["removed"].get(owner, 0) + 1
            key = path.split(".")[0].rsplit("/", 1)[-1]
            report["removed_by_mesh"][key] = report["removed_by_mesh"].get(key, 0) + 1
            doomed.append(actor)
        if APPLY:
            for actor in doomed:
                actor_sub.destroy_actor(actor)
        report["removed_actors_total"] = len(report["removed_actors"])
        report["removed_total"] = sum(report["removed"].values())
        report["reported_only_total"] = len(report["reported_only"])
        trunks = False
        if TRUNKS:
            try:        # after the removals: what is left of the trees
                trunks = tree_trunks(actor_sub.get_all_level_actors(), report["tree_trunks"])
            except Exception as e:  # noqa: BLE001  the trees stay as they were, the step goes on
                report["tree_trunks"]["error"] = repr(e)
        if APPLY and (report["removed_total"] or trunks or any(m["moved"] for m in report["landmarks_moved"])):
            level_sub.save_current_level()
            report["saved"] = True
    except StopIteration:
        pass
    except Exception as e:  # noqa: BLE001  report, never leave the editor on a dialog
        report["errors"].append(repr(e))
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "lane_clear_report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    log(json.dumps({k: report.get(k) for k in ("removed_total", "circuit_removed", "removed", "removed_by_mesh", "removed_actors_total",
                                                "landmarks_moved", "tree_trunks",
                                                "reported_only_total",
                                                "named_found", "errors", "saved")}))


main()
