"""Wave manifests: what to prepare and at which real-world size."""
from __future__ import annotations

import json
import re

# Per-object overrides on top of the catalog (wave 1). Everything else uses
# the defaults: bottom-centre pivot, automatic collision, tri budget by size.
WAVE1_OVERRIDES = {
    "W1-06": {"pivot": "back_center"},  # wall-mounted street light
    "W1-45": {"pivot": "back_center"},  # retracted awning
    "W1-46": {"pivot": "back_center"},  # extended awning
    "W1-47": {"pivot": "back_center"},  # facade air conditioner
    "W1-48": {"pivot": "back_center"},  # balcony railing
    "W1-58": {"pivot": "back_center"},  # surveillance camera
    "W1-59": {"pivot": "back_center"},  # wall ATM
    "W1-28": {"collision": "none"},  # manhole cover
    "W1-29": {"collision": "none"},  # storm drain grate
    "W1-17": {"collision": "none"},  # tree grate
    "W1-56": {"collision": "none"},  # street name sign (wall plate)
}

_ROW = re.compile(r"^\|\s*(W\d+-\d+)\s*\|\s*([^|]+?)\s*\|\s*([^|]*?)\s*\|\s*([^|]*?)\s*\|")


def slugify(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def parse_catalog(md_text: str, wave_prefix: str = "W1") -> list[dict]:
    """Read the asset-catalog.md rows of one wave. Size = longest dimension."""
    items = []
    for line in md_text.splitlines():
        m = _ROW.match(line)
        if not m or not m.group(1).startswith(wave_prefix + "-"):
            continue
        asset_id, name, _notes, size = m.groups()
        nums = [float(n) for n in re.findall(r"\d+(?:\.\d+)?", size)]
        if not nums:
            continue
        item = {"id": asset_id, "slug": slugify(name), "name": name,
                "size_m": max(nums)}
        item.update(WAVE1_OVERRIDES.get(asset_id, {}))
        items.append(item)
    return items


def default_m1_tris(size_m: float) -> int:
    """Triangle budget of the M1 base mesh by object size."""
    return 6000 if size_m < 1.0 else 15000 if size_m < 3.0 else 30000


# Auto-generated LODs in Unreal, as fractions of the M1 base mesh.
LOD_PLAN = [
    {"percent_triangles": 1.0, "screen_size": 1.0},
    {"percent_triangles": 0.35, "screen_size": 0.45},
    {"percent_triangles": 0.12, "screen_size": 0.20},
    {"percent_triangles": 0.04, "screen_size": 0.08},
]


def load_manifest(path: str) -> dict:
    with open(path, encoding="utf-8") as f:
        return json.load(f)
