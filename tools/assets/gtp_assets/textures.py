"""Extract PBR textures from a glTF material into Unreal-friendly PNG maps.

Outputs per material slot:
  BC (sRGB base color), N (tangent-space normal, OpenGL/+Y up as in glTF),
  ORM = R occlusion, G roughness, B metallic (linear). Same packing as glTF,
  so Unreal reads it without swizzling.
Missing maps become 4x4 constants built from the material factors.
"""
from __future__ import annotations

import os

import numpy as np
from PIL import Image

from . import calibrate

MAX_TEXTURE_SIZE = 4096  # cap only; textures are never downscaled below this


def _img(tex):
    if tex is None:
        return None
    return tex if isinstance(tex, Image.Image) else None


def _solid(rgba, size=4) -> Image.Image:
    arr = np.zeros((size, size, 4), dtype=np.uint8)
    arr[:] = np.clip(np.array(rgba) * 255 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(arr, "RGBA")


def _cap(img: Image.Image, cap: int) -> Image.Image:
    if max(img.size) <= cap:
        return img
    f = cap / max(img.size)
    return img.resize((max(1, round(img.width * f)), max(1, round(img.height * f))),
                      Image.LANCZOS)


def extract_material(material, out_dir: str, stem: str, cap: int = MAX_TEXTURE_SIZE,
                     calibrate_maps: bool = True) -> dict:
    """Write the three maps for one material; return its description."""
    os.makedirs(out_dir, exist_ok=True)
    base = _img(getattr(material, "baseColorTexture", None))
    mr = _img(getattr(material, "metallicRoughnessTexture", None))
    occ = _img(getattr(material, "occlusionTexture", None))
    nrm = _img(getattr(material, "normalTexture", None))

    factor = getattr(material, "baseColorFactor", None)
    factor = np.array(factor if factor is not None else [255, 255, 255, 255], dtype=float)
    factor = factor / 255.0 if factor.max() > 1.0 else factor
    rough_f = getattr(material, "roughnessFactor", None)
    metal_f = getattr(material, "metallicFactor", None)
    rough_f = 1.0 if rough_f is None else float(rough_f)
    metal_f = 1.0 if metal_f is None else float(metal_f)

    paths = {}
    # Base color: keep alpha only if the material really blends or masks.
    alpha_mode = getattr(material, "alphaMode", None) or "OPAQUE"
    if base is not None:
        b = _cap(base.convert("RGBA"), cap)
        if alpha_mode == "OPAQUE":
            b = b.convert("RGB")
    else:
        b = _solid(factor)
        if alpha_mode == "OPAQUE":
            b = b.convert("RGB")

    # ORM: R=occlusion (white if none), G=roughness, B=metallic.
    size = max([i.size for i in (mr, occ) if i is not None], default=(4, 4), key=max)
    orm = np.full((size[1], size[0], 3), 255, dtype=np.float32)
    if occ is not None:
        o = np.asarray(occ.convert("RGB").resize(size, Image.BILINEAR), dtype=np.float32)
        orm[..., 0] = o[..., 0]
    if mr is not None:
        m = np.asarray(mr.convert("RGB").resize(size, Image.BILINEAR), dtype=np.float32)
        orm[..., 1] = m[..., 1] * rough_f
        orm[..., 2] = m[..., 2] * metal_f
    else:
        orm[..., 1] = rough_f * 255
        orm[..., 2] = metal_f * 255
    orm_img = _cap(Image.fromarray(np.clip(orm + 0.5, 0, 255).astype(np.uint8), "RGB"), cap)
    report = {}
    if calibrate_maps:
        if base is not None:  # constants from factors are the author's intent: leave them
            b, report = calibrate.calibrate_albedo(b, orm_img)
        orm_img, varied = calibrate.vary_roughness(orm_img)
        report["roughness_varied"] = varied

    paths["base_color"] = f"{stem}_BC.png"
    b.save(os.path.join(out_dir, paths["base_color"]))
    paths["orm"] = f"{stem}_ORM.png"
    orm_img.save(os.path.join(out_dir, paths["orm"]))

    if nrm is not None:
        n = calibrate.flip_normal_green(_cap(nrm.convert("RGB"), cap))
    else:
        n = _solid([0.5, 0.5, 1.0, 1.0]).convert("RGB")
    paths["normal"] = f"{stem}_N.png"
    n.save(os.path.join(out_dir, paths["normal"]))

    return {
        "textures": paths,
        "size_px": [b.width, b.height],
        "calibration": report,
        "alpha_mode": alpha_mode,
        "double_sided": bool(getattr(material, "doubleSided", False)),
        "has_source_maps": {
            "base_color": base is not None, "metal_rough": mr is not None,
            "occlusion": occ is not None, "normal": nrm is not None,
        },
    }
