#!/usr/bin/env python3
"""Check a release folder or archives before they are shared.

Fails when the content holds files that must never ship in a default build
(logo artwork, source assets, private paths) or when an archive is above
2 GiB or has no checksum line.

Usage: check_release_bundle.py PATH [--allow-logos]
  PATH  a packaged app folder, or a folder holding *.zip and SHA256SUMS
"""
import argparse
import pathlib
import re
import sys
import zipfile

LIMIT = 2 * 1024**3
FORBIDDEN = [
    (re.compile(r"(^|/)T_SignLogo_", re.I), "logo artwork (brands rule)", True),
    (re.compile(r"(^|/)CityBrands(/|$)", re.I), "CityBrands plugin (brands rule)", True),
    (re.compile(r"\.(fbx|glb|blend)$", re.I), "source model", False),
    (re.compile(r"(^|/)\.(git|scratch)(/|$)|(^|/)tripo\.env$|\.env$", re.I), "private file", False),
    (re.compile(r"(^|/)(Users|home)/[^/]+/(Desktop|Documents|dev)(/|$)", re.I), "private path", False),
]


def violations(names, allow_logos=False):
    found = []
    for name in names:
        for pattern, why, logo in FORBIDDEN:
            if pattern.search(name) and not (logo and allow_logos):
                found.append(f"{name}: {why}")
    return found


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("path")
    parser.add_argument("--allow-logos", action="store_true")
    args = parser.parse_args(argv)
    root = pathlib.Path(args.path)
    if not root.is_dir():
        print(f"check_release_bundle: not a folder: {root}", file=sys.stderr)
        return 2
    problems = []
    archives = sorted(root.glob("*.zip"))
    if archives:
        sums = root / "SHA256SUMS"
        listed = set()
        if sums.exists():
            listed = {line.split(None, 1)[1].strip() for line in sums.read_text().splitlines() if line.strip()}
        else:
            problems.append("SHA256SUMS missing")
        for a in archives:
            if a.stat().st_size > LIMIT:
                problems.append(f"{a.name}: above 2 GiB")
            if a.name not in listed:
                problems.append(f"{a.name}: no checksum line")
            if not args.allow_logos and "-logos" in a.name:
                problems.append(f"{a.name}: logo variant in a default release")
            with zipfile.ZipFile(a) as zf:
                problems += [f"{a.name}: {v}" for v in violations(zf.namelist(), args.allow_logos)]
    else:
        names = [p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file() or p.is_symlink()]
        problems += violations(names, args.allow_logos)
    for p in problems[:50]:
        print(p)
    print("PASS" if not problems else f"FAIL ({len(problems)})")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
