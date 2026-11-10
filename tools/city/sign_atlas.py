"""Pack the shop sign textures of a street into a few atlas pages (performance, M1).

Each sign used to be its own actor with its own material instance and texture: about 600
draw calls for the 341 fascia panels and the blade signs of the street map. With the atlas,
place_facades.py draws all the signs of a page with one instanced component and one material
(M_Sign_Atlas): the instance's custom data gives the cell of its sign in the page.

Lettering pages (the real name, no artwork) are written next to the lettering textures and
go under /Game; logo pages (brand artwork) are written as T_SignLogoAtlas_* and, like the
logo textures, are only imported into the out-of-Git CityBrands plugin.

Input:  gen/<focus>/signs/signs.csv and its T_Sign_* / T_SignLogo_* PNGs (signs.py)
Output: gen/<focus>/signs/T_SignAtlas_<n>.png, T_SignLogoAtlas_<n>.png, sign_atlas.json
  sign_atlas.json: page size, cell size in UV, and for each sign_id its lettering cell
  {"page", "u", "v"} and, when it has one, its logo cell.

Usage: python3 tools/city/sign_atlas.py --name presqu-ile --focus rue-de-la-republique
"""

import argparse
import csv
import json
import os

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))

PAGE = 4096
CELL = (1008, 228)      # about half of the 2048 x 464 sign textures (~2.5 px per cm on a 4 m fascia), 4 per row
GUTTER = 8              # edge pixels repeated around each cell so mips do not bleed
SLOT = (CELL[0] + 2 * GUTTER, CELL[1] + 2 * GUTTER)
COLS, ROWS = PAGE // SLOT[0], PAGE // SLOT[1]


def pack(images, prefix, out_dir):
    """images: list of (key, path). Returns key -> {"page", "u", "v"}; writes the pages."""
    cells, page, k = {}, None, 0
    pages = []
    for key, path in images:
        if k % (COLS * ROWS) == 0:
            page = Image.new("RGB", (PAGE, PAGE), (40, 40, 40))
            pages.append(page)
        n = k % (COLS * ROWS)
        cx, cy = (n % COLS) * SLOT[0], (n // COLS) * SLOT[1]
        im = Image.open(path).convert("RGB").resize(CELL, Image.LANCZOS)
        padded = im.resize(SLOT, Image.NEAREST)          # gutter = stretched edge
        padded.paste(im, (GUTTER, GUTTER))
        page.paste(padded, (cx, cy))
        cells[key] = {"page": len(pages) - 1, "u": (cx + GUTTER) / PAGE, "v": (cy + GUTTER) / PAGE}
        k += 1
    for i, p in enumerate(pages):
        p.save(os.path.join(out_dir, "%s_%d.png" % (prefix, i)), optimize=True)
    return cells, len(pages)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    a = ap.parse_args()
    d = os.path.join(REPO, "assets", "geo", "processed", a.name, "gen", a.focus, "signs")
    with open(os.path.join(d, "signs.csv"), encoding="utf-8") as f:
        rows = [r for r in csv.DictReader(f) if r["has_fascia"] == "True"]
    for old in os.listdir(d):
        if old.startswith(("T_SignAtlas_", "T_SignLogoAtlas_")):
            os.remove(os.path.join(d, old))
    letters, n_l = pack([(r["sign_id"], os.path.join(d, r["texture"] + ".png")) for r in rows],
                        "T_SignAtlas", d)
    logos, n_g = pack([(r["sign_id"], os.path.join(d, r["logo_texture"] + ".png")) for r in rows
                       if r["logo_texture"] and os.path.exists(os.path.join(d, r["logo_texture"] + ".png"))],
                      "T_SignLogoAtlas", d)
    meta = {"page_px": PAGE, "cell_uv": [CELL[0] / PAGE, CELL[1] / PAGE], "pages": n_l, "logo_pages": n_g,
            "signs": {k: {"letters": v, "logo": logos.get(k)} for k, v in letters.items()}}
    with open(os.path.join(d, "sign_atlas.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=1)
    print(json.dumps({"signs": len(letters), "pages": n_l, "logos": len(logos), "logo_pages": n_g,
                      "per_page": COLS * ROWS}))


if __name__ == "__main__":
    main()
