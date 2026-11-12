#!/usr/bin/env python3
"""Check that THIRD_PARTY_NOTICES lists every licence family used in assets/SOURCES.md.

Each licence family has keywords that must appear in both THIRD_PARTY_NOTICES.md
and THIRD_PARTY_NOTICES.fr.md whenever SOURCES.md mentions the family. Run in CI
so a new source cannot be added without its credit line.

Usage: check_notices.py [--repo PATH]
"""
import argparse
import pathlib
import sys

# family -> (keywords looked for in SOURCES.md, keywords required in both notices)
FAMILIES = {
    "IGN / Etalab": (["etalab", "ign"], ["IGN", "Etalab"]),
    "Métropole de Lyon": (["métropole de lyon", "grand lyon", "grandlyon"], ["Métropole de Lyon"]),
    "OpenStreetMap": (["openstreetmap", "odbl"], ["OpenStreetMap", "ODbL"]),
    "CC BY-SA 4.0": (["cc by-sa"], ["CC BY-SA 4.0"]),
    "CC BY 4.0": (["cc by 4.0"], ["CC BY 4.0", "creativecommons.org/licenses/by/4.0"]),
    "Poly Haven / CC0": (["poly haven", "polyhaven", "cc0"], ["CC0", "Poly Haven"]),
    "Megascans / Fab": (["megascans", "fab "], ["Megascans", "Fab"]),
    "ambientCG": (["ambientcg"], ["ambientCG"]),
    "Unreal Engine": (["unreal", "epic games", "game animation sample"], ["Unreal Engine", "Epic Games"]),
}


def check(sources_text, notices):
    """notices: {filename: text}. Return a list of problems."""
    low = sources_text.lower()
    problems = []
    for family, (triggers, required) in FAMILIES.items():
        if not any(t in low for t in triggers) and family not in ("Unreal Engine", "OpenStreetMap"):
            continue
        for filename, text in notices.items():
            for word in required:
                if word not in text:
                    problems.append(f"{filename}: missing '{word}' ({family})")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--repo", default=str(pathlib.Path(__file__).resolve().parents[2]))
    args = parser.parse_args(argv)
    repo = pathlib.Path(args.repo)
    try:
        sources = (repo / "assets/SOURCES.md").read_text(encoding="utf-8")
        notices = {n: (repo / n).read_text(encoding="utf-8")
                   for n in ("THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES.fr.md")}
    except OSError as err:
        print(f"check_notices: {err}", file=sys.stderr)
        return 2
    problems = check(sources, notices)
    for p in problems:
        print(p)
    print("PASS" if not problems else "FAIL")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
