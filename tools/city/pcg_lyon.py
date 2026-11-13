"""Lyon facade grammar for the PCG building primitives (City Sample 5.8 PCG, Shape Grammar).

City Sample's Assign_CitySample_Buildings knows 27 American styles only. This module proposes the
Lyon ones, in the grammar syntax of the PCG Subdivide Segment / Subdivide Spline nodes (Epic,
"Using Shape Grammar with PCG": "A, B" sequence, "A*" as many as fit, "[A, B]" group,
"{A:2, B:1}" weighted pick, "<A, B>" priority; scalable modules absorb what is left).

It invents no geometry: every module is a mesh of our facade kit already in the project
(/Game/Environment/Facades/LyonKit, tools/city/facade_kit), with the sizes and the one-variant-
per-building rule of kit_layout.py, and the storey heights of procgen.TYPOLOGY.

For one row of the building table (tools/city/building_table.py) it gives:
  family      grammar family: haussmann | lyonnais | moderne | none (monument: keep its mesh)
  storeys     vertical plan within MaxHeight (the eave): role and height of each stone storey
  vertical    that plan as a grammar string, for a Subdivide Segment along Z
  rows        one horizontal grammar string per storey role, for a Subdivide Spline per storey
  roof        what stands above MaxHeight: mansard (dormers), cornice, genoise or nothing
Nothing here is checked in an editor yet: the PCG side is to be wired on the PC (Unreal MCP).
"""

import zlib

KIT_DIR = "/Game/Environment/Facades/LyonKit"

# Storey heights and bay widths (metres) of procgen.TYPOLOGY / kit_layout, per family.
FAMILY_DIMS = {
    "haussmann": {"gf": 4.2, "entresol": 2.6, "upper": 3.3, "bay": 3.3},
    "lyonnais": {"gf": 3.8, "entresol": 0.0, "upper": 3.0, "bay": 2.8},
    "moderne": {"gf": 4.0, "entresol": 0.0, "upper": 2.9, "bay": 2.8},
}

# Grammar symbols -> kit mesh, module width (m), scalable. "P" (blank pier) is the only scalable
# module of a row: it takes up what the bays leave at both ends, as Epic's fence example does.
MODULES = {
    "haussmann": {
        "P": ("SM_Facade_Haussmann_Blank", 0.5, True),
        "S": ("SM_Facade_Haussmann_Shop", 3.3, False),
        "K": ("SM_Facade_Haussmann_ShopClosed", 3.3, False),
        "E": ("SM_Facade_Haussmann_Entrance", 3.3, False),
        "N": ("SM_Facade_Haussmann_Entresol", 3.3, False),
        "W": ("SM_Facade_Haussmann_Window", 3.3, False),
        "B": ("SM_Facade_Haussmann_WindowBalcony", 3.3, False),
        "A": ("SM_Facade_Haussmann_WindowAttic", 3.3, False),
        "C": ("SM_Facade_Haussmann_Cornice", 3.3, True),
        "D": ("SM_Roof_Haussmann_Dormer", 3.3, False),
        "M": ("SM_Roof_Haussmann_MansardBlank", 0.5, True),
    },
    "lyonnais": {
        "P": ("SM_Facade_Lyonnais_Blank", 0.4, True),
        "S": ("SM_Facade_Lyonnais_Shop", 2.8, False),
        "G": ("SM_Facade_Lyonnais_GroundWindow", 2.8, False),
        "E": ("SM_Facade_Lyonnais_Entrance", 2.8, False),
        "W": ("SM_Facade_Lyonnais_Window", 2.8, False),
        "A": ("SM_Facade_Lyonnais_WindowSmall", 2.8, False),
        "C": ("SM_Facade_Lyonnais_Genoise", 2.8, True),
    },
}
MODULES["moderne"] = MODULES["lyonnais"]   # no modern module in the kit (and the kit is frozen)

# One variant per building, as kit_layout.VARIANTS: a frontage reads as one design.
VARIANTS = {
    "SM_Facade_Haussmann_Window": ["", "_B", "_C"],
    "SM_Facade_Haussmann_WindowBalcony": ["", "_B", "_C"],
    "SM_Facade_Haussmann_Shop": ["", "", "_B", "_C"],
    "SM_Facade_Lyonnais_Window": ["", "", "_B", "_C"],
}

# Vertical symbols (one per storey role), for a Subdivide Segment along Z over each facade.
ROLES = {
    "RDC": "ground storey: shops or ground windows, the entrance in the middle",
    "ENT": "entresol (Haussmann): low storey over the shops",
    "NOB": "étage noble (Haussmann 2nd floor): continuous balcony",
    "ETG": "regular storey",
    "BAL": "top stone storey (Haussmann 5th floor): continuous balcony",
    "SOM": "top storey of an old building: small windows",
    "MAN": "Haussmann mansard storey: zinc brisis with dormers, inside the measured wall",
}

# procgen.MANSARD_MIN_WALL_M / MANSARD_H_M: a Haussmann wall measured above 19 m ends with a
# mansard level (the brisis reads as wall in the LiDAR and in the 2018 model alike).
MANSARD_MIN_WALL_M = 19.0
MANSARD_H_M = 3.2

STYLE_TO_FAMILY = {"haussmann": "haussmann", "xixe": "lyonnais", "ancien": "lyonnais",
                   "entre-deux-guerres": "moderne", "moderne": "moderne", "monument": "none"}


def stable_index(key, n):
    return zlib.crc32(key.encode("utf-8")) % n


def family_of(row):
    """Grammar family of a table row; an unknown style follows its roof (flat: modern)."""
    style = row.get("style") or "inconnu"
    if row.get("remarkable"):
        return "none"
    if style in STYLE_TO_FAMILY:
        return STYLE_TO_FAMILY[style]
    return "moderne" if row.get("roof_type") == "flat" else "lyonnais"


def storey_plan(row, family):
    """[(role, height_m)] of the stone storeys between the ground and MaxHeight (the eave).
    Heights follow the family; the upper storeys share what is left so the plan fits exactly."""
    h = float(row.get("height_eave_m") or 0.0)
    d = FAMILY_DIMS[family]
    if family == "haussmann" and row.get("roof_type") not in ("mansard", "mixed") and h >= MANSARD_MIN_WALL_M:
        return storey_plan(dict(row, height_eave_m=h - MANSARD_H_M, roof_type="mansard"), family) + \
            [("MAN", MANSARD_H_M)]
    gf, ent = d["gf"], d["entresol"]
    if h < gf + 2.0:                                   # a one-storey shop or annex
        return [("RDC", round(max(h, 2.5), 2))]
    if h < gf + ent + d["upper"]:
        ent = 0.0
    n = max(1, round((h - gf - ent) / d["upper"]))
    up = (h - gf - ent) / n
    plan = [("RDC", gf)] + ([("ENT", ent)] if ent else [])
    for k in range(n):
        if family == "haussmann" and k == 0 and n >= 2:
            role = "NOB"
        elif family == "haussmann" and k == n - 1 and n >= 3:
            role = "BAL"
        elif family == "lyonnais" and k == n - 1 and n >= 4:
            role = "SOM"
        else:
            role = "ETG"
        plan.append((role, round(up, 2)))
    plan[-1] = (plan[-1][0], round(h - sum(z for _, z in plan[:-1]), 2))   # rounding goes to the top
    return plan


def roof_of(row, family):
    """What stands above MaxHeight. The table's eave is the foot of the roof faces, so a
    Haussmann mansard (steep zinc brisis) is roof, not wall: it shows as "mansard" or "mixed"."""
    rt = row.get("roof_type")
    if family == "haussmann":
        if rt in ("mansard", "mixed"):
            return "mansard"                 # above MaxHeight: the model's roof faces
        return "cornice"                     # under a MAN storey or a plain roof
    if family == "lyonnais" and rt != "flat":
        return "genoise"
    return "none"


def _variant(family, symbol, building_id):
    mesh = MODULES[family][symbol][0]
    opts = VARIANTS.get(mesh)
    return mesh + (opts[stable_index(building_id + mesh, len(opts))] if opts else "")


def rows_for(family, shops):
    """Horizontal grammar per storey role. The entrance sits in the middle of the frontage,
    as the gate of Epic's fence example (a repeat on each side of it)."""
    if family == "haussmann":
        ground = "P, {S:3, K:1}*, E, {S:3, K:1}*, P" if shops else "P, K*, E, K*, P"
        return {"RDC": ground, "ENT": "P, N*, P", "NOB": "P, B*, P", "ETG": "P, W*, P", "BAL": "P, B*, P",
                "MAN": "M, [D, M]*", "ROOF_mansard": "M, [D, M]*", "ROOF_cornice": "C*"}
    if family in ("lyonnais", "moderne"):
        ground = "P, {S:3, G:1}*, E, {S:3, G:1}*, P" if shops else "P, G*, E, G*, P"
        return {"RDC": ground, "ETG": "P, W*, P", "SOM": "P, A*, P", "ROOF_genoise": "C*"}
    return {}


def grammar_for(row):
    """Everything a PCG graph needs for one building of the table (see the module docstring)."""
    fam = family_of(row)
    bid = str(row.get("id") or "")
    if fam == "none":
        return {"family": "none", "storeys": [], "vertical": "", "rows": {}, "roof": "none", "modules": {}}
    plan = storey_plan(row, fam)
    shops = bool(row.get("ground_floor_commercial"))
    rows = rows_for(fam, shops)
    roof = roof_of(row, fam)
    used = {r for r, _ in plan}
    rows = {k: v for k, v in rows.items() if k in used or k == "ROOF_" + roof}
    symbols = sorted({c for v in rows.values() for c in v if c.isalpha() and c.isupper()})
    modules = {s: {"mesh": KIT_DIR + "/" + _variant(fam, s, bid), "width_m": MODULES[fam][s][1],
                   "scalable": MODULES[fam][s][2]} for s in symbols}
    return {"family": fam, "storeys": plan, "vertical": ", ".join(r for r, _ in plan),
            "rows": rows, "roof": roof, "modules": modules}
