#!/usr/bin/env python3
"""Credits of the interior assets, generated from the Fab download manifest.

The manifest (a CSV kept outside the repository, columns ordre, fiche, url, licence, auteur, etat...)
lists every free listing downloaded for the interiors. This writes, for the listings actually
downloaded (etat exporte, ajoute_au_projet or telecharge):
  - credits.md: the attribution block (English and French) for THIRD_PARTY_NOTICES, one line per
    CC BY listing (title, author, link, licence), then the CC0 and Fab Standard License listings
    that need no attribution but are named for the record;
  - sources_rows.md: the rows to paste into assets/SOURCES.md;
  - blocked.txt: listings that must not enter a distributed package yet (licence or author
    missing, or a CC BY listing without its author).
Nothing is written into the repository: the credits stay provisional until the authors and links
are checked on the listing pages.

Usage: credits_from_manifest.py MANIFEST.csv OUT_DIR [--date YYYY-MM-DD]
"""

import argparse
import csv
import datetime
import os
import sys

DOWNLOADED = {"exporte", "ajoute_au_projet", "telecharge"}
CONTENT_DIR = "GranTheftPraline/Content/Interiors/{Free,Fab}/ (not versioned)"


def family(licence):
    low = (licence or "").strip().lower()
    if not low:
        return None
    if low.startswith("cc0"):
        return "CC0"
    if low.startswith("cc-by") or low.startswith("cc by"):
        return "CC BY 4.0"
    if low.startswith("fsl") or "standard" in low:
        return "Fab Standard License"
    if "unreal" in low:
        return "Unreal Engine EULA"
    return licence.strip()


def load(path):
    with open(path, encoding="utf-8") as handle:
        return [row for row in csv.DictReader(handle) if (row.get("etat") or "").strip() in DOWNLOADED]


def build(rows, date):
    attributed, named, blocked, sources = [], [], [], []
    for row in rows:
        title = (row.get("fiche") or "").strip()
        author = (row.get("auteur") or "").strip()
        url = (row.get("url") or "").strip()
        fam = family(row.get("licence"))
        if not url.startswith("http"):
            url = ""
        if fam is None:
            blocked.append("%s: licence not recorded" % title)
            continue
        if fam == "CC BY 4.0":
            if not author:
                blocked.append("%s: CC BY without an author" % title)
                continue
            attributed.append((title, author, url))
            sources.append("| %s | %s | Fab listing \"%s\" by %s | %s | CC BY 4.0 (credit: %s) | Reference interior (provisional credit) |"
                           % (date, CONTENT_DIR, title, author, url or "—", author))
        else:
            named.append((title, author, url, fam))
            sources.append("| %s | %s | Fab listing \"%s\"%s | %s | %s | Reference interior |"
                           % (date, CONTENT_DIR, title, (" by " + author) if author else "", url or "—", fam))
    lines = ["## Reference interior / Intérieur de référence", "",
             "Licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/), unchanged except for "
             "level of detail and texture size / sous licence CC BY 4.0, sans modification hormis le niveau de "
             "détail et la taille des textures :", ""]
    for title, author, url in sorted(attributed, key=lambda item: item[0].lower()):
        lines.append("- \"%s\" by %s%s, CC BY 4.0" % (title, author, (" (" + url + ")") if url else ""))
    lines += ["", "No attribution required, named for the record / sans attribution requise, cités pour mémoire :", ""]
    for title, author, url, fam in sorted(named, key=lambda item: item[0].lower()):
        lines.append("- \"%s\"%s, %s" % (title, (" by " + author) if author else "", fam))
    return "\n".join(lines) + "\n", "\n".join(sources) + "\n", "\n".join(blocked) + ("\n" if blocked else ""), len(attributed)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("manifest")
    parser.add_argument("out_dir")
    parser.add_argument("--date", default=datetime.date.today().isoformat())
    args = parser.parse_args(argv)
    credits, sources, blocked, count = build(load(args.manifest), args.date)
    os.makedirs(args.out_dir, exist_ok=True)
    for name, text in (("credits.md", credits), ("sources_rows.md", sources), ("blocked.txt", blocked)):
        with open(os.path.join(args.out_dir, name), "w", encoding="utf-8") as handle:
            handle.write(text)
    print("%d CC BY credits, %d listings blocked, written to %s" % (count, blocked.count("\n"), args.out_dir))
    return 0


if __name__ == "__main__":
    sys.exit(main())
