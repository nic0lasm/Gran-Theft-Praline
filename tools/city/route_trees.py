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


# The Norway maple of Fab (Megaplants, copied into the game's Content on the PC on 24/11/2026: 15
# SimpleWind meshes, all Nanite, 0.3 to 25 m). Lyon's streets are lined with plane trees and no
# free plane tree exists: the maple is the nearest free crown, and its "Forest" forms have the
# clear trunk of a street tree. The alder "Field" form the route was planted with branches from
# the ground: in the long take of gate 81 (85-100 s) it filled the camera. The map keeps its
# alders (the base map is frozen); the horizon step replaces them where the maple is there.
MAPLE = "/Game/NorwayMaple/Geometry/SimpleWind/SM_NorwayMaple_"
MAPLES = {"mature": MAPLE + "Forest_04",      # 15.9 m
          "young": MAPLE + "Forest_07",       # 6.0 m
          "square": MAPLE + "Field_02"}       # 22.3 m, open grown: the rows across a square
HEIGHTS_CM = {"mature": (1300.0, 1700.0), "young": (500.0, 700.0)}   # mean height kept inside
SQUARE_TREE_CM = 1650.0
REPLACED = {"SM_BlackAlder_Field_05": "mature", "SM_BlackAlder_Field_05_M1": "mature",
            "SM_BlackAlder_Sapling_06": "young", "SM_BlackAlder_Sapling_06_M1": "young"}


def is_tree(path):
    """True for the mesh of a tree (its trunk is what stands in a lane, not the box of its crown)."""
    return any(w in path for w in ("Tree", "Alder", "Maple", "Beech")) and "Grate" not in path


def kind_of(mesh_name):
    """'mature', 'young' or None: which maple replaces a mesh of the route."""
    return REPLACED.get(mesh_name)


def rescale(kind, old_height_cm, new_height_cm, mean_scale):
    """Factor for the scale of every instance when a mesh new_height_cm tall replaces one
    old_height_cm tall: the mean height of the trees stays what it was, brought inside the range
    of its kind (a sapling scaled up to stand for a tree does not become a 12 m maple)."""
    lo, hi = HEIGHTS_CM[kind]
    target = min(max(old_height_cm * mean_scale, lo), hi)
    return target / (new_height_cm * mean_scale) if new_height_cm > 0.0 and mean_scale > 0.0 else 1.0


# A leafy tree of the packs carries the collision of its own shape, and the alder of the route
# is a field tree: its stems lean a metre and more over what is beside it, at the height of a
# car. Gate 91: the car of the flight, 0.3 to 0.9 m off its line, was "held up" six times by
# trees whose trunks stood clear of that line (quay, 22 s; rue du President Carnot, 140 to 182 s).
# A tree of a street is its trunk: the leafy mesh stops nothing, a plain post stands for the
# trunk (as the street trees swapped in at launch, whose leafy twin has no collision).
LEAFY = ("Alder", "Maple", "Beech")
YOUNG_BELOW_CM = 700.0                                          # a sapling: a thinner, shorter post
TRUNK_POST_CM = {"mature": (45.0, 300.0), "young": (16.0, 200.0)}   # side, height


def is_leafy(path):
    """True for a whole tree of the packs (trunk, stems and crown in one mesh)."""
    return any(w in path for w in LEAFY)


def trunk_post(x, y, z, scale, tall_cm):
    """The post that stands for the trunk of a tree planted at (x, y, z), cm: (x, y, z of its
    middle, side, height) for a box. tall_cm: the tree's height once scaled; scale: the scale
    of the instance, which the post follows between 0.6 and 1.6."""
    side, height = TRUNK_POST_CM["young" if tall_cm < YOUNG_BELOW_CM else "mature"]
    k = min(max(abs(scale), 0.6), 1.6)
    return (x, y, z + height / 2.0, side * k, height)
