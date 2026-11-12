"""Map data for the AI of the non-player characters, on a slice of the street (default: rue de
la Republique, Bellecour -> Jacobins) and a survey of the traboules of the centre.

Everything is written in the frame of the game map: Unreal centimetres, X east, Y south, Z up,
origin and altitude of processed/<zone>/manifest.json (as every gen/ file). Z comes from the
LiDAR ground model (MNT) where it covers the point, else it is left empty: the game traces down.

Inputs (not committed): raw/<zone>/osm_*.osm (+ --osm-extra folders of OSM XML tiles, e.g. the
Croix-Rousse slopes and Vieux Lyon), raw/<zone>/lidar_mnt.f32, processed/<zone>/shops.csv,
gen/<focus>/traffic/lanes.csv (traffic.py), gen/<focus>/pedestrians/walkways.csv
(pedestrians.py), gen/<facades>/facade_modules.csv (generate.py).

Outputs in --out (CSV, points "x y z;x y z;..." in Unreal cm; see the README written beside):
  walk_lines.csv     OSM footways, sidewalks, pedestrian streets, steps, paths (one row per way)
  walk_areas.csv     OSM pedestrian areas and squares (closed outlines)
  walkways.csv       the walkway graph the game already uses (pedestrians.py), clipped
  crossings.csv      pedestrian crossings: OSM nodes and crossing ways, control type, island
  lanes.csv          driving lanes with direction (traffic.py), clipped, + signals on each lane
  signals.csv        traffic lights, stop and give-way signs, with the nearest lane
  stops.csv          bus and tram stops, metro entrances, taxi ranks, bike-share stations
  shop_doors.csv     shops (OSM) with a door on the generated facade and its facing direction
  traboules.csv      traboules and covered passages (OSM), whole centre, with district
  summary.json       counts, bounding box, sources

  python3 tools/city/npc_data.py --out <dir> [--osm-extra <dir>] [--bbox X0,Y0,X1,Y1]
"""

import argparse
import csv
import glob
import json
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

import numpy as np
from shapely.geometry import LineString, Point, Polygon, box

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import citylib as cl  # noqa: E402
import raster  # noqa: E402

# Bellecour -> Jacobins slice in local metres (generate.py --clip), padded 40 m for the corners.
SLICE = (-120.0, -610.0, 340.0, -140.0)
WALK_HIGHWAYS = {"footway", "pedestrian", "path", "steps", "living_street", "corridor", "elevator"}
STOP_TAGS = [("highway", "bus_stop", "bus_stop"), ("railway", "tram_stop", "tram_stop"),
             ("railway", "subway_entrance", "metro_entrance"), ("amenity", "taxi", "taxi"),
             ("amenity", "bicycle_rental", "bike_share"), ("railway", "station", "station")]
TRABOULE_RE = re.compile(r"traboule|passage|cour des|galerie", re.I)


def load_osm(paths):
    nodes, ways, rels = {}, {}, {}
    for f in paths:
        for el in ET.parse(f).getroot():
            i = el.get("id")
            tags = {t.get("k"): t.get("v") for t in el.findall("tag")}
            if el.tag == "node":
                nodes[i] = (float(el.get("lon")), float(el.get("lat")), tags)
            elif el.tag == "way":
                ways[i] = ([n.get("ref") for n in el.findall("nd")], tags)
            elif el.tag == "relation":
                rels[i] = ([(m.get("type"), m.get("ref"), m.get("role")) for m in el.findall("member")], tags)
    return nodes, ways, rels


class Frame:
    def __init__(self, proc, raw):
        with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
            man = json.load(f)
        self.o = man["origin_lambert93_m"]
        self.alt = man["origin_altitude_m"]
        self.mnt = raster.Raster.load(raw, "mnt")

    def local(self, lon, lat):
        x, y = cl.lonlat_to_l93(lon, lat)
        return x - self.o[0], y - self.o[1]

    def ue(self, pts):
        """[(x, y) local m] -> [(X, Y, Z or None) Unreal cm]."""
        a = np.asarray(pts, float).reshape(-1, 2)
        z = self.mnt.sample(a[:, 0] + self.o[0], a[:, 1] + self.o[1]) - self.alt
        return [(round(x * 100, 1), round(-y * 100, 1), None if math.isnan(h) else round(h * 100, 1))
                for (x, y), h in zip(a, z)]


def fmt(pts):
    return ";".join("%g %g %s" % (x, y, "" if z is None else "%g" % z) for x, y, z in pts).replace(" ;", ";")


def way_local(frame, nodes, refs):
    return [frame.local(*nodes[r][:2]) for r in refs if r in nodes]


def write_csv(path, fields, rows):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fields)
        w.writeheader()
        w.writerows(rows)
    return len(rows)


def ue_poly_to_local(s):
    return [(float(p.split()[0]) / 100.0, -float(p.split()[1]) / 100.0) for p in s.split(";") if p.strip()]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique", help="gen/ folder with traffic/ and pedestrians/")
    ap.add_argument("--facades", default="rue-de-la-republique", help="gen/ folder with facade_modules.csv")
    ap.add_argument("--bbox", default=",".join("%g" % v for v in SLICE), help="slice, local metres X0,Y0,X1,Y1")
    ap.add_argument("--osm-extra", action="append", default=[], help="folder of extra OSM XML tiles (traboules)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    a = ap.parse_args()
    raw = os.path.join(a.geo, "raw", a.name)
    proc = os.path.join(a.geo, "processed", a.name)
    gen = os.path.join(proc, "gen", a.focus)
    os.makedirs(a.out, exist_ok=True)
    fr = Frame(proc, raw)
    zone = box(*[float(v) for v in a.bbox.split(",")])
    base = sorted(glob.glob(os.path.join(raw, "osm_*.osm")))
    extra = [p for d in a.osm_extra for p in sorted(glob.glob(os.path.join(d, "*.osm")))]
    nodes, ways, rels = load_osm(base + extra)
    counts = {}

    def node_in(i):
        return zone.contains(Point(fr.local(*nodes[i][:2])))

    # -- walk lines and areas ------------------------------------------------------------
    lines, areas = [], []
    for wid, (refs, t) in ways.items():
        hw, ah = t.get("highway"), t.get("area:highway")
        if hw not in WALK_HIGHWAYS and ah is None and t.get("place") != "square":
            continue
        pts = way_local(fr, nodes, refs)
        if len(pts) < 2:
            continue
        closed = refs[0] == refs[-1] and len(pts) >= 4
        is_area = closed and (t.get("area") == "yes" or ah is not None or t.get("place") == "square")
        geom = Polygon(pts) if is_area else LineString(pts)
        if not geom.is_valid or not zone.intersects(geom):
            continue
        row = {"osm": "way/" + wid, "highway": hw or ah or "square", "footway": t.get("footway", ""),
               "name": t.get("name", ""), "width_m": t.get("width", ""), "surface": t.get("surface", ""),
               "lit": t.get("lit", ""), "covered": t.get("covered", ""), "tunnel": t.get("tunnel", ""),
               "incline": t.get("incline", ""), "access": t.get("access", t.get("foot", "")),
               "points_ue_cm": fmt(fr.ue(pts))}
        (areas if is_area else lines).append(row)
    fields = ["osm", "highway", "footway", "name", "width_m", "surface", "lit", "covered", "tunnel", "incline",
              "access", "points_ue_cm"]
    counts["walk_lines"] = write_csv(os.path.join(a.out, "walk_lines.csv"), fields, lines)
    counts["walk_areas"] = write_csv(os.path.join(a.out, "walk_areas.csv"), fields, areas)

    # -- walkway graph of the game (pedestrians.py), clipped ------------------------------
    src = os.path.join(gen, "pedestrians", "walkways.csv")
    rows = []
    if os.path.exists(src):
        with open(src, encoding="utf-8") as f:
            for r in csv.DictReader(f):
                if zone.intersects(LineString(ue_poly_to_local(r["points_ue_cm"]))):
                    rows.append(r)
        counts["walkways"] = write_csv(os.path.join(a.out, "walkways.csv"), list(rows[0].keys()) if rows else
                                       ["edge_id"], rows)

    # -- crossings -------------------------------------------------------------------------
    sig_nodes = {i for i, n in nodes.items() if n[2].get("highway") == "traffic_signals"}
    crossings = []
    for i, (lon, lat, t) in nodes.items():
        if (t.get("highway") == "crossing" or "crossing" in t) and node_in(i):
            p = fr.local(lon, lat)
            crossings.append({"osm": "node/" + i, "geometry": "point", "control": t.get("crossing", ""),
                              "markings": t.get("crossing:markings", t.get("crossing_ref", "")),
                              "island": t.get("crossing:island", ""), "tactile": t.get("tactile_paving", ""),
                              "button": t.get("button_operated", ""), "points_ue_cm": fmt(fr.ue([p]))})
    for wid, (refs, t) in ways.items():
        if t.get("footway") == "crossing" or t.get("highway") == "crossing":
            pts = way_local(fr, nodes, refs)
            if len(pts) >= 2 and zone.intersects(LineString(pts)):
                ctl = t.get("crossing", "")
                if not ctl and any(r in sig_nodes for r in refs):
                    ctl = "traffic_signals"
                crossings.append({"osm": "way/" + wid, "geometry": "line", "control": ctl,
                                  "markings": t.get("crossing:markings", t.get("crossing_ref", "")),
                                  "island": t.get("crossing:island", ""), "tactile": t.get("tactile_paving", ""),
                                  "button": t.get("button_operated", ""), "points_ue_cm": fmt(fr.ue(pts))})
    counts["crossings"] = write_csv(os.path.join(a.out, "crossings.csv"),
                                    ["osm", "geometry", "control", "markings", "island", "tactile", "button",
                                     "points_ue_cm"], crossings)

    # -- lanes (traffic.py) and signals ----------------------------------------------------
    lanes = []
    src = os.path.join(gen, "traffic", "lanes.csv")
    if os.path.exists(src):
        with open(src, encoding="utf-8") as f:
            for r in csv.DictReader(f):
                ln = LineString(ue_poly_to_local(r["points_ue_cm"]))
                if zone.intersects(ln):
                    lanes.append((r, ln))
    signals = []
    for i, (lon, lat, t) in nodes.items():
        kind = t.get("highway")
        if kind not in ("traffic_signals", "stop", "give_way") or not node_in(i):
            continue
        p = Point(fr.local(lon, lat))
        near = sorted(((ln.distance(p), r["lane_id"], ln.project(p)) for r, ln in lanes))[:1]
        d, lane, along = near[0] if near and near[0][0] <= 15.0 else (None, "", None)
        signals.append({"osm": "node/" + i, "kind": kind,
                        "direction": t.get("traffic_signals:direction", t.get("direction", "")),
                        "for_crossing": "yes" if t.get("crossing") or "crossing" in t.get("traffic_signals", "")
                        else "", "lane_id": lane, "lane_dist_m": "" if d is None else round(d, 1),
                        "along_lane_m": "" if along is None else round(along, 1),
                        "points_ue_cm": fmt(fr.ue([(p.x, p.y)]))})
    counts["signals"] = write_csv(os.path.join(a.out, "signals.csv"),
                                  ["osm", "kind", "direction", "for_crossing", "lane_id", "lane_dist_m",
                                   "along_lane_m", "points_ue_cm"], signals)
    by_lane = {}
    for s in signals:
        if s["lane_id"] != "":
            by_lane.setdefault(s["lane_id"], []).append("%s@%s" % (s["kind"], s["along_lane_m"]))
    lane_rows = [dict(r, controls=" ".join(by_lane.get(r["lane_id"], []))) for r, _ln in lanes]
    counts["lanes"] = write_csv(os.path.join(a.out, "lanes.csv"),
                                (list(lanes[0][0].keys()) if lanes else ["lane_id"]) + ["controls"], lane_rows)

    # -- every OSM carriageway of the slice, with its direction (lanes.csv only has the BD TOPO
    # roads within 60 m of a generated building) ---
    car = {"motorway", "trunk", "primary", "secondary", "tertiary", "unclassified", "residential", "service",
           "living_street", "primary_link", "secondary_link", "tertiary_link"}
    roads = []
    for wid, (refs, t) in ways.items():
        if t.get("highway") not in car or t.get("area") == "yes":
            continue
        pts = way_local(fr, nodes, refs)
        if len(pts) < 2 or not zone.intersects(LineString(pts)):
            continue
        ow = t.get("oneway", "")
        if t.get("junction") == "roundabout" and not ow:
            ow = "yes"
        roads.append({"osm": "way/" + wid, "highway": t["highway"], "name": t.get("name", ""),
                      "oneway": {"yes": "1", "1": "1", "true": "1", "-1": "-1"}.get(ow, "0"),
                      "lanes": t.get("lanes", ""), "lanes_forward": t.get("lanes:forward", ""),
                      "lanes_backward": t.get("lanes:backward", ""), "maxspeed": t.get("maxspeed", ""),
                      "access": t.get("motor_vehicle", t.get("access", "")), "bus_lane": t.get("busway", t.get(
                          "lanes:bus", "")), "tunnel": t.get("tunnel", ""), "bridge": t.get("bridge", ""),
                      "points_ue_cm": fmt(fr.ue(pts))})
    counts["osm_roads"] = write_csv(os.path.join(a.out, "osm_roads.csv"),
                                    ["osm", "highway", "name", "oneway", "lanes", "lanes_forward", "lanes_backward",
                                     "maxspeed", "access", "bus_lane", "tunnel", "bridge", "points_ue_cm"], roads)

    # -- stops -----------------------------------------------------------------------------
    routes_of = {}
    for rid, (members, t) in rels.items():
        if t.get("type") == "route" and t.get("route") in ("bus", "tram", "subway", "trolleybus"):
            for typ, ref, _role in members:
                routes_of.setdefault((typ, ref), set()).add("%s %s" % (t.get("route"), t.get("ref", "?")))
    stops = []
    for i, (lon, lat, t) in nodes.items():
        kind = next((k for key, val, k in STOP_TAGS if t.get(key) == val), None)
        if kind is None and t.get("public_transport") == "platform":
            kind = "platform"
        if kind is None or not node_in(i):
            continue
        p = fr.local(lon, lat)
        stops.append({"osm": "node/" + i, "kind": kind, "name": t.get("name", ""),
                      "lines": " | ".join(sorted(routes_of.get(("node", i), set()))),
                      "shelter": t.get("shelter", ""), "capacity": t.get("capacity", ""),
                      "points_ue_cm": fmt(fr.ue([p]))})
    counts["stops"] = write_csv(os.path.join(a.out, "stops.csv"),
                                ["osm", "kind", "name", "lines", "shelter", "capacity", "points_ue_cm"], stops)

    # -- shop doors on the generated facade -------------------------------------------------
    doors = []
    mods = []
    fpath = os.path.join(proc, "gen", a.facades, "facade_modules.csv")
    if os.path.exists(fpath):
        with open(fpath, encoding="utf-8") as f:
            mods = [r for r in csv.DictReader(f) if r["storey"] == "0" and r["side"] == "street"
                    and r["kind"] in ("shop", "shopfront_closed", "entrance")]
    entrances = [(i, Point(fr.local(*nodes[i][:2])), nodes[i][2]) for i, n in nodes.items()
                 if "entrance" in n[2] and node_in(i)]
    with open(os.path.join(proc, "shops.csv"), encoding="utf-8") as f:
        shops = [s for s in csv.DictReader(f) if zone.contains(Point(float(s["x_m"]), float(s["y_m"])))]
    for s in shops:
        p = Point(float(s["x_m"]), float(s["y_m"]))
        cand = [m for m in mods if m["kind"] != "entrance"]
        best = min(cand, key=lambda m: math.hypot(float(m["x_m"]) - p.x, float(m["y_m"]) - p.y), default=None)
        d = math.hypot(float(best["x_m"]) - p.x, float(best["y_m"]) - p.y) if best else 1e9
        ent = min(entrances, key=lambda e: e[1].distance(p), default=None)
        if best is not None and d <= 20.0:
            yaw = float(best["ue_yaw"])
            # ue_yaw is the module's outward direction (generate.ue_yaw); the pivot is on the facade
            # plane: the door point stands 0.6 m outside it, on the pavement, facing the street.
            fx, fy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
            dx, dy = float(best["ue_x"]) + 60.0 * fx, float(best["ue_y"]) + 60.0 * fy
            door = fmt([(round(dx, 1), round(dy, 1), round(float(best["ue_z"]), 1))])
            src_door, facing = "facade_module", round(yaw % 360.0, 1)
        else:
            door, src_door, facing = fmt(fr.ue([(p.x, p.y)])), "osm_point", ""
        doors.append({"osm": s["osm_id"], "name": s["name"], "brand": s["brand"], "category": s["category"],
                      "type": s["type"], "opening_hours": s["opening_hours"], "building_id": s["building_id"],
                      "door_source": src_door, "door_ue_cm": door, "facing_yaw_deg": facing,
                      "facade_dist_m": "" if best is None else round(d, 1),
                      "osm_entrance": "" if ent is None or ent[1].distance(p) > 10 else "node/" + ent[0]})
    counts["shop_doors"] = write_csv(os.path.join(a.out, "shop_doors.csv"),
                                     ["osm", "name", "brand", "category", "type", "opening_hours", "building_id",
                                      "door_source", "door_ue_cm", "facing_yaw_deg", "facade_dist_m", "osm_entrance"],
                                     doors)

    # -- traboules and covered passages of the centre ----------------------------------------
    def district(lon, lat):
        if lon < 4.8275 and lat < 45.767:
            return "vieux-lyon"
        if lat >= 45.7665:
            return "pentes-croix-rousse" if lat >= 45.768 or lon < 4.8355 else "terreaux"
        return "presqu-ile"
    trab = []
    for wid, (refs, t) in ways.items():
        name = t.get("name", "")
        is_named = bool(re.search(r"traboule", name + t.get("description", "") + t.get("note", ""), re.I))
        is_passage = t.get("tunnel") == "building_passage" and t.get("highway") in WALK_HIGHWAYS | {"service"}
        is_cover = t.get("highway") in WALK_HIGHWAYS and t.get("covered") in ("yes", "arcade") and \
            bool(TRABOULE_RE.search(name))
        if not (is_named or is_passage or is_cover or t.get("traboule")):
            continue
        ll = [nodes[r][:2] for r in refs if r in nodes]
        if len(ll) < 2:
            continue
        pts = [fr.local(*q) for q in ll]
        mid = ll[len(ll) // 2]
        trab.append({"osm": "way/" + wid, "name": name,
                     "kind": "traboule" if is_named or t.get("traboule") else
                     ("building_passage" if is_passage else "covered_passage"),
                     "district": district(*mid), "highway": t.get("highway", ""),
                     "access": t.get("access", ""), "foot": t.get("foot", ""),
                     "opening_hours": t.get("opening_hours", ""), "lit": t.get("lit", ""),
                     "length_m": round(LineString(pts).length, 1), "wikidata": t.get("wikidata", ""),
                     "wikipedia": t.get("wikipedia", ""), "points_ue_cm": fmt(fr.ue(pts))})
    for rid, (members, t) in rels.items():   # named traboules grouped as streets
        if re.search(r"traboule", t.get("name", ""), re.I):
            for row in trab:
                if ("way", row["osm"][4:], ) in {(m[0], m[1]) for m in members}:
                    row["wikidata"] = row["wikidata"] or t.get("wikidata", "")
    counts["traboules"] = write_csv(os.path.join(a.out, "traboules.csv"),
                                    ["osm", "name", "kind", "district", "highway", "access", "foot", "opening_hours",
                                     "lit", "length_m", "wikidata", "wikipedia", "points_ue_cm"], trab)
    by_d = {}
    for r in trab:
        by_d.setdefault(r["district"], {}).setdefault(r["kind"], 0)
        by_d[r["district"]][r["kind"]] += 1

    lons = [n[0] for n in nodes.values()]
    lats = [n[1] for n in nodes.values()]
    summary = {"frame": "Unreal cm: X east, Y south, Z up; origin Lambert-93 %s, altitude %s m (manifest)"
               % (fr.o, fr.alt),
               "slice_local_m": [float(v) for v in a.bbox.split(",")],
               "slice_ue_cm": [zone.bounds[0] * 100, -zone.bounds[3] * 100, zone.bounds[2] * 100, -zone.bounds[1] * 100],
               "counts": counts, "traboules_by_district": by_d,
               "osm_coverage_lonlat": [round(min(lons), 4), round(min(lats), 4), round(max(lons), 4), round(max(lats), 4)],
               "osm_tiles": len(base) + len(extra),
               "sources": ["OpenStreetMap contributors (ODbL)", "IGN BD TOPO (Etalab 2.0) via traffic.py / pedestrians.py",
                           "IGN LiDAR HD MNT (Etalab 2.0) for Z"]}
    with open(os.path.join(a.out, "summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=1, ensure_ascii=False)
    print(json.dumps(summary["counts"]), json.dumps(by_d, ensure_ascii=False))


if __name__ == "__main__":
    main()
