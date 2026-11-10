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


# Wave 2 (vehicle drafts, roadway objects): the catalog table has no size column,
# so family, name and longest dimension in metres are declared here.
WAVE2 = {
    "W2-01": ("Vehicles", "Draft_CityCar", 3.9, {}), "W2-02": ("Vehicles", "Draft_Sedan", 4.6, {}),
    "W2-03": ("Vehicles", "Draft_Van", 5.0, {}), "W2-04": ("VehicleParts", "Wheel_Alloy", 0.65, {}),
    "W2-05": ("VehicleParts", "Wheel_SteelHubcap", 0.65, {}), "W2-06": ("VehicleParts", "Tyre_SpareWorn", 0.65, {}),
    "W2-07": ("Debris", "CarDoor_Crumpled", 1.1, {}), "W2-08": ("Debris", "Bumper_Broken", 1.6, {}),
    "W2-09": ("Debris", "CrashDebris_Pile", 0.8, {}), "W2-10": ("Debris", "CarHood_Dented", 1.4, {}),
    "W2-11": ("StreetFurniture", "Bollard_ConcreteRound", 0.9, {}),
    "W2-12": ("Roadworks", "Bollard_FlexibleLane", 0.75, {}),
    "W2-13": ("StreetFurniture", "Bollard_RemovableSteel", 1.0, {}),
    "W2-14": ("Roadworks", "SpeedBump_RubberSegment", 0.5, {}),
    "W2-15": ("Roadworks", "SpeedCushion_Asphalt", 1.8, {}),
    "W2-16": ("Signage", "Post_DeliveryBay", 2.3, {}), "W2-17": ("StreetFurniture", "Post_FoldDownParking", 0.8, {}),
    "W2-18": ("Utilities", "Barrier_ParkingArm", 3.5, {"pivot": "back_center"}),
    "W2-19": ("VehicleParts", "Plate_FrontBlank", 0.52, {"pivot": "back_center"}),
    "W2-20": ("VehicleParts", "Plate_RearBlankHolder", 0.52, {"pivot": "back_center"}),
    "W2-21": ("Debris", "Tyres_Stack", 0.8, {}), "W2-22": ("Roadworks", "RoadPlate_Steel", 2.4, {"collision": "none"}),
    "W2-23": ("Roadworks", "Barrier_WaterFilled", 1.8, {}), "W2-24": ("Roadworks", "Barrier_Jersey", 2.0, {}),
    "W2-25": ("Roadworks", "Delineator_Post", 1.1, {}), "W2-26": ("Debris", "CarBattery_Generic", 0.3, {}),
    "W2-27": ("Debris", "Muffler_Rusty", 1.0, {}), "W2-28": ("Debris", "SideMirror_Broken", 0.25, {}),
    "W2-29": ("Utilities", "GritBin_Yellow", 0.8, {}), "W2-30": ("Roadworks", "Sign_RoadworkTriangle", 1.2, {}),
}

_ROW = re.compile(r"^\|\s*(W\d+-\d+)\s*\|\s*([^|]+?)\s*\|(?:\s*([^|]*?)\s*\|)?(?:\s*([^|]*?)\s*\|)?")


def slugify(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def parse_catalog(md_text: str, wave_prefix: str = "W1") -> list[dict]:
    """Read the asset-catalog.md rows of one wave. Size = longest dimension."""
    items = []
    specs = _spec_table(wave_prefix)
    for line in md_text.splitlines():
        m = _ROW.match(line)
        if not m or not m.group(1).startswith(wave_prefix + "-"):
            continue
        asset_id, name, _notes, size = (g or "" for g in m.groups())
        nums = [float(n) for n in re.findall(r"\d+(?:\.\d+)?", size)]
        if asset_id in WAVE2:
            nums = [WAVE2[asset_id][2]]
        if asset_id in specs:
            nums = [specs[asset_id][2]]
        if not nums:
            continue
        item = {"id": asset_id, "slug": slugify(name), "name": name,
                "size_m": max(nums)}
        item.update(WAVE1_OVERRIDES.get(asset_id, {}))
        if asset_id in WAVE1_NAMES:
            item["family"], item["asset"] = WAVE1_NAMES[asset_id]
        if asset_id in WAVE2:
            item["family"], item["asset"] = WAVE2[asset_id][:2]
            item.update(WAVE2[asset_id][3])
        if asset_id in specs:
            item["family"], item["asset"] = specs[asset_id][:2]
            item.update(specs[asset_id][3])
        item["category"] = category(item["size_m"])
        items.append(item)
    return items


def _spec_table(wave_prefix: str) -> dict:
    """Specs files (tools/assets/gtp_assets/specs/wNN.txt): id|family|asset|size_m|options."""
    import os
    path = os.path.join(os.path.dirname(__file__), "specs", f"w{wave_prefix[1:]}.txt")
    table = {}
    if not os.path.exists(path):
        return table
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            asset_id, family, asset, size, opts = (line.split("|") + [""])[:5]
            extra = dict(o.split("=", 1) for o in opts.split(",") if o)
            for k in ("rotate_x_deg", "rotate_y_deg"):
                if k in extra:
                    extra[k] = float(extra[k])
            table[asset_id] = (family, asset, float(size), extra)
    return table


def category(size_m: float) -> str:
    """Art bible 5.1 size classes: small < 1 m, medium 1-3 m, large above."""
    return "small" if size_m < 1.0 else "medium" if size_m <= 3.0 else "large"


# Art bible 5.1: Nanite source cap, M1 max LOD0 triangles, number of LODs.
BUDGETS = {
    "small": {"ultra_tris": 300_000, "m1_tris": 3_000, "lods": 3},
    "medium": {"ultra_tris": 1_000_000, "m1_tris": 10_000, "lods": 4},
    "large": {"ultra_tris": 2_000_000, "m1_tris": 30_000, "lods": 4},
}
# M1 rules (perf freeze 10/11, from the audit of the 284 props): tighter M1 LOD0 caps by
# object size and a texture side chosen from the on-screen size, never 4K.
M1_RULES_VERSION = "m1rules-1"
M1_TRIS_BY_SIZE = ((0.5, 800), (1.0, 2_000), (2.0, 6_000), (3.0, 12_000), (6.0, 20_000))
M1_TRIS_MAX = 30_000
HERO_FAMILIES = {"Shopfront", "Facade", "Monument"}  # seen close from the pavement: 2K
TEXTURE_BY_SIZE = ((1.0, 512), (3.0, 1024))
TEXTURE_MAX = 2048


def m1_tris_cap(size_m: float) -> int:
    """M1 LOD0 triangle cap for an object of ``size_m`` (never above the art bible class budget)."""
    for limit, cap in M1_TRIS_BY_SIZE:
        if size_m < limit:
            return cap
    return M1_TRIS_MAX


def texture_cap(size_m: float, family: str = "") -> int:
    """Largest texture side justified by the on-screen size (about 1 texel per pixel at 3 m)."""
    if family in HERO_FAMILIES:
        return TEXTURE_MAX
    for limit, side in TEXTURE_BY_SIZE:
        if size_m < limit:
            return side
    return TEXTURE_MAX


SCREEN_SIZES = [1.0, 0.5, 0.25, 0.1]  # art bible 6.2


def lod_plan(lods: int) -> list[dict]:
    """LOD chain for the single Nanite mesh, as fractions of the M1 LOD0.

    Validated on UE 5.8.4: once the Nanite fallback is set (PERCENT_TRIANGLES, the M1 LOD0
    budget), LOD0 in the render data *is* the fallback and ``set_lods`` percentages apply to
    it. So the chain halves from there: 1 / 0.5 / 0.25 / 0.125 (art bible 6.2).
    """
    return [{"percent_triangles": 0.5 ** k, "screen_size": SCREEN_SIZES[k]}
            for k in range(lods)]


def load_manifest(path: str) -> dict:
    with open(path, encoding="utf-8") as f:
        return json.load(f)
