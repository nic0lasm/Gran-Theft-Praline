"""Copy City Sample content into our project at the SAME /Game paths, file for file, never
overwriting (Epic content, out of Git: see .gitignore). Plain Python, editor closed.

A copy keeps the package paths, so the references between City Sample assets still resolve, which
a move to another folder would break (and a real Migrate needs City Sample open in the editor).

    python tools/vehicles/copy_citysample_content.py --src <CitySample>/Content \
        --dst <project>/GranTheftPraline/Content Vehicle AI/Traffic

Folders are relative to Content (default: Vehicle). Files already present are left untouched and
counted. Report: <dst>/../Saved/CitySampleCopy/copy_report.json (copied, skipped, bytes, md5 of
each copied file). --dry-run lists and sizes without copying.
"""

import argparse
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="City Sample Content folder")
    ap.add_argument("--dst", required=True, help="our project's Content folder")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("folders", nargs="*", default=["Vehicle"])
    args = ap.parse_args()
    t0 = time.time()
    report = {"src": args.src, "dst": args.dst, "folders": args.folders, "dry_run": args.dry_run,
              "copied": 0, "skipped_existing": 0, "bytes": 0, "files": {}, "errors": []}
    for folder in args.folders:
        root = os.path.join(args.src, folder)
        if not os.path.isdir(root):
            report["errors"].append("missing " + root)
            continue
        for base, _, names in os.walk(root):
            for name in names:
                source = os.path.join(base, name)
                relative = os.path.relpath(source, args.src)
                target = os.path.join(args.dst, relative)
                if os.path.exists(target):
                    report["skipped_existing"] += 1
                    continue
                size = os.path.getsize(source)
                report["bytes"] += size
                report["copied"] += 1
                if args.dry_run:
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
