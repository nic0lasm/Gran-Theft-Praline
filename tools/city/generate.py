#!/usr/bin/env python3
"""Generate the 3D city of a zone: LiDAR-fitted roofs, facade layouts, greybox mesh.

Needs the raw data of fetch.py (BD TOPO, OSM, LiDAR HD, Grand Lyon) and the
tables of build.py. Python 3 with tools/city/requirements.txt.

Output (regenerated, not committed): assets/geo/processed/<name>/gen/<focus>/
  buildings_gen.csv     per building: typology, storeys, roof kind, eave and top heights,
                        ground height, LiDAR checks, colour
  facade_modules.csv    one row per facade module (input of the engine facade kit / PCG)
  greybox.glb           one node per building, vertex colours, glTF axes (X east, Y up, Z south)
  shell.glb             same without the facade modules (roofs, party walls): the kit fills the rest
  report.json           counts and fidelity measurements

Usage:
  python3 tools/city/generate.py --name presqu-ile --focus "Rue de la République" --buffer 45
  python3 tools/city/generate.py --name presqu-ile --focus all      # every building of the zone
  python3 tools/city/generate.py --name presqu-ile --focus "Rue de la République" \
      --clip=-80,-570,300,-180 --out republique-tranche          # Bellecour -> Jacobins slice
"""

import argparse
import csv
import json
import math
import os
import re
import sys
import time

import numpy as np
import shapely
from shapely.geometry import box, LineString, Point, Polygon
from shapely.ops import unary_union

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import citylib as cl  # noqa: E402
import grandlyon  # noqa: E402
import meshkit  # noqa: E402
import procgen  # noqa: E402
import raster  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

MODULE_FIELDS = ["building_id", "typology", "side", "street", "kind", "storey", "bay", "x_m", "y_m", "z_m",
                 "ue_x", "ue_y", "ue_z", "ue_yaw", "width_m", "height_m", "colour", "sign_name", "sign_brand",
                 "sign_wikidata", "shop_osm_id", "sign_continued"]
BUILDING_FIELDS = ["building_id", "bdtopo_id", "courtyard_share", "podium", "typology", "colour", "levels", "street_edges", "court_edges",
                   "party_edges", "roof_kind", "ridges", "ground_z", "eave_z", "top_z", "wall_h_m",
                   "bdtopo_height_m", "lidar_top_minus_ground_m", "modules", "triangles", "cx", "cy",
                   "mesh_name", "ue_x", "ue_y", "ue_z", "bounds_ue_cm", "shell_bounds_ue_cm"]


def slug(s):
    s = s.lower()
    for a, b in (("àâä", "a"), ("éèêë", "e"), ("îï", "i"), ("ôö", "o"), ("ùûü", "u"), ("ç", "c")):
        s = re.sub(f"[{a}]", b, s)
    return re.sub(r"[^a-z0-9]+", "-", s).strip("-")


def parse_ring(s):
    return [tuple(float(v) for v in p.split()) for p in s.split(";") if p]


def load_buildings(proc):
    out = []
    with open(os.path.join(proc, "buildings.csv"), encoding="utf-8") as f:
        for k, row in enumerate(csv.DictReader(f)):
            ring = parse_ring(row["ring_xy"])
            holes = [parse_ring(h) for h in row["holes_xy"].split("|") if h]
            poly = Polygon(ring, holes).buffer(0)
            if poly.is_empty:
                continue
            if poly.geom_type == "MultiPolygon":
                poly = max(poly.geoms, key=lambda g: g.area)
            row["key"] = f"{row['id']}_{k}"
            row["poly"] = poly
            out.append(row)
    return out


def load_roads(raw, origin):
    roads = []
    with open(os.path.join(raw, "bdtopo_troncon_de_route.geojson"), encoding="utf-8") as f:
        feats = json.load(f)["features"]
    for ft in feats:
        p = ft["properties"]
        g = ft["geometry"]
        parts = [g["coordinates"]] if g["type"] == "LineString" else g.get("coordinates", [])
        name = p.get("nom_voie_ban_gauche") or p.get("nom_voie_ban_droite") or p.get("nom_collaboratif_gauche")
        half = (p.get("largeur_de_chaussee") or 4.0) / 2.0
        for coords in parts:
            pts = [(c[0] - origin[0], c[1] - origin[1]) for c in coords]
            if len(pts) >= 2:
                roads.append((LineString(pts), half, name))
    return roads


def load_shops(proc):
    by_building = {}
    with open(os.path.join(proc, "shops.csv"), encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row["building_id"]:
                row["x"], row["y"] = float(row["x_m"]), float(row["y_m"])
                by_building.setdefault(row["building_id"], []).append(row)
    return by_building


AXIS_GAP_M = 150.0   # pieces of one street split by a square (place de la Republique) are chained


def chain_axis(lines):
    """One line along the whole street: the merged pieces chained end to end across gaps such as
    a square, longest first. The longest piece alone (before 12/11) left out the Bellecour ->
    place de la Republique stretch, so the player start fell on the square, not at Bellecour."""
    merged = shapely.line_merge(unary_union(lines))
    pieces = sorted(getattr(merged, "geoms", [merged]), key=lambda g: -g.length)
    coords = list(pieces[0].coords)
    rest = [list(g.coords) for g in pieces[1:]]
    while rest:
        best = None
        for k, c in enumerate(rest):
            for at_end in (True, False):
                end = coords[-1] if at_end else coords[0]
                for rev in (False, True):
                    cc = c[::-1] if rev else c
                    d = math.dist(end, cc[0] if at_end else cc[-1])
                    if best is None or d < best[0]:
                        best = (d, k, at_end, cc)
        d, k, at_end, cc = best
        if d > AXIS_GAP_M:
            break
        coords = coords + cc if at_end else cc + coords
        rest.pop(k)
    return LineString(coords)


def ue_yaw(nx, ny):
    """Unreal yaw (degrees) of a module facing outward along (nx, ny) local (east, north)."""
    return round(math.degrees(math.atan2(-ny, nx)), 2)


def ground_height(poly, mnt, to_world, origin_alt):
    pts = []
    ring = poly.exterior
    for d in np.arange(0, ring.length, 2.0):
        p = ring.interpolate(d)
        pts.append(to_world((p.x, p.y)))
    z = mnt.sample(*np.array(pts).T) - origin_alt
    z = z[~np.isnan(z)]
    return float(np.min(z)) if len(z) else 0.0


def write_shell_cells(out, shell_items, cell_m=250.0, prefix="SM_ShellCell_", stem="shell_cells"):
    """The shells merged per 250 m streaming cell (ADR-0010 grid, cells.py names): one mesh and
    one draw per cell instead of one per building. import_city.py uses it with GTP_SHELL_MERGE=1.
    Vertices stay at their place (node origin at the cell's corner, ground 0)."""
    cells = {}
    for name, mesh, (cx, cy, _gz) in shell_items:
        key = (cell_m * math.floor(cx / cell_m), cell_m * math.floor(cy / cell_m))
        cells.setdefault(key, (meshkit.Mesh(), []))
        cells[key][0].extend(mesh)
        cells[key][1].append(name)

    def f(v):
        return ("p" if v >= 0 else "m") + "%04d" % abs(int(round(v)))
    items, nodes = [], []
    for (x0, y0), (mesh, names) in sorted(cells.items()):
        mname = "%s%s_%s" % (prefix, f(x0), f(y0))
        items.append((mname, mesh, (x0, y0, 0.0)))
        nodes.append({"name": mname, "cell": "cell-%s-%s" % (f(x0), f(y0)), "buildings": names,
                      "triangles": mesh.tris, "ue_cm": list(cl.to_unreal(x0, y0, 0.0))})
    meshkit.write_glb(os.path.join(out, stem + ".glb"), items)
    with open(os.path.join(out, stem + ".json"), "w", encoding="utf-8") as fh:
        json.dump({"cell_m": cell_m, "nodes": nodes}, fh, indent=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--focus", default="Rue de la République",
                    help="street name (BAN spelling) whose buildings are generated, or 'all'")
    ap.add_argument("--buffer", type=float, default=45.0, help="metres around the focus street")
    ap.add_argument("--extra-bbox", action="append", default=[], metavar="X0,Y0,X1,Y1",
                    help="also take the buildings whose centroid is in this box (local metres, "
                         "x east, y north); repeatable. E.g. the Rhone quays and the Lafayette "
                         "bridge head for the drivable loop of 0.3: 250,-60,700,220")
    ap.add_argument("--cell", metavar="X0,Y0,X1,Y1",
                    help="streaming cell mode (tools/city/cells.py): take every building whose centroid "
                         "is in this box, local metres; --focus is then only the output name")
    ap.add_argument("--clip", metavar="X0,Y0,X1,Y1",
                    help="with a street --focus: keep only the buildings whose centroid is in this box "
                         "(local metres), and the focus axis inside it; a shorter slice of the street")
    ap.add_argument("--out", help="output folder name under gen/ (default: the slug of --focus)")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    args = ap.parse_args()
    t0 = time.time()
    raw = os.path.join(args.geo, "raw", args.name)
    proc = os.path.join(args.geo, "processed", args.name)
    out = os.path.join(proc, "gen", args.out or slug(args.focus))
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(proc, "manifest.json"), encoding="utf-8") as f:
        manifest = json.load(f)
    origin = manifest["origin_lambert93_m"]
    origin_alt = manifest["origin_altitude_m"]
    to_world = lambda p: (p[0] + origin[0], p[1] + origin[1])  # noqa: E731

    buildings = load_buildings(proc)
    # Landmarks (tools/city/landmarks.py) get a LiDAR volume of their own instead of the kit.
    replaced = set()
    lm_path = os.path.join(proc, "landmarks.json")
    if os.environ.get("GTP_CITY_LANDMARKS", "1") != "0" and os.path.exists(lm_path):
        with open(lm_path, encoding="utf-8") as f:
            replaced = set(json.load(f).get("replaced_bdtopo", []))
        buildings = [b for b in buildings if b["id"] not in replaced]
    roads = load_roads(raw, origin)
    shops = load_shops(proc)
    mnt, mns = raster.Raster.load(raw, "mnt"), raster.Raster.load(raw, "mns")
    ridge_lines = []
    for ft in grandlyon.load(raw, "ridges"):
        g = ft["geometry"]
        parts = [g["coordinates"]] if g["type"] == "LineString" else g["coordinates"]
        for c in parts:
            if len(c) >= 2 and len(c[0]) >= 3:
                ridge_lines.append([(p[0] - origin[0], p[1] - origin[1], p[2] - origin_alt) for p in c])
    ridges = procgen.RidgeIndex(ridge_lines)

    focus_axis = []
    if args.cell:
        cell = box(*[float(v) for v in args.cell.split(",")])
        selected = [b for b in buildings if cell.contains(b["poly"].centroid)]
    elif args.focus == "all":
        selected = buildings
    else:
        street = [r[0] for r in roads if r[2] == args.focus]
        if not street:
            sys.exit(f"no road named {args.focus!r} in BD TOPO for this zone")
        zone = unary_union(street).buffer(args.buffer)
        selected = [b for b in buildings if zone.intersects(b["poly"].centroid.buffer(1.0))]
        if args.extra_bbox:
            boxes = [box(*[float(v) for v in e.split(",")]) for e in args.extra_bbox]
            have = {id(b) for b in selected}
            selected += [b for b in buildings if id(b) not in have
                         and any(bx.contains(b["poly"].centroid) for bx in boxes)]
        if args.clip:   # a slice of the street (e.g. Bellecour -> Jacobins): buildings and axis inside the box
            clip = box(*[float(v) for v in args.clip.split(",")])
            selected = [b for b in selected if clip.contains(b["poly"].centroid)]
            street = [g for g in (s.intersection(clip) for s in street) if not g.is_empty and g.length > 0]
        longest = chain_axis(street)
        for d in np.arange(0, longest.length, 10.0):
            p = longest.interpolate(d)
            gz = mnt.sample(np.array([p.x + origin[0]]), np.array([p.y + origin[1]]))[0] - origin_alt
            focus_axis.append(list(cl.to_unreal(p.x, p.y, 0.0 if np.isnan(gz) else float(gz))))
    print(f"{len(selected)} buildings selected ({args.focus}), {len(ridge_lines)} ridge lines")

    mod_rows, bld_rows, glb_items, shell_items = [], [], [], []
    roof_err, roof_kinds, typologies, tri_total = [], {}, {}, 0
    skipped = []
    work = []
    for b in selected:
        parts, removed = procgen.carve_courtyards(b["poly"], mns, mnt, to_world,
                                                  lambda p: (p[0] - origin[0], p[1] - origin[1]))
        if not parts or (removed > 0.7 and b["poly"].area < 60) or all(p[1] is not None for p in parts) \
                and b["poly"].area < 60 and max(p[1] for p in parts) < 3.0:
            skipped.append(b["key"])  # LiDAR sees ground: demolished, covered passage or glass roof
            continue
        for k, (part, podium) in enumerate(parts):
            work.append((b, part, removed, f"{b['key']}" + (f"_p{k}" if len(parts) > 1 else ""), podium))
    # Neighbour lookups use the carved parts, so courtyard walls are not mistaken for party walls.
    fps = {b["key"]: b["poly"] for b in buildings}
    for b in selected:
        fps.pop(b["key"], None)
    fps.update({w[3]: w[1] for w in work})
    ctx = procgen.Context(fps, roads)
    courtyard_m2 = sum(b["poly"].area for b in selected if b["key"] not in skipped) - sum(w[1].area for w in work)
    podium_m2 = sum(w[1].area for w in work if w[4] is not None)

    parts_of = {}
    for w in work:
        parts_of.setdefault(w[0]["key"], []).append(w)

    def shops_for(b, key):
        """Each shop of a BD TOPO building goes to the closest of its parts."""
        own = shops.get(b["id"], [])
        siblings = parts_of[b["key"]]
        if len(siblings) == 1:
            return own
        return [s for s in own if min(siblings, key=lambda w: w[1].distance(Point(s["x"], s["y"])))[3] == key]

    for b, poly, removed, key, podium in work:
        ground = ground_height(poly, mnt, to_world, origin_alt)
        roof = procgen.fit_roof(poly, mns, origin_alt, ground, ridges.clipped(poly), to_world,
                                float(b["height_m"]) if b["height_m"] else None, podium)
        roof_tris = procgen.roof_triangles(poly, roof, mns, origin_alt, to_world)
        modules, info = procgen.layout_building(ctx, {"id": key, "nature": b["nature"],
                                                      "usage_1": b["usage_1"], "wall_material": b["wall_material"]},
                                                poly, ground, roof, shops_for(b, key))
        wr = [to_world(p) for p in list(poly.exterior.coords)[:-1]]
        lx, ly, lz = mns.cells_in_ring(wr, inset_m=0.75)
        if len(lz):
            e = procgen.roof_error(roof_tris[0], roof_tris[1], lx - origin[0], ly - origin[1], lz - origin_alt)
            if len(e):
                roof_err.append(e)
        mesh = meshkit.building_mesh(poly, ground, roof, modules, info, roof_tris)
        c = poly.centroid
        mesh_name = "SM_Building_" + key.replace("BATIMENT", "")
        glb_items.append((mesh_name, mesh, (c.x, c.y, ground)))
        shell = meshkit.building_mesh(poly, ground, roof, modules, info, roof_tris, with_modules=False)
        shell_items.append((mesh_name.replace("SM_Building_", "SM_BuildingShell_"), shell, (c.x, c.y, ground)))

        def ue_bounds(m):
            """Pivot-relative bounds in Unreal axes (cm): X east, Y south, Z up; checked after import."""
            mv = np.asarray(m.v) - np.array([c.x, c.y, ground])
            return " ".join(str(v) for v in (
                round(float(mv[:, 0].min()) * 100), round(float(-mv[:, 1].max()) * 100),
                round(float(mv[:, 2].min()) * 100), round(float(mv[:, 0].max()) * 100),
                round(float(-mv[:, 1].min()) * 100), round(float(mv[:, 2].max()) * 100)))
        pux, puy, puz = cl.to_unreal(c.x, c.y, ground)
        tri_total += mesh.tris
        for m in modules:
            ux, uy, uz = cl.to_unreal(m["x"], m["y"], m["z"])
            mod_rows.append({**{k: m[k] for k in ("building_id", "typology", "side", "street", "kind", "storey",
                                                   "bay", "colour", "sign_name", "sign_brand", "sign_wikidata",
                                                   "shop_osm_id", "sign_continued")},
                             "x_m": round(m["x"], 3), "y_m": round(m["y"], 3), "z_m": round(m["z"], 3),
                             "ue_x": ux, "ue_y": uy, "ue_z": uz, "ue_yaw": ue_yaw(m["nx"], m["ny"]),
                             "width_m": round(m["width_m"], 3), "height_m": round(m["height_m"], 3)})
        lidar_h = roof.get("lidar_p98", roof["top_z"]) - ground
        roof_kinds[roof["kind"]] = roof_kinds.get(roof["kind"], 0) + 1
        typologies[info["typology"]] = typologies.get(info["typology"], 0) + 1
        bld_rows.append({
            "building_id": key, "bdtopo_id": b["id"], "courtyard_share": round(removed, 3),
            "podium": podium is not None, "typology": info["typology"], "colour": info["colour"],
            "levels": info["levels"], "street_edges": info["edges"]["street"], "court_edges": info["edges"]["court"],
            "party_edges": info["edges"]["party"], "roof_kind": roof["kind"], "ridges": len(roof["ridges"]),
            "ground_z": round(ground, 2), "eave_z": round(roof["eave_z"], 2), "top_z": round(roof["top_z"], 2),
            "wall_h_m": round(info["wall_h"], 2), "bdtopo_height_m": b["height_m"],
            "lidar_top_minus_ground_m": round(lidar_h, 2), "modules": len(modules), "triangles": mesh.tris,
            "cx": round(c.x, 2), "cy": round(c.y, 2), "mesh_name": mesh_name, "ue_x": pux, "ue_y": puy,
            "ue_z": puz, "bounds_ue_cm": ue_bounds(mesh), "shell_bounds_ue_cm": ue_bounds(shell)})

    with open(os.path.join(out, "facade_modules.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, MODULE_FIELDS)
        w.writeheader()
        w.writerows(mod_rows)
    with open(os.path.join(out, "buildings_gen.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, BUILDING_FIELDS)
        w.writeheader()
        w.writerows(bld_rows)
    meshkit.write_glb(os.path.join(out, "shell.glb"), shell_items)
    write_shell_cells(out, shell_items)
    meshkit.write_glb(os.path.join(out, "greybox.glb"), glb_items,
                      extras={"zone": args.name, "focus": args.focus, "origin_lambert93_m": origin,
                              "origin_altitude_m": origin_alt, "node_translation": "building centroid at ground"})

    err = np.concatenate(roof_err) if roof_err else np.zeros(1)
    kinds = {}
    for r in mod_rows:
        kinds[r["kind"]] = kinds.get(r["kind"], 0) + 1
    report = {
        "zone": args.name, "focus": args.focus, "buffer_m": args.buffer, "extra_bbox": args.extra_bbox,
        "clip": args.clip,
        "bdtopo_buildings": len(selected),
        "left_to_landmarks": sorted(replaced),
        "skipped_lidar_ground": len(skipped), "building_parts": len(bld_rows),
        "open_courtyards_m2": round(courtyard_m2), "covered_courtyards_m2": round(podium_m2),
        "typologies": typologies, "roof_kinds": roof_kinds, "modules": len(mod_rows), "module_kinds": kinds,
        "shops_with_sign": sum(1 for r in mod_rows if r["kind"] == "shop" and r["sign_continued"] is False),
        "shops_with_brand": sum(1 for r in mod_rows if r["kind"] == "shop" and r["sign_brand"]
                                and r["sign_continued"] is False),
        "triangles_total": tri_total,
        # every LiDAR cell (0.5 m) inside the roofs vs the generated roof surface
        "roof_vs_lidar_abs_error_m": {"cells": int(len(err)), "median": round(float(np.median(err)), 2),
                                      "p90": round(float(np.percentile(err, 90)), 2)},
        "seconds": round(time.time() - t0, 1),
        "focus_axis_ue_cm": focus_axis,
    }
    with open(os.path.join(out, "report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    print(json.dumps({k: v for k, v in report.items() if k != "focus_axis_ue_cm"}, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
