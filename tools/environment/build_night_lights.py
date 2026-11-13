"""Write the night lights file read by GTPEnvironmentSubsystem at run time.

One row per light, from the city pipeline outputs of each focus (tools/city):
  props_placement.csv    W1-04 street lamps (OpenStreetMap highway=street_lamp)
  facade_modules.csv     kind=shop modules: one soft rect light per shop front
  details_placement.csv  W14-43 wall sconces, when the street dressing adds them

Output (kept out of Git, the positions come from OpenStreetMap):
  GranTheftPraline/Content/Environment/Data/NightLights.csv
  kind,x,y,z,yaw,pitch,width_cm,height_cm,lumens,kelvin,radius_cm,shadow,map

Units: Unreal centimetres and degrees (X east, Y south), lumens, kelvin. Light
values follow the art bible section 3.2: warm white LED street lighting
(3000 K), cooler shop windows (4000 K). Only the nearest lights are switched
on by the game (light budget of ADR-0002), so every lamp is listed.

Usage: python3 tools/environment/build_night_lights.py [--out FILE] GEN_DIR [GEN_DIR ...]
  GEN_DIR  assets/geo/processed/<zone>/gen/<focus> (the focus gives the map name)
"""

import argparse
import csv
import math
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_OUT = os.path.join(REPO, "GranTheftPraline", "Content", "Environment", "Data", "NightLights.csv")
COLUMNS = ["kind", "x", "y", "z", "yaw", "pitch", "width_cm", "height_cm", "lumens", "kelvin", "radius_cm",
           "shadow", "map"]

# W1-04: old-style lamp, 4.5 m high, lantern centre about 0.4 m under the top.
STREET_LAMP = dict(catalog="W1-04", head_cm=410.0, lumens=3000.0, kelvin=3000.0, radius_cm=2000.0)
# Shop front: light just outside the window, near its top, aimed down at the pavement.
SHOP = dict(out_cm=60.0, height_cm=260.0, pitch=-25.0, width_share=0.8, panel_height_cm=150.0,
            lumens=1500.0, kelvin=4000.0, radius_cm=1000.0)
SHOP_KINDS = ("shop", "shopfront_closed", "gf_window")
SCONCE = dict(catalog="W14-43", lumens=800.0, kelvin=2700.0, radius_cm=800.0, out_cm=25.0)


def map_name(gen_dir):
    focus = os.path.basename(os.path.normpath(gen_dir))
    return "L_City_" + "".join(p.capitalize() for p in focus.split("-"))


def read_csv(path):
    if not os.path.exists(path):
        return []
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def row(kind, x, y, z, yaw, pitch, width, height, spec, shadow, level):
    return {"kind": kind, "x": round(x, 1), "y": round(y, 1), "z": round(z, 1), "yaw": round(yaw, 2),
            "pitch": pitch, "width_cm": round(width, 1), "height_cm": round(height, 1), "lumens": spec["lumens"],
            "kelvin": spec["kelvin"], "radius_cm": spec["radius_cm"], "shadow": shadow, "map": level}


def lights_for(gen_dir):
    level = map_name(gen_dir)
    out = []
    for r in read_csv(os.path.join(gen_dir, "props_placement.csv")):
        if r["catalog_id"] == STREET_LAMP["catalog"]:
            out.append(row("street", float(r["ue_x"]), float(r["ue_y"]), float(r["ue_z"]) + STREET_LAMP["head_cm"],
                           float(r["ue_yaw"]), 0, 0, 0, STREET_LAMP, 1, level))
    for r in read_csv(os.path.join(gen_dir, "facade_modules.csv")):
        # every lit shop window: named shops and the unnamed ground-floor shops, which the
        # facade kit draws as open lit shopfronts too (kit_layout VARIANTS, 13/11)
        if r["kind"] not in SHOP_KINDS or r["side"] != "street":
            continue
        yaw = float(r["ue_yaw"])
        a = math.radians(yaw)
        x = float(r["ue_x"]) + math.cos(a) * SHOP["out_cm"]
        y = float(r["ue_y"]) + math.sin(a) * SHOP["out_cm"]
        out.append(row("shop", x, y, float(r["ue_z"]) + SHOP["height_cm"], yaw, SHOP["pitch"],
                       float(r["width_m"]) * 100.0 * SHOP["width_share"], SHOP["panel_height_cm"], SHOP, 0, level))
    for r in read_csv(os.path.join(gen_dir, "details_placement.csv")):
        if r.get("catalog_id") == SCONCE["catalog"]:
            yaw = float(r["ue_yaw"])
            a = math.radians(yaw)
            out.append(row("sconce", float(r["ue_x"]) + math.cos(a) * SCONCE["out_cm"],
                           float(r["ue_y"]) + math.sin(a) * SCONCE["out_cm"], float(r["ue_z"]), yaw, 0, 0, 0,
                           SCONCE, 0, level))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("gen_dirs", nargs="+")
    ap.add_argument("--out", default=DEFAULT_OUT)
    args = ap.parse_args(argv)
    rows = []
    for gen in args.gen_dirs:
        found = lights_for(gen)
        kinds = {}
        for r in found:
            kinds[r["kind"]] = kinds.get(r["kind"], 0) + 1
        print("%s: %s" % (map_name(gen), ", ".join("%d %s" % (n, k) for k, n in sorted(kinds.items())) or "none"))
        rows += found
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=COLUMNS)
        w.writeheader()
        w.writerows(rows)
    print("%d lights -> %s" % (len(rows), args.out))
    return 0 if rows else 1


if __name__ == "__main__":
    sys.exit(main())
