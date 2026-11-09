"""Facade kit layout tables, without Blender: shared by kit.py, assemble.py,
the Unreal placement script and the tests."""

import zlib

# Nominal module sizes (metres), matching procgen.TYPOLOGY.
HAUSSMANN = {"bay": 3.3, "gf": 4.2, "entresol": 2.6, "upper": 3.3}
LYONNAIS = {"bay": 2.8, "gf": 3.8, "upper": 3.0}

# facade_modules.csv kind -> module, per typology family
KIND_TO_MODULE = {
    "haussmann": {
        "window": "SM_Facade_Haussmann_Window", "window_balcony": "SM_Facade_Haussmann_WindowBalcony",
        "window_small": "SM_Facade_Haussmann_WindowAttic", "court_window": "SM_Facade_Haussmann_CourtWindow",
        "entresol_window": "SM_Facade_Haussmann_Entresol", "shop": "SM_Facade_Haussmann_Shop",
        "shopfront_closed": "SM_Facade_Haussmann_ShopClosed", "entrance": "SM_Facade_Haussmann_Entrance",
        "gf_window": "SM_Facade_Haussmann_ShopClosed", "blank": "SM_Facade_Haussmann_Blank",
        "monument_bay": "SM_Facade_Haussmann_Window", "_eave": "SM_Facade_Haussmann_Cornice",
    },
    "lyonnais": {
        "window": "SM_Facade_Lyonnais_Window", "window_balcony": "SM_Facade_Lyonnais_Window",
        "window_small": "SM_Facade_Lyonnais_WindowSmall", "court_window": "SM_Facade_Lyonnais_CourtWindow",
        "entresol_window": "SM_Facade_Lyonnais_WindowSmall", "shop": "SM_Facade_Lyonnais_Shop",
        "shopfront_closed": "SM_Facade_Lyonnais_Shop", "entrance": "SM_Facade_Lyonnais_Entrance",
        "gf_window": "SM_Facade_Lyonnais_GroundWindow", "blank": "SM_Facade_Lyonnais_Blank",
        "monument_bay": "SM_Facade_Lyonnais_Window", "_eave": "SM_Facade_Lyonnais_Genoise",
    },
}
FAMILY_OF_TYPOLOGY = {"haussmann": "haussmann", "ancien": "lyonnais", "moderne": "lyonnais",
                      "monument": "haussmann"}


# Variants: a building keeps one variant per module (stable hash of its id), so a
# frontage reads as one design, and neighbours differ. Repeats weight the pick.
VARIANTS = {
    "SM_Facade_Haussmann_Window": ["SM_Facade_Haussmann_Window", "SM_Facade_Haussmann_Window_B",
                                   "SM_Facade_Haussmann_Window_C"],
    "SM_Facade_Haussmann_WindowBalcony": ["SM_Facade_Haussmann_WindowBalcony", "SM_Facade_Haussmann_WindowBalcony_B",
                                          "SM_Facade_Haussmann_WindowBalcony_C"],
    "SM_Facade_Haussmann_Shop": ["SM_Facade_Haussmann_Shop", "SM_Facade_Haussmann_Shop",
                                 "SM_Facade_Haussmann_Shop_B", "SM_Facade_Haussmann_Shop_C"],
    # ground-floor bays with no OSM shop: still shop windows on these streets; one in eight shuttered
    "SM_Facade_Haussmann_ShopClosed": ["SM_Facade_Haussmann_ShopClosed", "SM_Facade_Haussmann_ShopClosed",
                                       "SM_Facade_Haussmann_ShopClosed", "SM_Facade_Haussmann_ShopClosed_B",
                                       "SM_Facade_Haussmann_ShopClosed_B", "SM_Facade_Haussmann_ShopClosed_C",
                                       "SM_Facade_Haussmann_ShopClosed_C", "SM_Facade_Haussmann_ShopShutter"],
}

# Shop fascia (sign band): height, top below the storey top, and the y of its
# front face in the module frame (outside is -Y) for each shop module. The sign
# (logo or lettering) goes 1 cm in front of that face.
FASCIA = {"haussmann": {"height_m": 0.6, "top_below_storey_top_m": 0.95, "width_share": 2.66 / 3.3},
          "lyonnais": {"height_m": 0.65, "top_below_storey_top_m": 0.75, "width_share": 2.6 / 2.8}}
FASCIA_FRONT_Y = {
    "SM_Facade_Haussmann_Shop": 0.02, "SM_Facade_Haussmann_Shop_B": -0.2, "SM_Facade_Haussmann_Shop_C": -0.04,
    "SM_Facade_Lyonnais_Shop": -0.16,
}


def stable_index(key, n):
    return zlib.crc32(key.encode("utf-8")) % n


def module_for(typology, kind, building_id=""):
    base = KIND_TO_MODULE[FAMILY_OF_TYPOLOGY[typology]][kind]
    options = VARIANTS.get(base)
    if not options or not building_id:
        return base
    # a shop's whole frontage and its neighbours in the same building share a style
    return options[stable_index(building_id + base, len(options))]


def sign_frame(module, family, storey_height_m, nominal_height_m):
    """(z above the module floor of the fascia centre, outward offset of the sign) in metres,
    for a module scaled to storey_height_m."""
    f = FASCIA[family]
    centre = nominal_height_m - f["top_below_storey_top_m"] - f["height_m"] / 2
    return centre * storey_height_m / nominal_height_m, -FASCIA_FRONT_Y.get(module, 0.02) + 0.01
