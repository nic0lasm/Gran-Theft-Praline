#!/usr/bin/env python3
"""Interior layout and object budget (docs/gameplay/reference-interior.md, ADR-0012).

Reads an interior spec (rooms, work stations, dining, fixtures) and the mesh
catalogue, places every object deterministically from a seed, and reports what
it costs: objects the player perceives, instances actually drawn (kits show a
stack of N items as one mesh), unique meshes, draw calls with one instanced
component per mesh and level, shadow draws and triangles after LOD, for the M1
tier (no Nanite) and the PC tier (Nanite). Fails when a budget is exceeded, so
the CI keeps the interior affordable before any Unreal session.

    python3 -I tools/interiors/interior_layout.py tools/interiors/fastfood_bellecour.json \
        --density 2 --tier m1 --csv out/placements.csv --report out/report.json

The CSV (centimetres, Unreal axes: y flipped) is what step 2 imports into the
interior level instance, one instanced static mesh component per class and level.
"""

import argparse
import csv
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CATALOG = os.path.join(HERE, "catalog.json")

WORKTOP_Z = 0.9
# Share of the small and micro items of the work stations kept at each density level:
# 0 = equipment and furniture only, 1 = half stocked, 2 and 3 = fully stocked.
STOCK_SHARE = (0.0, 0.5, 1.0, 1.0)
MAX_DENSITY = 3
CEILING_CLASSES = {"ceiling_tile", "ceiling_light", "sprinkler", "vent_grille"}
WALL_CLASSES = {"wall_socket", "fire_extinguisher", "wall_menu_screen", "hand_dryer"}


def load_json(path):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def zone_map(spec):
    return {zone["name"]: zone for zone in spec["zones"]}


def rect_area(rect):
    return max(0.0, rect[2] - rect[0]) * max(0.0, rect[3] - rect[1])


def grid_points(rect, count, margin=0.5):
    """count points spread on a regular grid inside rect (row major)."""
    if count <= 0:
        return []
    x0, y0, x1, y1 = rect[0] + margin, rect[1] + margin, rect[2] - margin, rect[3] - margin
    width, depth = max(x1 - x0, 0.1), max(y1 - y0, 0.1)
    cols = max(1, int(math.ceil(math.sqrt(count * width / depth))))
    rows = int(math.ceil(count / cols))
    points = []
    for index in range(count):
        col, row = index % cols, index // cols
        points.append((x0 + (col + 0.5) * width / cols, y0 + (row + 0.5) * depth / rows))
    return points


class Layout:
    def __init__(self, spec, catalog, density, seed):
        self.spec = spec
        self.catalog = catalog
        self.density = density
        self.rng = random.Random(seed)
        self.zones = zone_map(spec)
        self.storey = spec["frame"]["storey_height"]
        self.placements = []

    def add(self, cls, zone, x, y, z, yaw=0.0, station="", min_density=0):
        if cls not in self.catalog["classes"]:
            raise KeyError(f"class '{cls}' is missing from the catalogue")
        level = self.zones[zone]["level"]
        self.placements.append({
            "class": cls, "zone": zone, "level": level, "station": station,
            "x": round(x, 3), "y": round(y, 3), "z": round(z + level * self.storey, 3), "yaw": round(yaw, 1),
            "min_density": min_density,
        })

    def jitter(self, amount):
        return self.rng.uniform(-amount, amount)

    def stations(self):
        by_zone = {}
        for station in self.spec["stations"]:
            by_zone.setdefault(station["zone"], []).extend([station] * station["count"])
        for zone, stations in by_zone.items():
            rect = self.zones[zone]["rect"]
            for index, (station, point) in enumerate(zip(stations, grid_points(rect, len(stations)))):
                tag = f"{station['name']}_{index}"
                for cls, full_count in station["items"].items():
                    size = self.catalog["classes"][cls]["size"]
                    stock = size not in ("large", "medium")
                    z = 0.0 if size == "large" else WORKTOP_Z
                    for item in range(full_count):
                        need = min_level(item, [int(round(full_count * share)) for share in STOCK_SHARE]) if stock else 0
                        spread = 0.0 if size == "large" else 0.35
                        self.add(cls, zone, point[0] + self.jitter(spread), point[1] + self.jitter(spread),
                                 z + (0.0 if size == "large" else 0.02 * (item % 5)), self.rng.choice((0, 90, 180, 270)), tag, need)

    def dining(self):
        clutter = self.spec["table_clutter"]
        groups = {}
        for group in self.spec["dining"]:
            groups.setdefault(group["zone"], []).extend([group] * group["count"])
        for zone, tables in groups.items():
            for index, (group, point) in enumerate(zip(tables, grid_points(self.zones[zone]["rect"], len(tables), 0.8))):
                tag = f"{group['table']}_{index}"
                self.add(group["table"], zone, point[0], point[1], 0.0, 0.0, tag)
                seat = group.get("seat", "chair")
                seats = 2 if seat == "booth_bench" else group["seats"]
                for seat_index in range(seats):
                    angle = 2 * math.pi * seat_index / seats
                    self.add(seat, zone, point[0] + 0.6 * math.cos(angle), point[1] + 0.6 * math.sin(angle), 0.0,
                             math.degrees(angle) + 180.0, tag)
                if self.rng.random() < clutter["used_share"]:
                    for item in range(clutter["per_table"][-1]):
                        cls = clutter["items"][item % len(clutter["items"])]
                        self.add(cls, zone, point[0] + self.jitter(0.3), point[1] + self.jitter(0.3), 0.75, self.rng.uniform(0, 360), tag,
                                 min_level(item, clutter["per_table"]))
                    for item in range(clutter["fry_singles"][-1]):
                        self.add("fry_single", zone, point[0] + self.jitter(0.4), point[1] + self.jitter(0.4), 0.75, self.rng.uniform(0, 360), tag,
                                 min_level(item, clutter["fry_singles"]))

    def kiosks(self):
        kiosks = self.spec["kiosks"]
        rect = self.zones[kiosks["zone"]]["rect"]
        for index in range(kiosks["count"]):
            x = rect[0] + (index + 0.5) * (rect[2] - rect[0]) / kiosks["count"]
            self.add("kiosk", kiosks["zone"], x, rect[1] + 0.4, 0.0, 0.0, f"kiosk_{index}")

    def fixtures(self):
        for cls, rule in self.spec["fixtures"].items():
            if "count" in rule:
                for x, y in grid_points(self.zones[rule["zone"]]["rect"], rule["count"]):
                    self.add(cls, rule["zone"], x, y, self.fixture_z(cls))
                continue
            for name, zone in self.zones.items():
                if "zones" in rule and name not in rule["zones"]:
                    continue
                count = int(round(rect_area(zone["rect"]) * rule["per_m2"]))
                for x, y in grid_points(zone["rect"], count, 0.0 if cls == "ceiling_tile" else 0.3):
                    self.add(cls, name, x, y, self.fixture_z(cls))

    def fixture_z(self, cls):
        if cls in CEILING_CLASSES:
            return self.storey - 0.4
        if cls in WALL_CLASSES:
            return 1.4 if cls != "wall_socket" else 0.3
        return 0.0

    def build(self):
        self.stations()
        self.dining()
        self.kiosks()
        self.fixtures()
        return self.placements


def min_level(index, counts_by_level):
    """Lowest density level whose count includes item number index."""
    for level, count in enumerate(counts_by_level):
        if index < count:
            return level
    return len(counts_by_level)


def generate_all(spec, catalog, seed=1):
    """The full layout (density 3), each object tagged with the lowest density that shows it.

    One layout for every density: the game switches density by hiding the components of the
    higher levels (-InteriorDensity=), so an object never moves when the density changes.
    """
    return Layout(spec, catalog, MAX_DENSITY, seed).build()


def generate(spec, catalog, density, seed=1):
    if not 0 <= density <= MAX_DENSITY:
        raise ValueError("density goes from 0 to 3")
    return [p for p in generate_all(spec, catalog, seed) if p["min_density"] <= density]


def report(placements, catalog, tier, budgets):
    classes = catalog["classes"]
    rules = catalog["size_rules"]
    per_class = {}
    components = set()
    for placement in placements:
        per_class[placement["class"]] = per_class.get(placement["class"], 0) + 1
        components.add((placement["class"], placement["level"], placement["min_density"]))
    logical = sum(classes[cls]["items_per_instance"] * count for cls, count in per_class.items())
    draws = sum(classes[cls]["sections"] for cls, _level, _density in components)
    shadow_key = "shadow_m1" if tier == "m1" else "shadow_pc"
    shadow_draws = sum(classes[cls]["sections"] for cls, _level, _density in components if rules[classes[cls]["size"]][shadow_key])
    tris_lod0 = sum(classes[cls]["tris_lod0"] * count for cls, count in per_class.items())
    tris_avg = sum(classes[cls]["tris_lod0"] * rules[classes[cls]["size"]]["lod_factor"] * count for cls, count in per_class.items())
    interactive = sum(count for cls, count in per_class.items() if classes[cls]["interactive"])
    result = {
        "tier": tier,
        "objects_perceived": logical,
        "rendered_instances": len(placements),
        "interactive_instances": interactive,
        "unique_meshes": len(per_class),
        "instanced_components": len(components),
        "draw_calls": draws,
        "shadow_draw_calls": shadow_draws,
        "triangles_lod0": int(tris_lod0),
        "triangles_lod_avg": int(tris_avg),
        "per_class": dict(sorted(per_class.items(), key=lambda item: -item[1])),
    }
    budget = budgets[tier]
    over = {}
    for key, limit in budget.items():
        if key in result and result[key] > limit:
            over[key] = {"value": result[key], "limit": limit}
    result["budget"] = budget
    result["over_budget"] = over
    return result


def write_csv(path, placements, catalog):
    rules = catalog["size_rules"]
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["class", "zone", "level", "station", "x_cm", "y_cm", "z_cm", "yaw",
                         "ism_group", "min_density", "cull_cm", "shadow_m1", "shadow_pc", "interactive", "items"])
        for placement in placements:
            info = catalog["classes"][placement["class"]]
            rule = rules[info["size"]]
            writer.writerow([
                placement["class"], placement["zone"], placement["level"], placement["station"],
                round(placement["x"] * 100, 1), round(-placement["y"] * 100, 1), round(placement["z"] * 100, 1), placement["yaw"],
                f"{placement['class']}@L{placement['level']}@D{placement['min_density']}", placement["min_density"], rule["cull_m"] * 100,
                int(rule["shadow_m1"]), int(rule["shadow_pc"]), int(info["interactive"]), info["items_per_instance"],
            ])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("spec")
    parser.add_argument("--catalog", default=DEFAULT_CATALOG)
    parser.add_argument("--density", type=int, default=None, help="0..3, default from the spec per tier")
    parser.add_argument("--tier", choices=("m1", "pc"), default="m1")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--csv")
    parser.add_argument("--report")
    args = parser.parse_args(argv)

    spec = load_json(args.spec)
    catalog = load_json(args.catalog)
    density = args.density if args.density is not None else spec["density"]["default_" + args.tier]
    placements = generate(spec, catalog, density, args.seed)
    result = report(placements, catalog, args.tier, spec["budgets"])
    result["density"] = density
    if args.csv:
        write_csv(args.csv, placements, catalog)
    if args.report:
        os.makedirs(os.path.dirname(os.path.abspath(args.report)), exist_ok=True)
        with open(args.report, "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=1)
    summary = {key: value for key, value in result.items() if key not in ("per_class", "budget")}
    print(json.dumps(summary, indent=1))
    return 1 if result["over_budget"] else 0


if __name__ == "__main__":
    sys.exit(main())
