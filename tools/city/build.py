#!/usr/bin/env python3
"""Turn raw BD TOPO + OSM data into engine-ready tables.

Input : assets/geo/raw/<name>/       (see fetch.py)
Output: assets/geo/processed/<name>/ (regenerated, not committed except the manifest)

  buildings.csv / buildings.geojson   footprint (local m), height, floors, uses, ground altitude
  shops.csv        OSM shops/amenities with name, brand, address, matched building (storefront sign data)
  trees.csv        OSM trees
  furniture.csv    street lamps, benches, bins, bus stops, hydrants, traffic lights, ...
  roads.geojson    BD TOPO road segments with width, lanes, direction, name
  vegetation.geojson, water.geojson
  manifest.json    origin, axes, counts, sources and licences

Local metres: x = east, y = north, origin = Lambert-93 point in manifest.json.
Unreal (cm): X = x*100, Y = -y*100, Z = (altitude - origin_altitude)*100.

Usage: python3 tools/city/build.py --name presqu-ile
"""

import argparse
import csv
import glob
import json
import os
import statistics
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import citylib as cl  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_FLOOR_M = 3.0
DEFAULT_HEIGHT_M = 15.0
MATCH_MAX_M = 15.0  # a shop point this close to a footprint is attributed to it

POI_KEYS = ("shop", "amenity", "tourism", "office", "craft", "leisure", "historic")
POI_SKIP = {"bench", "waste_basket", "bicycle_parking", "bicycle_rental", "drinking_water",
            "fountain", "post_box", "recycling", "parking_space", "shelter", "clock",
            "waste_disposal", "vending_machine", "toilets", "bbq", "parking_entrance",
            "motorcycle_parking", "charging_station", "taxi", "give_box"}
FURNITURE = {
    ("highway", "street_lamp"), ("highway", "bus_stop"), ("highway", "traffic_signals"),
    ("highway", "crossing"), ("emergency", "fire_hydrant"), ("amenity", "bench"),
    ("amenity", "waste_basket"), ("amenity", "bicycle_parking"), ("amenity", "bicycle_rental"),
    ("amenity", "drinking_water"), ("amenity", "fountain"), ("amenity", "post_box"),
    ("amenity", "recycling"), ("amenity", "clock"), ("amenity", "shelter"),
    ("amenity", "parking_entrance"), ("amenity", "toilets"),
}


def read_geojson(raw, layer):
    path = os.path.join(raw, f"bdtopo_{layer}.geojson")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return json.load(f)["features"]


def polygons(geom):
    if geom["type"] == "Polygon":
        return [geom["coordinates"]]
    if geom["type"] == "MultiPolygon":
        return geom["coordinates"]
    return []


def lines(geom):
    if geom["type"] == "LineString":
        return [geom["coordinates"]]
    if geom["type"] == "MultiLineString":
        return geom["coordinates"]
    return []


def local(origin, pt):
    return round(pt[0] - origin[0], 2), round(pt[1] - origin[1], 2)


def build_buildings(raw, origin):
    out = []
    for feat in read_geojson(raw, "batiment"):
        p = feat["properties"]
        for poly in polygons(feat["geometry"]):
            ring = [local(origin, c) for c in poly[0][:-1]]
            if len(ring) < 3 or abs(cl.ring_area(ring)) < 1.0:
                continue
            if cl.ring_area(ring) < 0:
                ring.reverse()
            holes = [[local(origin, c) for c in h[:-1]] for h in poly[1:]]
            ground = p.get("altitude_minimale_sol")
            roof = p.get("altitude_maximale_toit")
            h = p.get("hauteur")
            src = "bdtopo_hauteur"
            if h is None and roof is not None and ground is not None:
                h, src = round(roof - ground, 1), "roof_minus_ground"
            if h is None and p.get("nombre_d_etages"):
                h, src = p["nombre_d_etages"] * DEFAULT_FLOOR_M, "floors_x3m"
            if h is None:
                h, src = DEFAULT_HEIGHT_M, "default"
            cx, cy = cl.ring_centroid(ring)
            out.append({
                "id": p["cleabs"], "nature": p.get("nature"), "usage_1": p.get("usage_1"),
                "usage_2": p.get("usage_2"), "floors": p.get("nombre_d_etages"),
                "dwellings": p.get("nombre_de_logements"), "height_m": h, "height_source": src,
                "ground_alt_m": ground, "roof_alt_max_m": roof,
                "wall_material": p.get("materiaux_des_murs"), "roof_material": p.get("materiaux_de_la_toiture"),
                "light_construction": p.get("construction_legere"), "rnb_id": p.get("identifiants_rnb"),
                "area_m2": round(abs(cl.ring_area(ring)), 1), "cx": round(cx, 2), "cy": round(cy, 2),
                "ring": ring, "holes": holes,
            })
    return out


def parse_osm(raw, bbox):
    """Return (pois, trees, furniture) as lists of dicts with lon/lat/tags."""
    nodes, ways = {}, {}
    for path in sorted(glob.glob(os.path.join(raw, "osm_*.osm"))):
        for _, el in ET.iterparse(path):
            if el.tag == "node":
                tags = {t.get("k"): t.get("v") for t in el.findall("tag")}
                nodes[el.get("id")] = (float(el.get("lon")), float(el.get("lat")), tags)
            elif el.tag == "way":
                ways[el.get("id")] = ([n.get("ref") for n in el.findall("nd")],
                                      {t.get("k"): t.get("v") for t in el.findall("tag")})
            if el.tag in ("node", "way", "relation"):
                el.clear()
    lat0, lon0, lat1, lon1 = bbox
    items = [("node", i, lon, lat, t) for i, (lon, lat, t) in nodes.items() if t]
    for i, (refs, t) in ways.items():
        pts = [nodes[r] for r in refs if r in nodes]
        if t and pts:
            items.append(("way", i, sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts), t))
    pois, trees, furn = [], [], []
    for kind, i, lon, lat, t in items:
        if not (lat0 <= lat <= lat1 and lon0 <= lon <= lon1):
            continue
        rec = {"osm": f"{kind}/{i}", "lon": lon, "lat": lat, "tags": t}
        if t.get("natural") == "tree":
            trees.append(rec)
        elif any((k, t.get(k)) in FURNITURE for k in ("highway", "emergency", "amenity")):
            furn.append(rec)
        for k in POI_KEYS:
            if k in t and t[k] not in POI_SKIP and (t.get("name") or t.get("brand")):
                pois.append(rec)
                break
    return pois, trees, furn


def match_building(pt, buildings):
    best, best_d = None, MATCH_MAX_M
    for b in buildings:
        if cl.point_in_ring(pt, b["ring"]):
            return b, 0.0
        d = cl.dist_point_ring(pt, b["ring"])
        if d < best_d:
            best, best_d = b, d
    return best, best_d


def addr(t):
    street = " ".join(x for x in (t.get("addr:housenumber"), t.get("addr:street")) if x)
    return ", ".join(x for x in (street, t.get("addr:postcode")) if x)


def write_csv(path, rows, fields):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        w.writerows(rows)


def point_rows(recs, origin, origin_alt, kind, buildings=None):
    rows = []
    for r in recs:
        x, y = cl.lonlat_to_l93(r["lon"], r["lat"])
        lx, ly = x - origin[0], y - origin[1]
        t = r["tags"]
        b, d = match_building((lx, ly), buildings) if buildings is not None else (None, None)
        alt = b["ground_alt_m"] if b and b["ground_alt_m"] is not None else origin_alt
        ux, uy, uz = cl.to_unreal(lx, ly, alt - origin_alt)
        row = {"osm_id": r["osm"], "x_m": round(lx, 2), "y_m": round(ly, 2),
               "ue_x": ux, "ue_y": uy, "ue_z": uz, "lon": round(r["lon"], 7), "lat": round(r["lat"], 7)}
        if kind == "shop":
            cat = next(k for k in POI_KEYS if k in t)
            row.update({"category": cat, "type": t[cat], "name": t.get("name", ""),
                        "brand": t.get("brand", ""), "brand_wikidata": t.get("brand:wikidata", ""),
                        "operator": t.get("operator", ""), "cuisine": t.get("cuisine", ""),
                        "opening_hours": t.get("opening_hours", ""), "address": addr(t),
                        "building_id": b["id"] if b else "", "building_dist_m": round(d, 1) if b else ""})
        elif kind == "tree":
            row.update({"species": t.get("species", t.get("genus", "")), "height_m": t.get("height", ""),
                        "leaf_type": t.get("leaf_type", "")})
        else:
            k = next(k for k in ("highway", "emergency", "amenity") if (k, t.get(k)) in FURNITURE)
            row.update({"type": f"{k}={t[k]}", "name": t.get("name", ""), "direction": t.get("direction", "")})
        rows.append(row)
    return rows


def local_collection(feats, origin, keep):
    out = []
    for f in feats:
        g, props = f["geometry"], {k: f["properties"].get(k) for k in keep}
        if g["type"] in ("Polygon", "MultiPolygon"):
            coords = [[[list(local(origin, c)) for c in r] for r in poly] for poly in polygons(g)]
            geom = {"type": "MultiPolygon", "coordinates": coords}
        else:
            coords = [[list(local(origin, c)) for c in ln] for ln in lines(g)]
            geom = {"type": "MultiLineString", "coordinates": coords}
        out.append({"type": "Feature", "properties": props, "geometry": geom})
    return {"type": "FeatureCollection", "features": out}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--raw", default=os.path.join(REPO, "assets", "geo", "raw"))
    ap.add_argument("--out", default=os.path.join(REPO, "assets", "geo", "processed"))
    args = ap.parse_args()
    raw, out = os.path.join(args.raw, args.name), os.path.join(args.out, args.name)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(raw, "fetch.json")) as f:
        bbox = tuple(json.load(f)["bbox_lat0_lon0_lat1_lon1"])

    ox, oy = cl.lonlat_to_l93((bbox[1] + bbox[3]) / 2, (bbox[0] + bbox[2]) / 2)
    origin = (round(ox), round(oy))
    buildings = build_buildings(raw, origin)
    alts = [b["ground_alt_m"] for b in buildings if b["ground_alt_m"] is not None]
    origin_alt = round(statistics.median(alts), 1) if alts else 0.0

    bfields = ["id", "nature", "usage_1", "usage_2", "floors", "dwellings", "height_m", "height_source",
               "ground_alt_m", "roof_alt_max_m", "wall_material", "roof_material", "light_construction",
               "rnb_id", "area_m2", "cx", "cy", "ue_x", "ue_y", "ue_z", "ring_xy", "holes_xy"]
    rows = []
    for b in buildings:
        ux, uy, uz = cl.to_unreal(b["cx"], b["cy"], (b["ground_alt_m"] or origin_alt) - origin_alt)
        rows.append({**{k: b[k] for k in bfields[:17]}, "ue_x": ux, "ue_y": uy, "ue_z": uz,
                     "ring_xy": ";".join(f"{x} {y}" for x, y in b["ring"]),
                     "holes_xy": "|".join(";".join(f"{x} {y}" for x, y in h) for h in b["holes"])})
    write_csv(os.path.join(out, "buildings.csv"), rows, bfields)
    with open(os.path.join(out, "buildings.geojson"), "w") as f:
        json.dump({"type": "FeatureCollection", "features": [
            {"type": "Feature", "properties": {k: b[k] for k in bfields[:17]},
             "geometry": {"type": "Polygon", "coordinates": [[list(p) for p in b["ring"] + b["ring"][:1]]] +
                          [[list(p) for p in h + h[:1]] for h in b["holes"]]}} for b in buildings]}, f)

    counts = {"buildings": len(buildings)}
    pois, trees, furn = parse_osm(raw, bbox)
    common = ["osm_id", "x_m", "y_m", "ue_x", "ue_y", "ue_z", "lon", "lat"]
    shop_rows = point_rows(pois, origin, origin_alt, "shop", buildings)
    write_csv(os.path.join(out, "shops.csv"), shop_rows, common + [
        "category", "type", "name", "brand", "brand_wikidata", "operator", "cuisine",
        "opening_hours", "address", "building_id", "building_dist_m"])
    write_csv(os.path.join(out, "trees.csv"), point_rows(trees, origin, origin_alt, "tree"),
              common + ["species", "height_m", "leaf_type"])
    write_csv(os.path.join(out, "furniture.csv"), point_rows(furn, origin, origin_alt, "furniture"),
              common + ["type", "name", "direction"])
    counts.update({"shops": len(shop_rows), "shops_with_brand": sum(1 for r in shop_rows if r["brand"]),
                   "shops_matched_to_building": sum(1 for r in shop_rows if r["building_id"]),
                   "trees": len(trees), "furniture": len(furn)})

    layers = {
        "roads": ("troncon_de_route", ["cleabs", "nature", "importance", "nombre_de_voies", "largeur_de_chaussee",
                                       "sens_de_circulation", "position_par_rapport_au_sol", "nom_1_gauche",
                                       "nom_1_droite", "vitesse_moyenne_vl", "urbain", "restriction_de_poids_total"]),
        "vegetation": ("zone_de_vegetation", ["cleabs", "nature"]),
        "water": ("surface_hydrographique", ["cleabs", "nature", "persistance"]),
        "structures": ("construction_surfacique", ["cleabs", "nature"]),
        "structures_lineaires": ("construction_lineaire", ["cleabs", "nature"]),
    }
    for name, (layer, keep) in layers.items():
        feats = read_geojson(raw, layer)
        with open(os.path.join(out, f"{name}.geojson"), "w", encoding="utf-8") as f:
            json.dump(local_collection(feats, origin, keep), f, ensure_ascii=False)
        counts[name] = len(feats)

    manifest = {
        "name": args.name, "bbox_lat0_lon0_lat1_lon1": bbox,
        "origin_lambert93_m": origin, "origin_altitude_m": origin_alt,
        "axes": "x=east, y=north (metres); Unreal X=east, Y=south, Z=up (cm)",
        "counts": counts,
        "sources": [
            {"name": "IGN BD TOPO V3", "licence": "Licence Ouverte Etalab 2.0", "credit": "IGN, BD TOPO"},
            {"name": "OpenStreetMap contributors", "licence": "ODbL 1.0",
             "note": "derived tables stay out of Git; credit in game and docs"},
        ],
    }
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
    print(json.dumps(counts, indent=2))


if __name__ == "__main__":
    main()
