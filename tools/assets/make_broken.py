#!/usr/bin/env python3
"""Write the broken variants (gtp_assets/broken.py) of prepared props into their staging folder.

For every catalogue id of broken.BROKEN found in the staging folder, reads <id>/<id>_ultra.glb
(prepared by prepare_props.py), writes <id>/broken/<id>_<Variant>.glb and lists them under
"broken" in <id>/asset.json. tools/assets/unreal/import_props.py then imports each one as
SM_<Asset>_<Variant> next to SM_<Asset>_A, with the same material instances.

  python3 tools/assets/make_broken.py --staging assets/staging/w1 [--ids W1-04 W1-12]
"""

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gtp_assets.broken import BROKEN, variants  # noqa: E402
from gtp_assets.geometry import export_geometry_glb, load_glb  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--staging", required=True, nargs="+", help="one or more staging folders (w1, w8...)")
    ap.add_argument("--ids", nargs="*", default=None)
    args = ap.parse_args()
    done, missing = [], []
    for aid in sorted(args.ids or BROKEN):
        meta_path = next((os.path.join(s, aid, "asset.json") for s in args.staging
                          if os.path.exists(os.path.join(s, aid, "asset.json"))), None)
        if meta_path is None:
            missing.append(aid)
            continue
        folder = os.path.dirname(meta_path)
        with open(meta_path, encoding="utf-8") as f:
            meta = json.load(f)
        asset = load_glb(os.path.join(folder, meta["files"]["ultra"]))
        slots = ["M_Imported_%s_%d" % (aid, i) for i in range(len(asset.parts))]
        os.makedirs(os.path.join(folder, "broken"), exist_ok=True)
        meta["broken"] = {}
        for name, (piece, keep) in variants(aid, asset).items():
            rel = os.path.join("broken", "%s_%s.glb" % (aid, name))
            export_geometry_glb(piece, os.path.join(folder, rel), [slots[i] for i in keep])
            meta["broken"][name] = {"file": rel, "triangles": piece.tri_count, "slots": keep}
        with open(meta_path, "w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)
        done.append("%s %s" % (aid, "+".join(meta["broken"])))
    print(json.dumps({"written": done, "not_staged": missing}, indent=1))


if __name__ == "__main__":
    main()
