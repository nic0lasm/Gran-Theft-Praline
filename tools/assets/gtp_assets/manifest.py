"""Wave manifests: what to prepare and at which real-world size."""
from __future__ import annotations

import json
import re

# Per-object overrides on top of the catalog (wave 1). Everything else uses
# the defaults: bottom-centre pivot, automatic collision, tri budget by size.
WAVE1_OVERRIDES = {
    "W1-06": {"pivot": "back_center"},  # wall-mounted street light
    "W1-45": {"pivot": "back_center"},  # retracted awning
    "W1-46": {"pivot": "back_center"},  # extended awning
    "W1-47": {"pivot": "back_center"},  # facade air conditioner
    "W1-48": {"pivot": "back_center"},  # balcony railing
    "W1-58": {"pivot": "back_center"},  # surveillance camera
    "W1-59": {"pivot": "back_center"},  # wall ATM
    "W1-28": {"collision": "none"},  # manhole cover
    "W1-29": {"collision": "none"},  # storm drain grate
    "W1-17": {"collision": "none"},  # tree grate
    "W1-56": {"collision": "none"},  # street name sign (wall plate)
}


# Unreal naming (art bible section 7): family folder + "Kind_Descriptor" asset name.
WAVE1_NAMES = {
    "W1-01": ("StreetFurniture", "Bench_WoodCastIron"), "W1-02": ("StreetFurniture", "Bench_ModernMetal"),
    "W1-03": ("StreetFurniture", "Bench_Concrete"), "W1-04": ("Lighting", "StreetLamp_OldStyle"),
    "W1-05": ("Lighting", "StreetLamp_Contemporary"), "W1-06": ("Lighting", "StreetLight_WallMounted"),
    "W1-07": ("StreetFurniture", "LitterBin_TransparentBag"), "W1-08": ("StreetFurniture", "LitterBin_Cylindrical"),
    "W1-09": ("StreetFurniture", "Bollard_AntiParking"), "W1-10": ("StreetFurniture", "Bollard_Retractable"),
    "W1-11": ("StreetFurniture", "Barrier_Sidewalk"), "W1-12": ("Mobility", "BikeRack_Hoop"),
    "W1-13": ("Mobility", "BikeShare_Terminal"), "W1-14": ("Mobility", "Bicycle_City"),
    "W1-15": ("Greenery", "Planter_RectangularShrub"), "W1-16": ("Greenery", "Planter_TreeWooden"),
    "W1-17": ("Greenery", "TreeGrate_Square"), "W1-18": ("StreetFurniture", "Fountain_Drinking"),
    "W1-19": ("Signage", "PosterColumn_Cultural"), "W1-20": ("Signage", "InfoSign_DistrictMap"),
    "W1-21": ("StreetFurniture", "BusShelter_Glass"), "W1-22": ("StreetFurniture", "Newsstand_Closed"),
    "W1-23": ("StreetFurniture", "PhoneBooth_Disused"), "W1-24": ("Utilities", "Mailbox_Street"),
    "W1-25": ("Utilities", "ParkingMeter_Standard"), "W1-26": ("Utilities", "EVCharger_Street"),
    "W1-27": ("Utilities", "Hydrant_Fire"), "W1-28": ("Utilities", "Manhole_Round"),
    "W1-29": ("Utilities", "DrainGrate_Storm"), "W1-30": ("Utilities", "Cabinet_Electrical"),
    "W1-31": ("Utilities", "Container_GlassRecycling"), "W1-32": ("Utilities", "WheelieBin_240L"),
    "W1-33": ("Utilities", "WheelieBin_660L"), "W1-34": ("Terrace", "Table_Cafe"),
    "W1-35": ("Terrace", "Chair_Rattan"), "W1-36": ("Terrace", "Chair_Metal"),
    "W1-37": ("Terrace", "Parasol_Closed"), "W1-38": ("Terrace", "Parasol_Open"),
    "W1-39": ("Terrace", "Windscreen_Glass"), "W1-40": ("Shopfront", "Easel_Chalkboard"),
    "W1-41": ("Shopfront", "MenuHolder_Freestanding"), "W1-42": ("Shopfront", "ClothingRack_Outdoor"),
    "W1-43": ("Shopfront", "Stall_Florist"), "W1-44": ("Shopfront", "Shutter_MetalClosed"),
    "W1-45": ("Shopfront", "Awning_Retracted"), "W1-46": ("Shopfront", "Awning_Extended"),
    "W1-47": ("Shopfront", "AirConditioner_Facade"), "W1-48": ("Shopfront", "Balcony_WroughtIron"),
    "W1-49": ("Roadworks", "Fence_Construction"), "W1-50": ("Roadworks", "Cone_Traffic"),
    "W1-51": ("Roadworks", "Barrier_CrowdControl"), "W1-52": ("Logistics", "Pallet_DeliveryBoxes"),
    "W1-53": ("Logistics", "HandTruck_Delivery"), "W1-54": ("Mobility", "Scooter_Parked"),
    "W1-55": ("Mobility", "KickScooter_Shared"), "W1-56": ("Signage", "StreetNameSign_Plate"),
    "W1-57": ("Signage", "Sign_PedestrianZone"), "W1-58": ("Utilities", "Camera_Surveillance"),
    "W1-59": ("Utilities", "ATM_WallMounted"), "W1-60": ("Logistics", "GarbageBags_Pile"),
}

_ROW = re.compile(r"^\|\s*(W\d+-\d+)\s*\|\s*([^|]+?)\s*\|\s*([^|]*?)\s*\|\s*([^|]*?)\s*\|")


def slugify(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def parse_catalog(md_text: str, wave_prefix: str = "W1") -> list[dict]:
    """Read the asset-catalog.md rows of one wave. Size = longest dimension."""
    items = []
    for line in md_text.splitlines():
        m = _ROW.match(line)
        if not m or not m.group(1).startswith(wave_prefix + "-"):
            continue
        asset_id, name, _notes, size = m.groups()
        nums = [float(n) for n in re.findall(r"\d+(?:\.\d+)?", size)]
        if not nums:
            continue
        item = {"id": asset_id, "slug": slugify(name), "name": name,
                "size_m": max(nums)}
        item.update(WAVE1_OVERRIDES.get(asset_id, {}))
        if asset_id in WAVE1_NAMES:
            item["family"], item["asset"] = WAVE1_NAMES[asset_id]
        item["category"] = category(item["size_m"])
        items.append(item)
    return items


def category(size_m: float) -> str:
    """Art bible 5.1 size classes: small < 1 m, medium 1-3 m, large above."""
    return "small" if size_m < 1.0 else "medium" if size_m <= 3.0 else "large"


# Art bible 5.1: Nanite source cap, M1 max LOD0 triangles, number of LODs.
BUDGETS = {
    "small": {"ultra_tris": 300_000, "m1_tris": 3_000, "lods": 3},
    "medium": {"ultra_tris": 1_000_000, "m1_tris": 10_000, "lods": 4},
    "large": {"ultra_tris": 2_000_000, "m1_tris": 30_000, "lods": 4},
}
SCREEN_SIZES = [1.0, 0.5, 0.25, 0.1]  # art bible 6.2


def lod_plan(ultra_tris: int, m1_tris: int, lods: int) -> list[dict]:
    """LOD chain for the single Nanite mesh.

    LOD0 is the Nanite source; its fallback (used when Nanite is off, M1 tier)
    is the M1 LOD0 budget; each further LOD halves the triangles.
    """
    m1_tris = min(m1_tris, ultra_tris)
    plan = [{"percent_triangles": 1.0, "screen_size": SCREEN_SIZES[0]}]
    for k in range(1, lods):
        plan.append({
            "percent_triangles": max(m1_tris * 0.5 ** k / ultra_tris, 0.0005),
            "screen_size": SCREEN_SIZES[k],
        })
    return plan


def load_manifest(path: str) -> dict:
    with open(path, encoding="utf-8") as f:
        return json.load(f)
