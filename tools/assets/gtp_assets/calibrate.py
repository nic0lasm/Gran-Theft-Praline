"""Texture recalibration to the art bible (section 4): density, albedo range,
roughness variation, normal convention. All steps are conservative: they only
correct what is out of range and say so in the report."""
from __future__ import annotations

import numpy as np
from PIL import Image

from .geometry import Part

TARGET_DENSITY_PX_CM = 10.24  # within-reach props (art bible 4.4)
MIN_SIZE, MAX_SIZE = 256, 4096
ALBEDO_MIN, ALBEDO_MAX = 30.0, 240.0  # hard limits for non-metals, 8-bit sRGB
ROUGHNESS_MIN_STD = 0.03


def _uv_area(part: Part) -> float:
    uv = part.uv[part.faces]  # (m, 3, 2)
    a, b, c = uv[:, 0], uv[:, 1], uv[:, 2]
    area = 0.5 * np.abs((b[:, 0] - a[:, 0]) * (c[:, 1] - a[:, 1])
                        - (c[:, 0] - a[:, 0]) * (b[:, 1] - a[:, 1]))
    return float(area.sum())


def _world_area_cm2(part: Part) -> float:
    v = part.vertices[part.faces].astype(np.float64)  # metres
    cross = np.cross(v[:, 1] - v[:, 0], v[:, 2] - v[:, 0])
    return float(0.5 * np.linalg.norm(cross, axis=1).sum()) * 1e4


def density_px_cm(part: Part, side_px: int) -> float:
    """Texel density of a square texture of ``side_px`` laid out by the part's UVs."""
    uv, world = _uv_area(part), _world_area_cm2(part)
    if uv <= 1e-9 or world <= 1e-9:
        return 0.0
    return float(side_px * np.sqrt(uv / world))


def target_side(part: Part, target_density: float = TARGET_DENSITY_PX_CM,
                cap: int = MAX_SIZE) -> int:
    """Smallest power-of-two side reaching ``target_density``, within [256, cap]."""
    uv, world = _uv_area(part), _world_area_cm2(part)
    if uv <= 1e-9 or world <= 1e-9:
        return cap
    needed = target_density * np.sqrt(world / uv)
    side = 1 << int(np.ceil(np.log2(max(needed, 1.0))))
    return int(min(max(side, MIN_SIZE), cap))


def flip_normal_green(img: Image.Image) -> Image.Image:
    """glTF normals are OpenGL (+Y up); Unreal expects DirectX (art bible 4.4)."""
    arr = np.asarray(img.convert("RGB")).copy()
    arr[..., 1] = 255 - arr[..., 1]
    return Image.fromarray(arr, "RGB")


def calibrate_albedo(base: Image.Image, orm: Image.Image) -> tuple[Image.Image, dict]:
    """Bring non-metal pixels inside 30-240 (8-bit sRGB luminance) by a linear stretch
    of the 0.5-99.5 percentile range, only if that range falls outside the limits."""
    rgba = np.asarray(base.convert("RGBA")).astype(np.float32)
    rgb = rgba[..., :3]
    metal = np.asarray(orm.resize(base.size, Image.BILINEAR))[..., 2] > 127
    lum = rgb @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    mask = ~metal
    info = {"albedo_range_before": None, "albedo_corrected": False}
    if mask.sum() < 16:
        return base, info
    lo, hi = np.percentile(lum[mask], [0.5, 99.5])
    info["albedo_range_before"] = [round(float(lo), 1), round(float(hi), 1)]
    if lo >= ALBEDO_MIN and hi <= ALBEDO_MAX:
        return base, info
    new_lo, new_hi = max(lo, ALBEDO_MIN), min(hi, ALBEDO_MAX)
    if new_hi - new_lo < 20:  # degenerate range: shift rather than stretch
        new_lo, new_hi = ALBEDO_MIN, ALBEDO_MIN + max(hi - lo, 20.0)
    scale = (new_hi - new_lo) / max(hi - lo, 1e-3)
    out = rgb.copy()
    out[mask] = (rgb[mask] - lo) * scale + new_lo
    rgba[..., :3] = np.clip(out, 0, 255)
    info["albedo_corrected"] = True
    mode = "RGBA" if base.mode == "RGBA" else "RGB"
    return Image.fromarray(rgba.astype(np.uint8), "RGBA").convert(mode), info


def vary_roughness(orm: Image.Image, seed: int = 7) -> tuple[Image.Image, bool]:
    """Roughness must never be a constant (art bible 4.1): add soft, deterministic,
    low-frequency variation (+-0.08) when the map is flat."""
    arr = np.asarray(orm.convert("RGB")).astype(np.float32)
    if arr[..., 1].std() / 255.0 >= ROUGHNESS_MIN_STD:
        return orm, False
    h, w = arr.shape[:2]
    rng = np.random.default_rng(seed)
    coarse = rng.random((max(2, h // 64), max(2, w // 64))).astype(np.float32)
    noise = np.asarray(Image.fromarray(coarse * 255).resize((w, h), Image.BICUBIC),
                       dtype=np.float32) / 255.0 - 0.5
    arr[..., 1] = np.clip(arr[..., 1] + noise * 2 * 0.08 * 255, 0, 255)
    return Image.fromarray(arr.astype(np.uint8), "RGB"), True
