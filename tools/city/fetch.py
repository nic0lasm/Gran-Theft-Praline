#!/usr/bin/env python3
"""Download raw city data for a bounding box.

Sources (see docs/city/plan.md for licences):
  * IGN BD TOPO V3 via the Geoplateforme WFS (Licence Ouverte Etalab 2.0)
  * OpenStreetMap via the standard API, tiled (ODbL, kept out of Git)

Output (never committed): assets/geo/raw/<name>/
  bdtopo_<layer>.geojson   Lambert-93 coordinates
  osm_<tile>.osm           raw OSM XML tiles

Usage:
  python3 tools/city/fetch.py --name presqu-ile \
      --bbox 45.7570,4.8250,45.7690,4.8420        # lat0,lon0,lat1,lon1
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import citylib as cl  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
WFS = "https://data.geopf.fr/wfs/ows"
BDTOPO_LAYERS = [
    "batiment", "troncon_de_route", "zone_de_vegetation", "surface_hydrographique",
    "construction_surfacique", "construction_lineaire", "erp",
    "zone_d_activite_ou_d_interet", "equipement_de_transport",
]
PAGE = 1000
OSM_MAP = "https://api.openstreetmap.org/api/0.6/map"
OSM_TILE_DEG = 0.006  # keeps each tile well under the API's 50 000-node cap


def fetch_wfs(layer, bbox):
    lat0, lon0, lat1, lon1 = bbox
    features, start = [], 0
    while True:
        url = WFS + "?" + cl.qs({
            "SERVICE": "WFS", "VERSION": "2.0.0", "REQUEST": "GetFeature",
            "TYPENAMES": f"BDTOPO_V3:{layer}", "OUTPUTFORMAT": "application/json",
            "SRSNAME": "EPSG:2154", "SORTBY": "cleabs", "COUNT": PAGE, "STARTINDEX": start,
            "BBOX": f"{lat0},{lon0},{lat1},{lon1},urn:ogc:def:crs:EPSG::4326",
        })
        page = cl.http_json(url)
        features += page["features"]
        if len(page["features"]) < PAGE:
            return {"type": "FeatureCollection", "features": features}
        start += PAGE


def osm_tiles(bbox):
    lat0, lon0, lat1, lon1 = bbox
    n_lat = max(1, round((lat1 - lat0) / OSM_TILE_DEG))
    n_lon = max(1, round((lon1 - lon0) / OSM_TILE_DEG))
    for i in range(n_lat):
        for j in range(n_lon):
            yield (f"{i}_{j}",
                   (lat0 + (lat1 - lat0) * i / n_lat, lon0 + (lon1 - lon0) * j / n_lon,
                    lat0 + (lat1 - lat0) * (i + 1) / n_lat, lon0 + (lon1 - lon0) * (j + 1) / n_lon))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--bbox", required=True, help="lat0,lon0,lat1,lon1 (WGS84)")
    ap.add_argument("--out", default=os.path.join(REPO, "assets", "geo", "raw"))
    ap.add_argument("--skip-osm", action="store_true")
    args = ap.parse_args()
    bbox = tuple(float(v) for v in args.bbox.split(","))
    out = os.path.join(args.out, args.name)
    os.makedirs(out, exist_ok=True)

    for layer in BDTOPO_LAYERS:
        fc = fetch_wfs(layer, bbox)
        with open(os.path.join(out, f"bdtopo_{layer}.geojson"), "w", encoding="utf-8") as f:
            json.dump(fc, f, ensure_ascii=False)
        print(f"bdtopo {layer}: {len(fc['features'])} features")

    if not args.skip_osm:
        for tag, tb in osm_tiles(bbox):
            url = OSM_MAP + "?" + cl.qs({"bbox": f"{tb[1]:.6f},{tb[0]:.6f},{tb[3]:.6f},{tb[2]:.6f}"})
            data = cl.http_get(url)
            with open(os.path.join(out, f"osm_{tag}.osm"), "wb") as f:
                f.write(data)
            print(f"osm tile {tag}: {len(data) / 1e6:.1f} MB")

    with open(os.path.join(out, "fetch.json"), "w") as f:
        json.dump({"name": args.name, "bbox_lat0_lon0_lat1_lon1": bbox}, f, indent=2)


if __name__ == "__main__":
    main()
