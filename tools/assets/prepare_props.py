#!/usr/bin/env python3
"""Batch-prepare Tripo masters for Unreal.

Examples:
  python3 tools/assets/prepare_props.py --manifest assets/manifests/wave-01.json \
      --masters assets/tripo/w1 --staging assets/staging/w1
  python3 tools/assets/prepare_props.py ... --ids W1-01 W1-07 --force
"""
import argparse
import json
import os
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gtp_assets.manifest import load_manifest  # noqa: E402
from gtp_assets.prepare import find_master, prepare_asset  # noqa: E402


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--manifest", required=True)
    ap.add_argument("--masters", required=True, help="folder holding the master GLBs")
    ap.add_argument("--staging", required=True, help="output folder (not versioned)")
    ap.add_argument("--ids", nargs="*", help="only these ids (default: every master found)")
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args(argv)

    manifest = load_manifest(args.manifest)
    items = manifest["items"]
    wanted = {i.upper() for i in args.ids} if args.ids else None
    done, skipped, missing, failed = [], [], [], []
    for item in items:
        if wanted and item["id"].upper() not in wanted:
            continue
        src = find_master(args.masters, item["id"])
        if src is None:
            missing.append(item["id"])
            continue
        try:
            meta = prepare_asset(src, item, args.staging, manifest.get("wave", "w1"),
                                 force=args.force)
        except Exception:
            traceback.print_exc()
            failed.append(item["id"])
            continue
        (skipped if meta.get("skipped") else done).append(item["id"])
        t = meta["triangles"]
        print(f"{item['id']}: {'unchanged' if meta.get('skipped') else 'prepared'} "
              f"master {t['master']:,} -> ultra {t['ultra']:,} tris (M1 LOD0 {t['m1_lod0']:,}), "
              f"collision {meta['collision']['type']}")
    report = {"prepared": done, "unchanged": skipped, "no_master_yet": missing,
              "failed": failed}
    os.makedirs(args.staging, exist_ok=True)
    with open(os.path.join(args.staging, "report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
    print(json.dumps({k: len(v) for k, v in report.items()}))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
