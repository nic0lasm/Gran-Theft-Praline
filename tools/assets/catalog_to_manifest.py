#!/usr/bin/env python3
"""Turn a wave table of production/asset-catalog.md into a manifest JSON."""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gtp_assets.manifest import parse_catalog  # noqa: E402


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--catalog", default="production/asset-catalog.md")
    ap.add_argument("--wave", default="W1", help="id prefix, e.g. W1")
    ap.add_argument("--out", required=True)
    args = ap.parse_args(argv)
    with open(args.catalog, encoding="utf-8") as f:
        items = parse_catalog(f.read(), args.wave)
    if not items:
        print(f"no {args.wave} rows found in {args.catalog}", file=sys.stderr)
        return 1
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump({"wave": args.wave.lower(), "items": items}, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"{len(items)} items -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
