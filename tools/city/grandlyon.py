"""Métropole de Lyon open data (data.grandlyon.com) through its public WFS.

Every layer below is published under the Licence Ouverte (checked on the
portal, 2026-11-09). Layers whose own source is OpenStreetMap (benches,
ornamental fountains) inherit ODbL: like every derived table, they stay out of Git.
"""

import json
import os

import citylib as cl

WFS = "https://download.data.grandlyon.com/wfs/grandlyon"
PAGE = 5000
LAYERS = {
    # name in raw/          : (WFS type name, what we use it for)
    "trees": ("metropole-de-lyon:abr_arbres_alignement.abrarbre", "street trees: species, height, crown"),
    "ridges": ("metropole-de-lyon:fpc_fond_plan_communaut.fpclignefaitage", "3D roof ridge lines (2012)"),
    "kerbs": ("metropole-de-lyon:fpc_fond_plan_communaut.fpcbordchaussee", "kerb lines"),
    "islands": ("metropole-de-lyon:fpc_fond_plan_communaut.fpcilot", "traffic islands"),
    "spot_heights": ("metropole-de-lyon:fpc_fond_plan_communaut.fpcpointcotesol", "surveyed ground heights"),
    "bins": ("metropole-de-lyon:gin_nettoiement.gincorbeille", "litter bins with model"),
    "bike_racks": ("metropole-de-lyon:pvo_patrimoine_voirie.pvostationnementvelo", "bike racks with model"),
    "shelters": ("metropole-de-lyon:pvo_patrimoine_voirie.pvomobilierurbain", "bus shelters and ad panels"),
    "benches": ("metropole-de-lyon:adr_voie_lieu.adrbanc_latest", "benches (OSM-sourced, ODbL)"),
    "fountains": ("metropole-de-lyon:adr_voie_lieu.adrfontaineornem_latest", "ornamental fountains (OSM-sourced)"),
    "bridges": ("metropole-de-lyon:fpc_fond_plan_communaut.fpcpont", "bridge decks"),
}


def fetch_layer(type_name, env):
    xmin, ymin, xmax, ymax = env
    feats, start = [], 0
    while True:
        url = WFS + "?" + cl.qs({
            "SERVICE": "WFS", "VERSION": "2.0.0", "REQUEST": "GetFeature", "TYPENAMES": type_name,
            "OUTPUTFORMAT": "application/json", "SRSNAME": "EPSG:2154", "COUNT": PAGE, "STARTINDEX": start,
            "SORTBY": "gid", "BBOX": f"{xmin},{ymin},{xmax},{ymax},urn:ogc:def:crs:EPSG::2154"})
        page = cl.http_json(url)
        feats += page.get("features", [])
        if len(page.get("features", [])) < PAGE:
            return {"type": "FeatureCollection", "features": feats}
        start += PAGE


def fetch(out_dir, env):
    for name, (type_name, _) in LAYERS.items():
        fc = fetch_layer(type_name, env)
        with open(os.path.join(out_dir, f"grandlyon_{name}.geojson"), "w", encoding="utf-8") as f:
            json.dump(fc, f, ensure_ascii=False)
        print(f"grandlyon {name}: {len(fc['features'])} features")


def load(raw_dir, name):
    path = os.path.join(raw_dir, f"grandlyon_{name}.geojson")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        return json.load(f)["features"]
