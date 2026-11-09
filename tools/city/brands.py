"""Brand facts for the shop signs, from Wikidata and Wikimedia Commons.

For every distinct brand:wikidata of the zone's shops (shops.csv, from OSM):
  * label, official colour (P465, sRGB hex), logo file name (P154)
  * the logo itself, rendered by Commons as a 960 px PNG (a standard thumbnail size)
Written to assets/geo/raw/<zone>/brands/ (not committed, like every OSM-derived
table; logos are trademarks and only ship through Plugins/CityBrands/, out of Git).

  python3 tools/city/brands.py --name presqu-ile [--focus rue-de-la-republique] [--no-logos]

Requests are spaced (Wikidata asks bots to stay under one per second) and cached:
a second run only fetches what is missing.
"""

import argparse
import csv
import hashlib
import json
import os
import time
import urllib.parse
import urllib.request

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
UA = "city-tools/0.2 (open data import; contact via repository issues)"
ENTITY = "https://www.wikidata.org/wiki/Special:EntityData/%s.json"
UPLOAD = "https://upload.wikimedia.org/wikipedia/commons/"
PAUSE_S = 1.1
LOGO_PAUSE_S = 12.0  # Commons throttles thumbnail renders harder than Wikidata


def get(url, tries=4):
    for k in range(tries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=40) as r:
                return r.read()
        except Exception as e:  # noqa: BLE001  (network errors are retried, then reported)
            if k == tries - 1:
                raise
            print("  retry", url, e)
            time.sleep(30 * (k + 1))


def commons_urls(file_name):
    """Direct upload.wikimedia.org URLs: a 960 px thumbnail (a standard size, served from
    cache), then the original. Special:FilePath with ?width= is throttled for scripts."""
    name = file_name.replace(" ", "_")
    h = hashlib.md5(name.encode("utf-8")).hexdigest()
    q = urllib.parse.quote(name)
    thumb = "%sthumb/%s/%s/%s/960px-%s%s" % (UPLOAD, h[0], h[:2], q, q, ".png" if name.lower().endswith(".svg") else "")
    return [thumb, "%s%s/%s/%s" % (UPLOAD, h[0], h[:2], q)]


def claim_value(claims, prop):
    for c in claims.get(prop, []):
        v = c.get("mainsnak", {}).get("datavalue", {}).get("value")
        if v is not None and c.get("rank") != "deprecated":
            return v
    return None


def brand_facts(qid):
    e = json.loads(get(ENTITY % qid))["entities"]
    ent = e.get(qid) or next(iter(e.values()))  # redirects come back under the new id
    labels = ent.get("labels", {})
    label = (labels.get("fr") or labels.get("en") or {}).get("value", "")
    colour = claim_value(ent.get("claims", {}), "P465")
    logo = claim_value(ent.get("claims", {}), "P154")
    return {"qid": qid, "label": label, "colour": ("#" + colour.upper()) if colour else "", "logo_file": logo or ""}


def wanted(proc, focus):
    """Distinct brand QIDs: of the focus street's signed modules, or of the whole zone."""
    if focus:
        path = os.path.join(proc, "gen", focus, "facade_modules.csv")
        with open(path, encoding="utf-8") as f:
            return sorted({r["sign_wikidata"] for r in csv.DictReader(f) if r["sign_wikidata"]})
    with open(os.path.join(proc, "shops.csv"), encoding="utf-8") as f:
        return sorted({q for r in csv.DictReader(f) for q in r["brand_wikidata"].split(";") if q.strip()})


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--name", required=True)
    ap.add_argument("--focus", default="")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    ap.add_argument("--no-logos", action="store_true")
    args = ap.parse_args()
    proc = os.path.join(args.geo, "processed", args.name)
    out = os.path.join(args.geo, "raw", args.name, "brands")
    os.makedirs(os.path.join(out, "logos"), exist_ok=True)
    index_path = os.path.join(out, "brands.json")
    index = json.load(open(index_path, encoding="utf-8")) if os.path.exists(index_path) else {}
    qids = wanted(proc, args.focus)
    t0, fetched, logos, failed = time.time(), 0, 0, []
    for q in qids:
        q = q.strip()
        if q not in index:
            try:
                index[q] = brand_facts(q)
                fetched += 1
            except Exception as e:  # noqa: BLE001
                failed.append((q, str(e)))
                continue
            time.sleep(PAUSE_S)
        f = index[q]
        if args.no_logos or not f["logo_file"]:
            continue
        ext = ".png"
        dest = os.path.join(out, "logos", "T_Brand_%s%s" % (q, ext))
        if not os.path.exists(dest):
            data, err = None, ""
            for url in commons_urls(f["logo_file"]):
                try:
                    data = get(url, tries=2)
                    break
                except Exception as e:  # noqa: BLE001
                    err = str(e)
            if data and f["logo_file"].lower().endswith(".svg") is False and not data.startswith(b"\x89PNG") \
                    and not data[:3] == b"\xff\xd8\xff":
                data, err = None, "not a raster image"
            if data:
                with open(dest, "wb") as fh:
                    fh.write(data)
                logos += 1
            else:
                failed.append((q, "logo: " + err))
            time.sleep(LOGO_PAUSE_S)
        f["logo_png"] = os.path.basename(dest) if os.path.exists(dest) else ""
        with open(index_path, "w", encoding="utf-8") as fh:
            json.dump(index, fh, indent=1, ensure_ascii=False)
    with open(index_path, "w", encoding="utf-8") as fh:
        json.dump(index, fh, indent=1, ensure_ascii=False)
    have = [index[q] for q in qids if q in index]
    print(json.dumps({"brands": len(qids), "fetched_now": fetched, "logos_now": logos,
                      "with_colour": sum(1 for f in have if f["colour"]),
                      "with_logo": sum(1 for f in have if f.get("logo_png")),
                      "failed": failed[:10], "seconds": round(time.time() - t0, 1)}, indent=1))


if __name__ == "__main__":
    main()
