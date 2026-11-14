"""Copy City Sample content into our project at the SAME /Game paths, file for file, never
overwriting (Epic content, out of Git: see .gitignore). Plain Python, editor closed.

A copy keeps the package paths, so the references between City Sample assets still resolve, which
a move to another folder would break (and a real Migrate needs City Sample open in the editor).

    python tools/vehicles/copy_citysample_content.py --src <CitySample>/Content \
        --dst <project>/GranTheftPraline/Content Vehicle AI/Traffic

Folders are relative to Content (default: Vehicle). An entry that is not a folder is a pattern
(glob, relative to Content too: Crowd/Character/Male/m_004/Hair/Hair/Hair_S_BuzzCut* takes the groom and its
follicle texture, with their .uexp and .ubulk); --list FILE adds the entries of a file, one per line (# for a
comment), such as the missing dependencies written by tools/character/crowd_hair_deps.py. Files already present
are left untouched and counted. Report: <dst>/../Saved/CitySampleCopy/copy_report.json (copied, skipped, bytes, md5 of
each copied file). --dry-run lists and sizes without copying (the size of each file it would copy).
"""

import argparse
import glob
import hashlib
import json
import os
import shutil
import time


def md5(path):
    digest = hashlib.md5()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def files_under(root):
    return [os.path.join(base, name) for base, _, names in os.walk(root) for name in names]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="City Sample Content folder")
    ap.add_argument("--dst", required=True, help="our project's Content folder")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--list", help="a file of more entries (folders or patterns), one per line")
    ap.add_argument("folders", nargs="*")
    args = ap.parse_args()
    entries = list(args.folders)
    if args.list:
        with open(args.list, encoding="utf-8") as f:
            entries += [line.strip() for line in f if line.strip() and not line.lstrip().startswith("#")]
    if not entries:
        entries = ["Vehicle"]
    t0 = time.time()
    seen = set()
    report = {"src": args.src, "dst": args.dst, "folders": entries, "dry_run": args.dry_run,
              "copied": 0, "skipped_existing": 0, "bytes": 0, "files": {}, "errors": []}
    for folder in entries:
        root = os.path.join(args.src, folder)
        if os.path.isdir(root):
            sources = files_under(root)
        else:
            matches = sorted(glob.glob(root))
            if not matches:
                report["errors"].append("nothing matches " + root)
                continue
            sources = [path for match in matches for path in (files_under(match) if os.path.isdir(match) else [match])]
        for source in sources:
            if os.path.realpath(source) in seen:
                continue
            seen.add(os.path.realpath(source))
            relative = os.path.relpath(source, args.src)
            if relative.startswith(".."):
                report["errors"].append("outside the source, not copied: " + source)
                continue
            target = os.path.join(args.dst, relative)
            if os.path.exists(target):
                report["skipped_existing"] += 1
                continue
            size = os.path.getsize(source)
            report["bytes"] += size
            report["copied"] += 1
            if args.dry_run:
                report["files"][relative.replace(os.sep, "/")] = size
                continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(source, target)
            report["files"][relative.replace(os.sep, "/")] = md5(target)
    report["gigabytes"] = round(report["bytes"] / 1e9, 2)
    report["seconds"] = round(time.time() - t0, 1)
    out = os.path.join(args.dst, "..", "Saved", "CitySampleCopy")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "copy_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    print(json.dumps({k: v for k, v in report.items() if k != "files"}, indent=2))


if __name__ == "__main__":
    main()
