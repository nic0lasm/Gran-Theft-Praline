#!/usr/bin/env python3
"""Building table of Lyon: one row per real building, ready for building primitives.

Whatever builds the city in Unreal (our facade kit, PCG building primitives such as City
Sample's, an LLM driving the editor), it needs the same few facts per building. This script
gathers them from open data into one documented file, from the République slice
(Bellecour -> Jacobins) out to the horizon:

  footprint (local metres), ground, eave and ridge heights, storeys, roof type and pitch,
  shops on the ground floor, year of construction and period style, wall and roof materials,
  use, and whether the street map already builds it.

Sources (credits in THIRD_PARTY_NOTICES):
- buildings, heights and roofs: Métropole de Lyon, textured 3D model 2018 (CityGML LOD2,
  Licence Ouverte v2), through the cache of far_decor.py prep (one row per CityGML object);
- year, storeys, materials, use: BDNB open (CSTB, Licence Ouverte Etalab 2.0), step "fetch";
- footprints and heights of the zone, street map typology and storeys, shops: our own
  processed files (BD TOPO IGN, LiDAR HD IGN, OpenStreetMap ODbL), when the building is in the
  zone; BD TOPO buildings of the zone that the 2018 model lacks get a row of their own.

Steps:
  fetch   BDNB department export (Rhône, about 560 MB) into assets/geo/raw/<name>/bdnb/, then the
          building groups of the arrondissements (--insee, default Lyon 1st to 7th) with their
          year, storeys, materials and use in <insee>.json (no owner or personal table is read)
  build   -> assets/geo/processed/<name>/buildings_lyon/buildings_lyon.geojson + .csv + summary.json

Usage:
  python3 tools/city/building_table.py fetch --name presqu-ile
  python3 tools/city/building_table.py build --name presqu-ile --focus rue-de-la-republique

Field list and conventions: docs/city/building-table.md (FR: building-table.fr.md).
"""

import argparse
import csv
import json
import math
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import citylib as cl  # noqa: E402
import far_decor  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
# BDNB open data, department export (CSV), millésime 2026-02-b; no owner table is read.
BDNB_DEP_URL = ("https://open-data.s3.fr-par.scw.cloud/bdnb_millesime_2026-02-b/millesime_2026-02-b_dep69/"
                "open_data_millesime_2026-02-b_dep69_csv.zip")
DEFAULT_INSEE = ["69381", "69382", "69383", "69384", "69385", "69386", "69387"]
# Roof kinds of the street map (generate.py) in the table's words; "lidar" (a roof fitted to the
# LiDAR surface) says nothing of its shape.
STREET_ROOF = {"ridged": "pitched", "flat": "flat", "mansard": "mansard"}
SLICE_CLIP = (-80.0, -570.0, 300.0, -180.0)   # generate.py --clip of the République slice (x0, y0, x1, y1)
SCHEMA_VERSION = 1
FIELDS = ("id", "source", "arrondissement", "remarkable", "centroid_x_m", "centroid_y_m", "dist_axis_m",
          "footprint_m2", "ground_z_m", "eave_z_m", "ridge_z_m", "height_eave_m", "height_ridge_m",
          "roof_type", "roof_pitch_deg", "storeys", "storeys_source", "style", "style_source", "year_built",
          "era", "use", "wall_material", "roof_material", "ground_floor_commercial", "ground_floor_shops",
          "shop_names", "in_slice", "in_street_map", "street_map_roof", "bdtopo_id", "bdnb_id")

# Roof faces by slope (degrees from horizontal); walls are steeper than WALL_DEG.
WALL_DEG = 80.0
STEEP_DEG = 55.0          # 55-80: mansard brisis, steep gables
FLAT_DEG = 10.0
# Storey heights by style when no source gives the count (ground, entresol, upper; metres),
# the street map's values (procgen.TYPOLOGY) for haussmann, ancien, moderne.
STOREYS = {"haussmann": (4.2, 2.6, 3.3), "ancien": (3.8, 0.0, 3.0), "moderne": (4.0, 0.0, 2.9),
           "xixe": (4.0, 0.0, 3.2), "entre-deux-guerres": (3.8, 0.0, 3.0), "monument": (6.0, 0.0, 6.0),
           "inconnu": (3.8, 0.0, 3.0)}
HAUSSMANN_STREETS = None   # procgen.HAUSSMANN_STREETS, loaded lazily (shapely import)


# --- fetch -----------------------------------------------------------------------------------
def cmd_fetch(a):
    """Department export of the BDNB (one zip, about 560 MB for the Rhône), then per commune the
    building groups (geometry) joined to their FFO attributes (year, storeys, materials, use)."""
    import io
    import zipfile
    out_dir = os.path.join(a.geo, "raw", a.name, "bdnb")
    os.makedirs(out_dir, exist_ok=True)
    want = [i for i in a.insee if not os.path.exists(os.path.join(out_dir, i + ".json"))]
    if not want:
        print("BDNB: already here")
        return
    zpath = os.path.join(out_dir, "dep69_csv.zip")
    if not os.path.exists(zpath):
        print("BDNB: downloading " + BDNB_DEP_URL)
        far_decor.download(BDNB_DEP_URL, zpath)
    csv.field_size_limit(1 << 28)
    with zipfile.ZipFile(zpath) as z:
        ffo = {}
        with z.open("csv/batiment_groupe_ffo_bat.csv") as fh:
            for r in csv.DictReader(io.TextIOWrapper(fh, encoding="utf-8"), delimiter=";"):
                ffo[r["batiment_groupe_id"]] = r
        rows = {i: [] for i in want}
        with z.open("csv/batiment_groupe.csv") as fh:
            for r in csv.DictReader(io.TextIOWrapper(fh, encoding="utf-8"), delimiter=";"):
                c = r["code_commune_insee"].strip('"')
                if c not in rows:
                    continue
                f = ffo.get(r["batiment_groupe_id"], {})
                year = f.get("annee_construction")
                levels = f.get("nb_niveau")
                rows[c].append({"batiment_groupe_id": r["batiment_groupe_id"], "wkt": r["geom_groupe"],
                                "annee_construction": int(year) if year and year.isdigit() else None,
                                "nb_niveau": int(levels) if levels and levels.isdigit() else None,
                                "usage_niveau_1_txt": f.get("usage_niveau_1_txt") or None,
                                "mat_mur_txt": f.get("mat_mur_txt") or None,
                                "mat_toit_txt": f.get("mat_toit_txt") or None})
    for c, rs in rows.items():
        path = os.path.join(out_dir, c + ".json")
        with open(path + ".part", "w", encoding="utf-8") as f:
            json.dump({"insee": c, "source": "BDNB open (CSTB), Licence Ouverte Etalab 2.0",
                       "export": BDNB_DEP_URL, "rows": rs}, f)
        os.replace(path + ".part", path)
        print(f"{c}: {len(rs)} building groups, {sum(1 for r in rs if r['annee_construction'])} with a year")


# --- analysis of one CityGML object ----------------------------------------------------------
def roof_of(d, bi):
    """Ground, eave and ridge z (local metres) and the roof type of one object, from its faces."""
    sel = d["polys"][d["first"][bi]:d["first"][bi + 1]]
    zs = d["v"][[s for _, s, _, _ in sel]] if len(sel) else None
    if zs is None:
        return None
    allv = np.vstack([d["v"][s:s + n] for _, s, n, _ in sel]).astype(np.float64)
    ground, ridge = float(allv[:, 2].min()), float(allv[:, 2].max())
    areas = {"flat": 0.0, "pitched": 0.0, "steep": 0.0}
    slopes, lows = [], []
    for _, s, n, _ in sel:
        q = d["v"][s:s + n].astype(np.float64)
        nrm = np.cross(q - q[0], np.roll(q, -1, 0) - q[0]).sum(0)
        area = np.linalg.norm(nrm) / 2
        if area < 0.5:
            continue
        slope = math.degrees(math.acos(min(1.0, abs(nrm[2]) / (2 * area))))
        if slope >= WALL_DEG:
            continue
        if slope < FLAT_DEG and q[:, 2].max() < ground + 0.8:
            continue                                   # ground surface
        kind = "flat" if slope < FLAT_DEG else "steep" if slope >= STEEP_DEG else "pitched"
        areas[kind] += area
        if kind == "pitched":
            slopes.append((slope, area))
        lows.append((float(q[:, 2].min()), area))
    total = sum(areas.values())
    # eave = lower edge of the main roof: area-weighted median of the roof faces' lowest points
    # (the lowest face of all is often a courtyard shed, 1.5 m under the street eave on average)
    eave = wmedian(lows)
    if total <= 0:
        rtype = "unknown"
    elif areas["flat"] >= 0.8 * total:
        rtype = "flat"
    elif areas["steep"] >= 0.15 * total and areas["steep"] + areas["pitched"] > 0.3 * total:
        rtype = "mansard"
    elif areas["flat"] >= 0.3 * total:
        rtype = "mixed"
    else:
        rtype = "pitched"
    pitch = round(wmedian(slopes), 1) if slopes else None
    return {"ground_z": ground, "eave_z": eave if eave is not None else ridge, "ridge_z": ridge,
            "roof_type": rtype, "roof_pitch_deg": pitch, "roof_area_m2": round(total, 1)}


def wmedian(pairs):
    """Weighted median of (value, weight) pairs, None when empty."""
    if not pairs:
        return None
    sl = sorted(pairs)
    half, acc = sum(w for _, w in sl) / 2, 0.0
    for v, w in sl:
        acc += w
        if acc >= half:
            return v
    return sl[-1][0]


def era_of(year):
    if not year:
        return None
    if year < 1800:
        return "avant-1800"
    if year < 1853:
        return "1800-1852"
    if year < 1915:
        return "1853-1914"
    if year < 1946:
        return "1915-1945"
    if year < 1976:
        return "1946-1975"
    return "depuis-1976"


def style_of(year, remarkable, typology, eave_h, flat_roof, on_haussmann_street):
    """Period style for the facade vocabulary: street map typology first, then the year."""
    if remarkable:
        return "monument", "maquette-remarquable"
    if typology:
        return typology, "carte-de-la-rue"
    if year:
        if year < 1853:
            return "ancien", "bdnb-annee"
        if year < 1915:
            return ("haussmann" if on_haussmann_street and eave_h >= 14 else "xixe"), "bdnb-annee"
        if year < 1946:
            return "entre-deux-guerres", "bdnb-annee"
        return "moderne", "bdnb-annee"
    if flat_roof and eave_h >= 12:
        return "moderne", "toit-plat"
    return "inconnu", "aucune"


def storeys_from(style, eave_h, mansard):
    g, e, u = STOREYS.get(style, STOREYS["inconnu"])
    if eave_h <= g + 1.0:
        return 1
    rest = eave_h - g - (e if e and eave_h - g > e + u else 0.0)
    n = 1 + (1 if e and eave_h - g > e + u else 0) + max(0, int(round(rest / u)))
    return n + (1 if mansard else 0)


# --- build -----------------------------------------------------------------------------------
def load_bdnb(raw):
    from shapely import wkt
    rows = []
    d = os.path.join(raw, "bdnb")
    if not os.path.isdir(d):
        return rows
    for fn in sorted(os.listdir(d)):
        if not fn.endswith(".json"):
            continue
        with open(os.path.join(d, fn), encoding="utf-8") as f:
            for r in json.load(f)["rows"]:
                try:
                    r["_geom"] = wkt.loads(r.pop("wkt"))
                except Exception:                 # malformed group geometry: skip it
                    continue
                rows.append(r)
    return rows


def cmd_build(a):
    from shapely import affinity
    from shapely.geometry import LineString, Point, Polygon, mapping
    from shapely.strtree import STRtree
    import procgen  # noqa: PLC0415
    t0 = time.time()
    raw, cg_dir, _ = far_decor.raw_dirs(a.geo, a.name)
    proc = os.path.join(a.geo, "processed", a.name)
    man, origin, origin_alt = far_decor.zone_envelope(proc)
    gen = os.path.join(proc, "gen", a.focus)
    with open(os.path.join(gen, "report.json"), encoding="utf-8") as f:
        axis = LineString([(p[0] / 100.0, -p[1] / 100.0) for p in json.load(f)["focus_axis_ue_cm"]])
    cache = os.path.join(cg_dir, "cache")
    stems = sorted(p[:-4] for p in os.listdir(cache) if p.endswith(".npz")) if os.path.isdir(cache) else []
    if not stems:
        raise SystemExit(f"no CityGML cache in {cache}: run far_decor.py fetch and prep first")

    # zone data: BD TOPO buildings, street map rows, shops, roads
    import generate  # noqa: PLC0415
    bd = {b["id"]: b for b in generate.load_buildings(proc)}
    with open(os.path.join(proc, "buildings.csv"), encoding="utf-8") as f:
        bd_rows = {r["id"]: r for r in csv.DictReader(f)}
    bd_ids = list(bd)
    bd_polys = [bd[i]["poly"] for i in bd_ids]
    bd_tree = STRtree(bd_polys)
    gen_rows = {}
    with open(os.path.join(gen, "buildings_gen.csv"), encoding="utf-8") as f:
        for r in csv.DictReader(f):
            k = r["bdtopo_id"]
            if k not in gen_rows or float(r["wall_h_m"]) > float(gen_rows[k]["wall_h_m"]):
                gen_rows[k] = r                       # tallest part of a multi-part building
    replaced = set()
    lm = os.path.join(proc, "landmarks.json")
    if os.path.exists(lm):
        with open(lm, encoding="utf-8") as f:
            replaced = set(json.load(f).get("replaced_bdtopo", []))
    shops = []
    with open(os.path.join(proc, "shops.csv"), encoding="utf-8") as f:
        for r in csv.DictReader(f):
            if r["category"] in ("shop", "amenity", "craft", "office") and r["x_m"]:
                shops.append((float(r["x_m"]), float(r["y_m"]), r["name"], r["type"]))
    shop_tree = STRtree([Point(x, y) for x, y, _, _ in shops]) if shops else None
    zone_box = None
    with open(os.path.join(raw, "lidar.json"), encoding="utf-8") as f:
        e = json.load(f)["envelope_l93"]
        zone_box = (e[0] - origin[0], e[1] - origin[1], e[2] - origin[0], e[3] - origin[1])
    roads = []
    rpath = os.path.join(proc, "roads.geojson")
    if os.path.exists(rpath):
        from shapely.geometry import shape
        with open(rpath, encoding="utf-8") as f:
            for ft in json.load(f)["features"]:
                name = (ft.get("properties") or {}).get("name") or ""
                if name:
                    roads.append((shape(ft["geometry"]), name))
    road_tree = STRtree([g for g, _ in roads]) if roads else None
    global HAUSSMANN_STREETS
    HAUSSMANN_STREETS = procgen.HAUSSMANN_STREETS

    # BDNB groups (Lambert-93 -> local)
    bdnb = load_bdnb(raw)
    bdnb_geoms = [affinity.translate(r["_geom"], -origin[0], -origin[1]) for r in bdnb]
    bdnb_tree = STRtree(bdnb_geoms) if bdnb_geoms else None

    def in_zone(x, y):
        return zone_box[0] <= x <= zone_box[2] and zone_box[1] <= y <= zone_box[3]

    def best_overlap(tree, geoms, fp):
        best, best_a = None, 0.0
        for k in tree.query(fp):
            ar = fp.intersection(geoms[k]).area
            if ar > best_a:
                best, best_a = k, ar
        return best, best_a

    feats, seen, covered_bd = [], set(), set()
    stats = {"citygml_objects": 0, "rows_citygml": 0, "rows_bdtopo_only": 0, "beyond_radius": 0,
             "in_two_zips": 0, "with_bdnb": 0, "with_year": 0, "in_street_map": 0, "in_slice": 0,
             "shops_linked": 0}
    for stem in stems:
        d = far_decor.load_cache(cache, stem)
        stats["citygml_objects"] += len(d["bids"])
        for bi in range(len(d["bids"])):
            if d["cnt"][bi] == 0:
                continue
            bid = str(d["bids"][bi])
            if bid and bid in seen:
                stats["in_two_zips"] += 1
                continue
            seen.add(bid)
            cx, cy = float(d["cen"][bi, 0]), float(d["cen"][bi, 1])
            dist = axis.distance(Point(cx, cy))
            if dist > a.radius:
                stats["beyond_radius"] += 1
                continue
            kind = str(d["kinds"][bi])
            if kind == "bridge":
                continue                                    # not a building primitive
            fp = far_decor.footprint(d, bi)
            if fp is None or fp.is_empty:
                continue
            roof = roof_of(d, bi)
            if roof is None:
                continue
            remarkable = bool(d["remarkable"][bi])
            props = {"id": bid or f"{stem}#{bi}", "source": "maquette-2018", "arrondissement": stem.split("_")[1],
                     "remarkable": remarkable, "centroid_x_m": round(cx, 2), "centroid_y_m": round(cy, 2),
                     "dist_axis_m": round(dist, 1), "footprint_m2": round(fp.area, 1)}
            props["ground_z_m"] = round(roof["ground_z"], 2)
            props["eave_z_m"] = round(roof["eave_z"], 2)
            props["ridge_z_m"] = round(roof["ridge_z"], 2)
            eave_h = max(0.0, roof["eave_z"] - roof["ground_z"])
            props["height_eave_m"] = round(eave_h, 2)
            props["height_ridge_m"] = round(roof["ridge_z"] - roof["ground_z"], 2)
            props["roof_type"] = roof["roof_type"]
            props["roof_pitch_deg"] = roof["roof_pitch_deg"]
            # BD TOPO link (zone only)
            bdtopo_id, typology, levels, levels_src = None, None, None, None
            if in_zone(cx, cy):
                k, ar = best_overlap(bd_tree, bd_polys, fp)
                if k is not None and ar >= 0.3 * min(fp.area, bd[bd_ids[k]]["poly"].area):
                    bdtopo_id = bd_ids[k]
                    covered_bd.add(bdtopo_id)
            props["bdtopo_id"] = bdtopo_id
            g = gen_rows.get(bdtopo_id) if bdtopo_id else None
            props["in_street_map"] = bool(g) or (bdtopo_id in replaced if bdtopo_id else False)
            props["in_slice"] = bool(SLICE_CLIP[0] <= cx <= SLICE_CLIP[2] and SLICE_CLIP[1] <= cy <= SLICE_CLIP[3])
            stats["in_street_map"] += int(props["in_street_map"])
            stats["in_slice"] += int(props["in_slice"])
            props["street_map_roof"] = g["roof_kind"] if g else None
            if g:
                typology, levels, levels_src = g["typology"], int(g["levels"]), "carte-de-la-rue"
            # BDNB link
            year, b_levels, use, wall_mat, roof_mat, bdnb_id = None, None, None, None, None, None
            if bdnb_tree is not None:
                k, ar = best_overlap(bdnb_tree, bdnb_geoms, fp)
                if k is not None and ar >= 0.3 * fp.area:
                    r = bdnb[k]
                    bdnb_id = r["batiment_groupe_id"]
                    year = r.get("annee_construction")
                    b_levels = r.get("nb_niveau")
                    use, wall_mat, roof_mat = r.get("usage_niveau_1_txt"), r.get("mat_mur_txt"), r.get("mat_toit_txt")
            if not wall_mat and bdtopo_id:
                wall_mat = bd_rows.get(bdtopo_id, {}).get("wall_material") or None
                roof_mat = roof_mat or bd_rows.get(bdtopo_id, {}).get("roof_material") or None
            stats["with_bdnb"] += int(bdnb_id is not None)
            stats["with_year"] += int(bool(year))
            props.update({"bdnb_id": bdnb_id, "year_built": year, "era": era_of(year), "use": use,
                          "wall_material": wall_mat, "roof_material": roof_mat})
            # style and storeys
            street_names = set()
            if road_tree is not None:
                for k in road_tree.query(fp.buffer(12.0)):
                    street_names.add(roads[k][1])
            style, style_src = style_of(year, remarkable, typology, eave_h, roof["roof_type"] == "flat",
                                        bool(street_names & HAUSSMANN_STREETS))
            props["style"], props["style_source"] = style, style_src
            if levels is None and b_levels:
                levels, levels_src = int(b_levels), "bdnb"
            if levels is None and bdtopo_id and bd_rows.get(bdtopo_id, {}).get("floors"):
                levels, levels_src = int(float(bd_rows[bdtopo_id]["floors"])), "bdtopo"
            if levels is None:
                levels, levels_src = storeys_from(style, eave_h, roof["roof_type"] == "mansard"), "estime-hauteur"
            props["storeys"], props["storeys_source"] = levels, levels_src
            # shops on the ground floor (zone data only)
            if in_zone(cx, cy) and shop_tree is not None:
                inside = [shops[k] for k in shop_tree.query(fp.buffer(3.0), predicate="contains")]
                props["ground_floor_shops"] = len(inside)
                props["ground_floor_commercial"] = len(inside) > 0
                props["shop_names"] = "; ".join(sorted({n for _, _, n, _ in inside if n}))[:300] or None
                stats["shops_linked"] += len(inside)
            else:
                props["ground_floor_shops"] = None
                props["ground_floor_commercial"] = None
                props["shop_names"] = None
            feats.append({"type": "Feature", "geometry": mapping(fp), "properties": props})
            stats["rows_citygml"] += 1
    # BD TOPO buildings of the zone the 2018 model lacks (built since, or missing)
    for i in bd_ids:
        if i in covered_bd:
            continue
        b, r = bd[i], bd_rows.get(i, {})
        c = b["poly"].centroid
        dist = axis.distance(c)
        if dist > a.radius or b["poly"].area < 8.0:
            continue
        h = float(r.get("height_m") or 0) or None
        g = gen_rows.get(i)
        style = g["typology"] if g else "inconnu"
        props = {"id": i, "source": "bdtopo", "arrondissement": None, "remarkable": False,
                 "centroid_x_m": round(c.x, 2), "centroid_y_m": round(c.y, 2), "dist_axis_m": round(dist, 1),
                 "footprint_m2": round(b["poly"].area, 1), "ground_z_m": None, "eave_z_m": None, "ridge_z_m": None,
                 "height_eave_m": h, "height_ridge_m": None, "roof_type": STREET_ROOF.get(g["roof_kind"], "unknown") if g else "unknown",
                 "roof_pitch_deg": None, "bdtopo_id": i, "in_street_map": bool(g) or i in replaced,
                 "street_map_roof": g["roof_kind"] if g else None,
                 "in_slice": bool(SLICE_CLIP[0] <= c.x <= SLICE_CLIP[2] and SLICE_CLIP[1] <= c.y <= SLICE_CLIP[3]),
                 "bdnb_id": None, "year_built": None, "era": None, "use": r.get("usage_1") or None,
                 "wall_material": r.get("wall_material") or None, "roof_material": r.get("roof_material") or None,
                 "style": style, "style_source": "carte-de-la-rue" if g else "aucune",
                 "storeys": int(g["levels"]) if g else (int(float(r["floors"])) if r.get("floors") else
                                                       (storeys_from("inconnu", h, False) if h else None)),
                 "storeys_source": "carte-de-la-rue" if g else ("bdtopo" if r.get("floors") else "estime-hauteur"),
                 "ground_floor_shops": None, "ground_floor_commercial": None, "shop_names": None}
        if bdnb_tree is not None:
            k, ar = best_overlap(bdnb_tree, bdnb_geoms, b["poly"])
            if k is not None and ar >= 0.3 * b["poly"].area:
                rb = bdnb[k]
                year = rb.get("annee_construction")
                props.update(bdnb_id=rb["batiment_groupe_id"], year_built=year, era=era_of(year),
                             use=rb.get("usage_niveau_1_txt") or props["use"],
                             wall_material=rb.get("mat_mur_txt") or props["wall_material"],
                             roof_material=rb.get("mat_toit_txt") or props["roof_material"])
                if not g:
                    props["style"], props["style_source"] = style_of(year, False, None, h or 0.0, False, False)
                    if rb.get("nb_niveau"):
                        props["storeys"], props["storeys_source"] = int(rb["nb_niveau"]), "bdnb"
                stats["with_bdnb"] += 1
                stats["with_year"] += int(bool(year))
        if shop_tree is not None:
            inside = [shops[k] for k in shop_tree.query(b["poly"].buffer(3.0), predicate="contains")]
            props.update(ground_floor_shops=len(inside), ground_floor_commercial=len(inside) > 0,
                         shop_names="; ".join(sorted({n for _, _, n, _ in inside if n}))[:300] or None)
        feats.append({"type": "Feature", "geometry": mapping(b["poly"]), "properties": props})
        stats["rows_bdtopo_only"] += 1

    out = os.path.join(proc, "buildings_lyon")
    os.makedirs(out, exist_ok=True)
    meta = {"schema_version": SCHEMA_VERSION, "zone": a.name, "focus": a.focus,
            "coordinates": "local metres, x east, y north, from Lambert-93 (EPSG:2154) origin "
                           f"{origin[0]} {origin[1]}; z in metres above {origin_alt} m (NGF-IGN69); "
                           "Unreal: X = x*100, Y = -y*100, Z = z*100 (cm)",
            "origin_lambert93_m": origin, "origin_altitude_m": origin_alt, "radius_m": a.radius,
            "slice_clip_m": SLICE_CLIP,
            "sources": {"maquette-2018": "Métropole de Lyon, Maquettes 3D texturées 2018 (CityGML LOD2), Licence Ouverte v2",
                        "bdnb": "BDNB open (CSTB), Licence Ouverte Etalab 2.0",
                        "bdtopo": "IGN BD TOPO, Licence Ouverte Etalab 2.0",
                        "shops": "© OpenStreetMap contributors, ODbL",
                        "carte-de-la-rue": "tools/city/generate.py (LiDAR HD IGN)"},
            "doc": "docs/city/building-table.md"}
    for ft in feats:
        ft["properties"] = {k: ft["properties"].get(k) for k in FIELDS}
    feats.sort(key=lambda ft: ft["properties"]["dist_axis_m"])
    with open(os.path.join(out, "buildings_lyon.geojson"), "w", encoding="utf-8") as f:
        json.dump({"type": "FeatureCollection", "metadata": meta, "features": feats}, f, ensure_ascii=False)
    keys = list(FIELDS)
    with open(os.path.join(out, "buildings_lyon.csv"), "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(keys + ["footprint_wkt"])
        from shapely.geometry import shape
        for ft in feats:
            p = ft["properties"]
            w.writerow([p.get(k) for k in keys] + [shape(ft["geometry"]).wkt])
    props = [ft["properties"] for ft in feats]
    summary = {**stats, "rows": len(feats), "seconds": round(time.time() - t0, 1),
               "roof_type": count(props, "roof_type"), "style": count(props, "style"),
               "storeys_source": count(props, "storeys_source"), "era": count(props, "era"),
               "slice": {"rows": sum(p["in_slice"] for p in props),
                         "commercial_ground_floor": sum(bool(p["ground_floor_commercial"]) for p in props if p["in_slice"]),
                         "with_year": sum(bool(p["year_built"]) for p in props if p["in_slice"])}}
    with open(os.path.join(out, "summary.json"), "w", encoding="utf-8") as f:
        json.dump({"metadata": meta, "summary": summary}, f, ensure_ascii=False, indent=1)
    print(json.dumps(summary, ensure_ascii=False))


def count(rows, key):
    out = {}
    for r in rows:
        out[str(r.get(key))] = out.get(str(r.get(key)), 0) + 1
    return dict(sorted(out.items(), key=lambda kv: -kv[1]))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", choices=["fetch", "build"])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--insee", nargs="+", default=DEFAULT_INSEE, help="BDNB communes (Lyon arrondissements)")
    ap.add_argument("--radius", type=float, default=2500.0, help="metres around the street axis")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    a = ap.parse_args()
    {"fetch": cmd_fetch, "build": cmd_build}[a.step](a)


if __name__ == "__main__":
    main()
