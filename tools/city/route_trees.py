"""Leafy trees of the route set, shared by props.py and route_dressing.py (plain Python, no deps).

Gate 65: the flight's tree grates were planted with the bare plane trunk (W14-63, W14-64) and the
run-time swap found no cooked leafy mesh, so 1 873 trees stayed bare. The route set now places the
leafy alder itself. Each path lists alternatives, "A|B": place_props.py takes the first one that
exists in the project (the M1 rebuild, then the pack's own mesh).
"""
import csv

ROUTE_TREES = {"mature": "/Game/BlackAlder/M1/SM_BlackAlder_Field_05_M1|/Game/BlackAlder/Geometry/SimpleWind/SM_BlackAlder_Field_05",
               "young": "/Game/BlackAlder/M1/SM_BlackAlder_Sapling_06_M1|/Game/BlackAlder/Geometry/SimpleWind/SM_BlackAlder_Sapling_06"}
TREE_IDS = {"W14-63": "mature", "W14-64": "young"}


def plant(row, kind):
    """Give a placement row the leafy alder of its kind ('mature' or 'young')."""
    row["asset_path"] = ROUTE_TREES[kind]
    row["asset"] = row["asset_path"].split("|")[0].rsplit("/", 1)[1]


def retree_csv(path):
    """Rewrite, in place, the tree rows of a route_placement.csv made before the fix (bare trunks).

    Returns how many rows changed; rows already leafy are left as they are."""
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        fields, rows = reader.fieldnames, list(reader)
    changed = 0
    for r in rows:
        kind = TREE_IDS.get(r.get("catalog_id", ""))
        if kind and r.get("asset_path") != ROUTE_TREES[kind]:
            plant(r, kind)
            changed += 1
    if changed:
        with open(path, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=fields)  # \r\n, like props.py
            w.writeheader()
            w.writerows(rows)
    return changed
