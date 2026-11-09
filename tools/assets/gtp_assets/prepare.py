"""Prepare one Tripo master for Unreal.

Output layout (one folder per asset, under the staging root):
  <id>/asset.json              what Unreal's import script reads
  <id>/<id>_ultra.glb          normalised geometry, capped to the art bible Nanite budget
  <id>/textures/*.png          BC / ORM / N per material slot (art bible suffixes)

One static mesh per prop in Unreal: Nanite source (Ultra), Nanite fallback set to
the M1 LOD0 budget, LOD chain halving from there (art bible sections 5 and 6).
"""
from __future__ import annotations

import hashlib
import json
import os
from typing import Optional

from . import __version__
from .calibrate import density_px_cm, target_side
from .collision import recipe as collision_recipe
from .geometry import decimate, export_geometry_glb, load_glb, normalize
from .manifest import BUDGETS, category, lod_plan
from .textures import extract_material


def file_sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def settings_key(item: dict) -> str:
    keys = {k: item.get(k) for k in ("size_m", "pivot", "rotate_y_deg", "collision",
                                     "m1_tris", "ultra_tris", "family", "asset")}
    return json.dumps(keys, sort_keys=True) + __version__


def prepare_asset(src_glb: str, item: dict, staging_root: str, wave: str = "w1",
                  force: bool = False) -> dict:
    """Run the full preparation. Skips work when source and settings are unchanged."""
    asset_id = item["id"]
    out = os.path.join(staging_root, asset_id)
    meta_path = os.path.join(out, "asset.json")
    sha = file_sha256(src_glb)
    if not force and os.path.exists(meta_path):
        with open(meta_path, encoding="utf-8") as f:
            old = json.load(f)
        if old.get("source_sha256") == sha and old.get("settings_key") == settings_key(item):
            old["skipped"] = True
            return old

    os.makedirs(os.path.join(out, "textures"), exist_ok=True)
    asset = load_glb(src_glb)
    master_tris = asset.tri_count
    size_m = float(item["size_m"])
    scale = normalize(asset, size_m, item.get("pivot", "bottom_center"),
                      float(item.get("rotate_y_deg", 0.0)))

    cat = category(size_m)
    budget = BUDGETS[cat]
    ultra_cap = int(item.get("ultra_tris") or budget["ultra_tris"])
    m1_target = int(item.get("m1_tris") or budget["m1_tris"])
    master_tris = asset.tri_count
    ultra = decimate(asset, ultra_cap)
    proxy = decimate(ultra, m1_target)  # M1-sized proxy, only used for the collision recipe

    slots = [f"M_Imported_{asset_id}_{i}" for i in range(len(ultra.parts))]
    export_geometry_glb(ultra, os.path.join(out, f"{asset_id}_ultra.glb"), slots)

    family = item.get("family", "Props")
    asset_name = item.get("asset") or item.get("slug", asset_id)
    materials = []
    for i, part in enumerate(ultra.parts):
        suffix = f"_{i}" if len(ultra.parts) > 1 else ""
        # Art bible 4.4: texture side = what the target density needs (never upscaled).
        side = target_side(part, float(item.get("density_px_cm", 10.24)))
        info = extract_material(part.material, os.path.join(out, "textures"),
                                f"T_{asset_name}{suffix}", cap=side)
        info["density_px_cm"] = round(density_px_cm(part, max(info["size_px"])), 2)
        info["slot"] = i
        info["imported_name"] = slots[i]
        info["instance_name"] = f"MI_{asset_name}{suffix}"
        materials.append(info)

    lo, hi = ultra.bounds()
    meta = {
        "id": asset_id,
        "slug": item.get("slug", asset_id.lower()),
        "name": item.get("name", asset_id),
        "family": family,
        "asset_name": asset_name,
        "category": cat,
        "wave": wave,
        "source": os.path.basename(src_glb),
        "source_sha256": sha,
        "settings_key": settings_key(item),
        "tool_version": __version__,
        "scale_applied": scale,
        "size_m": size_m,
        "bounds_m": {"min": [float(x) for x in lo], "max": [float(x) for x in hi]},
        "triangles": {"master": master_tris, "ultra": ultra.tri_count, "m1_lod0": m1_target},
        "files": {"ultra": f"{asset_id}_ultra.glb"},
        "lod_plan": lod_plan(ultra.tri_count, m1_target, budget["lods"]),
        "sloppy_fallback": bool(ultra.used_sloppy),
        "collision": collision_recipe(proxy, item.get("collision", "auto"), cat),
        "materials": materials,
    }
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)
    meta["skipped"] = False
    return meta


def find_master(masters_dir: str, asset_id: str) -> Optional[str]:
    """Masters are named ``W1-07.glb`` or start with the id (``W1-07_bench.glb``)."""
    if not os.path.isdir(masters_dir):
        return None
    for name in sorted(os.listdir(masters_dir)):
        if name.lower().endswith(".glb") and name.upper().startswith(asset_id.upper()):
            return os.path.join(masters_dir, name)
    return None
