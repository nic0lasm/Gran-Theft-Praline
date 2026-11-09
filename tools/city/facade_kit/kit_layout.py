"""Facade kit layout tables, without Blender: shared by kit.py, assemble.py,
the Unreal placement script and the tests."""

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


def module_for(typology, kind):
    return KIND_TO_MODULE[FAMILY_OF_TYPOLOGY[typology]][kind]
