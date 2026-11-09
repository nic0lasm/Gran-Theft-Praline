#!/usr/bin/env python3
"""Batch-prepare Tripo masters for Unreal.

Examples:
  python3 tools/assets/prepare_props.py --manifest assets/manifests/wave-01.json \
      --masters assets/tripo/w1 --staging assets/staging/w1
  python3 tools/assets/prepare_props.py ... --ids W1-01 W1-07 --force
"""
import argparse
import json
import multiprocessing
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
    jobs = []
    for item in items:
        if wanted and item["id"].upper() not in wanted:
            continue
        src = find_master(args.masters, item["id"])
        if src is None:
            missing.append(item["id"])
        else:
            jobs.append((item, src))

    # One fresh worker process per asset: a 2 M-triangle master peaks around 3 GB,
    # and memory must not pile up across the batch (16 GB machine, editor open).
    ctx = multiprocessing.get_context("spawn")
    with ctx.Pool(1, maxtasksperchild=1) as pool:
        results = [(item, pool.apply_async(
            prepare_asset, (src, item, args.staging, manifest.get("wave", "w1"), args.force)))
            for item, src in jobs]
        for item, res in results:
            try:
                meta = res.get()
            except Exception:
                traceback.print_exc()
                failed.append(item["id"])
                continue
            (skipped if meta.get("skipped") else done).append(item["id"])
            t = meta["triangles"]
            print(f"{item['id']}: {'unchanged' if meta.get('skipped') else 'prepared'} "
                  f"master {t['master']:,} -> ultra {t['ultra']:,} tris (M1 LOD0 {t['m1_lod0']:,}), "
                  f"collision {meta['collision']['type']}", flush=True)
    report = {"prepared": done, "unchanged": skipped, "no_master_yet": missing,
              "failed": failed}
    os.makedirs(args.staging, exist_ok=True)
    with open(os.path.join(args.staging, "report.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)
    print(json.dumps({k: len(v) for k, v in report.items()}))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
