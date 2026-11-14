"""Place the ZoneGraph of the generated city map (ADR-0015, Mass crowd and traffic), inside the
Unreal Editor, the same way as place_props.py (no UI, run by run_city_import.sh).

Input: gen/<focus>/zonegraph/zonegraph.json (tools/city/zonegraph.py: driving lanes, junction
polygons, walkways, crossings). The C++ importer UGTPZoneGraphImport (module GTPMass) places one
ZoneShape actor per shape in folder City/ZoneGraph, after removing the previous ones, and adds
the missing ZoneGraph tags and lane profiles to the settings in memory: our tags keep fixed bits
(Car 1, Intersection 2, Pedestrian 3, Crossing 4), the ones MassTraffic's lane filters use. The
lanes are then built from the shapes (UGTPZoneGraphImport::BuildZoneGraphData: a ZoneGraphData
actor in the map, the engine's builder run) and the map is saved: nobody edits the ZoneGraph by
hand. Report: Saved/CityImport/zonegraph_report.json ("build": shapes, lanes, zones).
"""

import json
import os
import time

import unreal

REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
ZONE = os.environ.get("GTP_CITY_ZONE", "presqu-ile")
FOCUS = os.environ.get("GTP_CITY_FOCUS", "rue-de-la-republique")
GEN = os.path.join(REPO, "assets", "geo", "processed", ZONE, "gen", FOCUS)
MAP = "/Game/Maps/L_City_" + "".join(p.capitalize() for p in FOCUS.split("-"))

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor_sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(msg):
    unreal.log_warning("[CityZoneGraph] " + msg)


def write(report, t0):
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "CityImport")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "zonegraph_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    log("report: " + json.dumps(report))


def main():
    t0 = time.time()
    report = {"map": MAP, "errors": []}
    path = os.path.join(GEN, "zonegraph", "zonegraph.json")
    if not os.path.exists(path):
        report["errors"].append("no zonegraph.json: run tools/city/zonegraph.py")
        return write(report, t0)
    importer = getattr(unreal, "GTPZoneGraphImport", None)
    if importer is None:
        report["errors"].append("module GTPMass not loaded (build the editor target)")
        return write(report, t0)
    level_sub.load_level(MAP)
    world = editor_sub.get_editor_world()
    result = json.loads(importer.build_from_file(world, path))
    report["counts"] = result.get("counts", {})
    report["info"] = result.get("info", {})
    report["errors"] += result.get("errors", [])
    # The engine builds ZoneGraph data by itself only with "build while editing" on: the importer
    # runs its builder, and the lanes are saved with the map (0 lanes: no MassTraffic car).
    build = json.loads(importer.build_zone_graph_data(world))
    report["build"] = build.get("counts", {})
    report["errors"] += build.get("errors", [])
    level_sub.save_current_level()
    write(report, t0)


main()
