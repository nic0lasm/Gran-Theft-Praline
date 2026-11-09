"""Shop sign textures for the facade kit: the real name, in the brand's colours.

One texture per signed shop of a generated street (facade_modules.csv rows with
kind = shop and a name or brand), sized like the kit fascia (2.66 x 0.6 m, so
2048 x 464 px, about 7 px per cm):
  T_Sign_<id>.png       lettering of the real name (no trademark artwork)
  T_SignLogo_<id>.png   the brand's logo on its fascia, when brands.py found one
                        (trademark: ships only through Plugins/CityBrands/, out of Git)
plus signs.csv: shop -> texture(s), background and lettering colours, typeface,
and whether the shop gets a sign at all (ATMs, parcel lockers, artworks and
car parks share the bay but have no fascia).

Colours: the brand's official colour (Wikidata P465) when known, else the
dominant colour of its logo, else a palette by trade (dark green for pharmacies,
deep red for restaurants...). Lettering is white or near-black, whichever
contrasts more (WCAG ratio, reported per sign).

  python3 tools/city/signs.py --name presqu-ile --focus rue-de-la-republique
Output (regenerated, not committed): gen/<focus>/signs/
"""

import argparse
import csv
import json
import os
import re
import sys
import time

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(HERE, "facade_kit"))
import kit_layout  # noqa: E402

FASCIA_M = (2.66, kit_layout.FASCIA["haussmann"]["height_m"])
SIZE = (2048, int(round(2048 * FASCIA_M[1] / FASCIA_M[0] / 8)) * 8)

NO_FASCIA = {"atm", "parcel_locker", "artwork", "parking", "vending_machine", "bicycle_rental", "charging_station",
             "post_box", "telephone", "bench", "waste_basket", "fountain", "taxi", "car_sharing", "ferry_terminal",
             "recycling", "toilets", "drinking_water", "parking_entrance", "bicycle_parking"}

# Background by trade when no brand colour is known (sRGB hex).
TRADE_COLOUR = {
    "pharmacy": "#0B6B3A", "restaurant": "#5A1E1B", "fast_food": "#B3261E", "cafe": "#3B2A20", "bar": "#1E1E24",
    "pub": "#16302B", "bakery": "#6B4A2B", "pastry": "#6B4A2B", "deli": "#2E3B2A", "jewelry": "#14161A",
    "watches": "#14161A", "bank": "#0F2C4C", "clothes": "#1B1B1B", "shoes": "#262626", "cosmetics": "#F2EDE6",
    "perfumery": "#F2EDE6", "optician": "#1F3A5F", "tobacco": "#7A1F1F", "hairdresser": "#2B2B2B",
    "travel_agent": "#0E4D64", "supermarket": "#1D5E2E", "books": "#2A2F45", "chocolate": "#3E2418",
}
DEFAULT_COLOUR = "#1C1E21"
SERIF_TRADES = {"restaurant", "cafe", "bakery", "pastry", "deli", "jewelry", "watches", "perfumery", "chocolate",
                "wine", "bar", "pub", "confectionery", "tea", "antiques", "books"}
FONTS = {
    "sans": ["/usr/share/fonts/opentype/inter/Inter-Bold.otf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
             "/System/Library/Fonts/Supplemental/Arial Bold.ttf"],
    "serif": ["/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf",
              "/System/Library/Fonts/Supplemental/Georgia Bold.ttf"],
}


def font_path(kind):
    for p in FONTS[kind]:
        if os.path.exists(p):
            return p
    return None


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def rgb_hex(c):
    return "#%02X%02X%02X" % tuple(int(v) for v in c[:3])


def luminance(c):
    def ch(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (ch(v) for v in c[:3])
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = sorted((luminance(a), luminance(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def logo_colour(img):
    """Dominant saturated colour of a logo's opaque pixels, or None."""
    im = img.convert("RGBA").resize((96, 96))
    counts = {}
    for r, g, b, a in (im.get_flattened_data() if hasattr(im, "get_flattened_data") else im.getdata()):
        if a < 200:
            continue
        mx, mn = max(r, g, b), min(r, g, b)
        if mx - mn < 40 or mx < 40 or mn > 225:  # greys, black, white
            continue
        k = (r // 24, g // 24, b // 24)
        counts[k] = counts.get(k, 0) + 1
    if not counts or sum(counts.values()) < 150:
        return None
    k = max(counts, key=counts.get)
    return tuple(min(255, v * 24 + 12) for v in k)


def fit_text(draw, text, path, box_w, box_h):
    size = box_h
    while size > 10:
        f = ImageFont.truetype(path, size)
        l, t, r, b = draw.textbbox((0, 0), text, font=f)
        if r - l <= box_w and b - t <= box_h:
            return f, (l, t, r, b)
        size = int(size * 0.92)
    f = ImageFont.truetype(path, 10)
    return f, draw.textbbox((0, 0), text, font=f)


def lettering(text, bg, fg, typeface):
    img = Image.new("RGB", SIZE, bg)
    d = ImageDraw.Draw(img)
    w, h = SIZE
    d.rectangle([0, 0, w - 1, int(h * 0.03)], fill=tuple(max(0, v - 25) for v in bg))  # top shadow line
    f, (l, t, r, b) = fit_text(d, text, font_path(typeface), int(w * 0.84), int(h * 0.56))
    d.text(((w - (r - l)) / 2 - l, (h - (b - t)) / 2 - t), text, font=f, fill=fg)
    return img


def with_logo(logo, bg):
    img = Image.new("RGB", SIZE, bg)
    lg = logo.convert("RGBA")
    # crop to the drawing: opaque and not near-white (many logos come on a white plate)
    px = lg.load()
    mask = Image.new("L", lg.size, 0)
    mp = mask.load()
    for y in range(0, lg.height):
        for x in range(0, lg.width):
            r, g, b, a = px[x, y]
            if a > 40 and min(r, g, b) < 235:
                mp[x, y] = 255
    bbox = mask.getbbox()
    if bbox:
        lg = lg.crop(bbox)
    w, h = SIZE
    s = min(w * 0.8 / lg.width, h * 0.72 / lg.height)
    lg = lg.resize((max(1, int(lg.width * s)), max(1, int(lg.height * s))), Image.LANCZOS)
    img.paste(lg, ((w - lg.width) // 2, (h - lg.height) // 2), lg)
    return img


def safe_id(osm_id, name):
    base = osm_id or name
    return re.sub(r"[^A-Za-z0-9]+", "_", base).strip("_")[:60]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    args = ap.parse_args()
    t0 = time.time()
    proc = os.path.join(args.geo, "processed", args.name)
    gen = os.path.join(proc, "gen", args.focus)
    out = os.path.join(gen, "signs")
    os.makedirs(out, exist_ok=True)
    brands_dir = os.path.join(args.geo, "raw", args.name, "brands")
    brands = {}
    if os.path.exists(os.path.join(brands_dir, "brands.json")):
        with open(os.path.join(brands_dir, "brands.json"), encoding="utf-8") as f:
            brands = json.load(f)
    with open(os.path.join(proc, "shops.csv"), encoding="utf-8") as f:
        shops = {r["osm_id"]: r for r in csv.DictReader(f)}
    with open(os.path.join(gen, "facade_modules.csv"), encoding="utf-8") as f:
        rows = [r for r in csv.DictReader(f) if r["kind"] == "shop" and r["sign_continued"] == "False"
                and (r["sign_brand"] or r["sign_name"])]

    out_rows, seen = [], set()
    for r in rows:
        sid = safe_id(r["shop_osm_id"], r["sign_name"])
        if sid in seen:
            continue
        seen.add(sid)
        shop = shops.get(r["shop_osm_id"], {})
        trade = shop.get("type", "")
        # OSM brand tags without a Wikidata id are often wrong (a chain's name on a
        # different shop); the shop's own name is what is painted above its door then.
        text = (r["sign_brand"] if r["sign_wikidata"] or not r["sign_name"] else r["sign_name"]).strip()
        row = {"sign_id": sid, "shop_osm_id": r["shop_osm_id"], "text": text, "wikidata": r["sign_wikidata"],
               "trade": trade, "has_fascia": trade not in NO_FASCIA}
        if not row["has_fascia"]:
            out_rows.append(row)
            continue
        facts = brands.get(r["sign_wikidata"], {}) if r["sign_wikidata"] else {}
        logo = None
        if facts.get("logo_png") and os.path.exists(os.path.join(brands_dir, "logos", facts["logo_png"])):
            logo = Image.open(os.path.join(brands_dir, "logos", facts["logo_png"]))
        if facts.get("colour"):
            bg, source = hex_rgb(facts["colour"]), "wikidata"
        elif logo is not None and logo_colour(logo):
            bg, source = logo_colour(logo), "logo"
        else:
            bg, source = hex_rgb(TRADE_COLOUR.get(trade, DEFAULT_COLOUR)), "trade"
        fg = (250, 248, 242) if contrast(bg, (250, 248, 242)) >= contrast(bg, (20, 20, 22)) else (20, 20, 22)
        typeface = "serif" if trade in SERIF_TRADES else "sans"
        shown = text.upper() if typeface == "sans" and len(text) <= 22 else text
        lettering(shown, bg, fg, typeface).save(os.path.join(out, "T_Sign_%s.png" % sid), optimize=True)
        row.update({"texture": "T_Sign_%s" % sid, "background": rgb_hex(bg), "lettering": rgb_hex(fg),
                    "colour_source": source, "typeface": typeface, "contrast": round(contrast(bg, fg), 1)})
        if logo is not None:
            # logo on white unless the brand colour contrasts better with the logo's own colours
            lbg = (255, 255, 255)
            lc = logo_colour(logo)
            if lc and contrast(lc, lbg) < 1.6:
                lbg = (20, 20, 22)
            with_logo(logo, lbg).save(os.path.join(out, "T_SignLogo_%s.png" % sid), optimize=True)
            row["logo_texture"] = "T_SignLogo_%s" % sid
        out_rows.append(row)

    fields = ["sign_id", "shop_osm_id", "text", "wikidata", "trade", "has_fascia", "texture", "logo_texture",
              "background", "lettering", "colour_source", "typeface", "contrast"]
    with open(os.path.join(out, "signs.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fields)
        w.writeheader()
        w.writerows(out_rows)
    signed = [r for r in out_rows if r["has_fascia"]]
    report = {"shops": len(out_rows), "fascia_signs": len(signed),
              "no_fascia": sorted({r["trade"] for r in out_rows if not r["has_fascia"]}),
              "with_logo": sum(1 for r in signed if r.get("logo_texture")),
              "colour_sources": {s: sum(1 for r in signed if r.get("colour_source") == s)
                                 for s in ("wikidata", "logo", "trade")},
              "min_contrast": min((r["contrast"] for r in signed), default=None),
              "texture_px": list(SIZE), "seconds": round(time.time() - t0, 1)}
    with open(os.path.join(out, "signs_report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=1)
    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main()
