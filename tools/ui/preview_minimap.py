"""Draws the mini-map the way the game does, to check data and look without Unreal.

Reads a minimap.csv (tools/ui/minimap.py) and renders the round, rotating view
(same maths as GTPMinimapCore.h) to a PNG. Review aid only; nothing here ships.

  python3 tools/ui/preview_minimap.py minimap.csv out.png --at X Y --yaw DEG \
      [--zoom near|medium|far] [--size 480]

--at is a position in Unreal cm; --yaw the view heading in degrees (0 = east, 90 = south).
"""

import argparse
import csv
import math

from PIL import Image, ImageDraw

WORLD_RADIUS_CM = {"near": 12000.0, "medium": 22000.0, "far": 40000.0}
STYLE = {  # colour, width in px at 1080 lines (min, max, factor of road width in m)
    "road_major": ((224, 224, 217, 242), 2.5, 5.0, 0.25),
    "road_minor": ((185, 185, 180, 235), 1.5, 3.0, 0.25),
    "path": ((107, 128, 107, 204), 1.0, 1.0, 0.0),
    "tunnel": ((89, 89, 89, 128), 1.5, 1.5, 0.0),
    "water": ((64, 115, 179, 230), 3.0, 3.0, 0.0),
    "building": ((60, 62, 70, 130), 1.0, 1.0, 0.0),
}
DRAW_ORDER = ["water", "building", "tunnel", "path", "road_minor", "road_major"]


def load(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            pts = [tuple(float(v) for v in p.split()) for p in r["points_ue_cm"].split(";")]
            rows.append((r["kind"], float(r["width_m"] or 0), pts))
    return rows


def to_screen(p, centre, yaw, k):
    dx, dy = p[0] - centre[0], p[1] - centre[1]
    fx, fy = math.cos(yaw), math.sin(yaw)
    return (dx * -fy + dy * fx) * k, -(dx * fx + dy * fy) * k


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv")
    ap.add_argument("out")
    ap.add_argument("--at", nargs=2, type=float, required=True)
    ap.add_argument("--yaw", type=float, default=0.0)
    ap.add_argument("--zoom", choices=WORLD_RADIUS_CM, default="medium")
    ap.add_argument("--size", type=int, default=480, help="image size in pixels (disc diameter + margin)")
    args = ap.parse_args()

    scale = 3  # supersampling
    size = args.size * scale
    radius = size * 0.45
    centre_px = size / 2
    yaw = math.radians(args.yaw)
    k = radius / WORLD_RADIUS_CM[args.zoom]
    px_scale = args.size / 480.0 * 1.0  # line widths follow the image size (480 px ~ a 1080p HUD x 1.9)

    img = Image.new("RGBA", (size, size), (24, 24, 28, 255))
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse([centre_px - radius, centre_px - radius, centre_px + radius, centre_px + radius], fill=255)
    layer = Image.new("RGBA", (size, size), (13, 15, 18, 184))
    d = ImageDraw.Draw(layer, "RGBA")
    rows = load(args.csv)
    far = args.zoom == "far"
    for kind in DRAW_ORDER:
        if far and kind in ("building", "path"):
            continue
        colour, wmin, wmax, f = STYLE[kind]
        for k_, width_m, pts in rows:
            if k_ != kind:
                continue
            w = max(1.0, min(wmax, max(wmin, width_m * f)) * px_scale * scale)
            sp = [(centre_px + x, centre_px + y) for x, y in (to_screen(p, args.at, yaw, k) for p in pts)]
            d.line(sp, fill=colour, width=int(round(w)), joint="curve")
    d.ellipse([centre_px - radius, centre_px - radius, centre_px + radius, centre_px + radius],
              outline=(230, 230, 224, 204), width=int(2 * px_scale * scale))
    a = 7 * px_scale * scale * 1.6
    d.polygon([(centre_px, centre_px - a * 1.3), (centre_px - a * 0.8, centre_px + a), (centre_px + a * 0.8, centre_px + a)],
              fill=(255, 158, 26, 255))
    # North mark: world -Y.
    nx, ny = to_screen((args.at[0], args.at[1] - WORLD_RADIUS_CM[args.zoom]), args.at, yaw, k)
    n = math.hypot(nx, ny)
    if n > 1:
        r = radius - 14 * px_scale * scale
        d.text((centre_px + nx / n * r - 6 * scale, centre_px + ny / n * r - 8 * scale), "N", fill=(242, 115, 89, 255))
    out = Image.composite(layer, img, mask)
    out = out.resize((args.size, args.size), Image.LANCZOS).convert("RGB")
    out.save(args.out)
    print("wrote", args.out)


if __name__ == "__main__":
    main()
