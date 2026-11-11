#!/usr/bin/env python3
"""Luminance statistics of a still (JPEG from -CaptureVideo), standard library only.

Usage: still_stats.py <image.jpg> [flat.jpg]
Prints: mean ground_mean ground_patch [spots]  (0-255, Rec. 601 luma)
  mean          whole image (same measure as mean-luma.swift, within 1)
  ground_mean   lower half of the image (street, facades' feet)
  ground_patch  standard deviation of 16 x 6 block means over the lower half: the
                large-scale light variation that cloud shadows add (texture detail
                averages out inside a block). Dominated by the scene's geometry with a
                fixed camera: use spots to judge cloud shadows.
  spots         with a flat still (same view, no cloud shadow at all): standard deviation
                in % of the per-block ratio image / flat, normalised to its mean. The
                geometry cancels out, what remains is the cloud shadow pattern.
On macOS the JPEG is decoded by sips (built in) to a BMP beside it, removed after; elsewhere
Pillow is used. Nothing is written outside the folder of the still.
"""
import os
import shutil
import struct
import subprocess
import sys


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp not in (24, 32):
        raise ValueError("unsupported BMP depth %d" % bpp)
    step = bpp // 8
    row = (width * step + 3) & ~3
    top_down = height < 0
    height = abs(height)
    luma = []
    for y in range(height):
        src = y if top_down else height - 1 - y
        base = offset + src * row
        line = data[base:base + width * step]
        luma.append([0.114 * line[i] + 0.587 * line[i + 1] + 0.299 * line[i + 2]
                     for i in range(0, width * step, step)])
    return luma


def load(path):
    if shutil.which("sips"):
        # Decoded next to the still (inside the run folder), never in the system temp.
        bmp = path + ".stats.bmp"
        try:
            subprocess.run(["sips", "-Z", "480", "-s", "format", "bmp", path, "--out", bmp],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return read_bmp(bmp)
        finally:
            if os.path.exists(bmp):
                os.remove(bmp)
    from PIL import Image  # noqa: PLC0415 (fallback off macOS)
    img = Image.open(path).convert("RGB")
    img.thumbnail((480, 480))
    w, h = img.size
    px = img.load()
    return [[0.299 * px[x, y][0] + 0.587 * px[x, y][1] + 0.114 * px[x, y][2] for x in range(w)]
            for y in range(h)]


def blocks_of(luma, bx=16, by=6):
    h, w = len(luma), len(luma[0])
    ground = luma[h // 2:]
    gh = len(ground)
    out = []
    for j in range(by):
        rows = ground[j * gh // by:(j + 1) * gh // by]
        for i in range(bx):
            x0, x1 = i * w // bx, (i + 1) * w // bx
            cells = [v for r in rows for v in r[x0:x1]]
            out.append(sum(cells) / len(cells))
    return out


def spots(luma, flat):
    a, b = blocks_of(luma), blocks_of(flat)
    ratios = [x / y for x, y in zip(a, b) if y > 2.0]
    if len(ratios) < 10:
        return float("nan")
    m = sum(ratios) / len(ratios)
    return 100.0 * (sum((r / m - 1.0) ** 2 for r in ratios) / len(ratios)) ** 0.5


def stats(luma):
    h, w = len(luma), len(luma[0])
    mean = sum(map(sum, luma)) / (w * h)
    ground = luma[h // 2:]
    gh = len(ground)
    ground_mean = sum(map(sum, ground)) / (w * gh)
    bx, by = 16, 6
    blocks = []
    for j in range(by):
        rows = ground[j * gh // by:(j + 1) * gh // by]
        for i in range(bx):
            x0, x1 = i * w // bx, (i + 1) * w // bx
            cells = [v for r in rows for v in r[x0:x1]]
            blocks.append(sum(cells) / len(cells))
    m = sum(blocks) / len(blocks)
    patch = (sum((b - m) ** 2 for b in blocks) / len(blocks)) ** 0.5
    return mean, ground_mean, patch


def main():
    flat_path = sys.argv[2] if len(sys.argv) > 2 else None
    tail = " nan" if flat_path else ""
    if len(sys.argv) < 2 or not os.path.isfile(sys.argv[1]):
        print("nan nan nan" + tail)
        return
    try:
        luma = load(sys.argv[1])
        line = "%.0f %.0f %.1f" % stats(luma)
        if flat_path:
            line += " %.1f" % (spots(luma, load(flat_path)) if os.path.isfile(flat_path) else float("nan"))
        print(line)
    except Exception as exc:  # a broken still must not stop the pass
        sys.stderr.write("still_stats: %s\n" % exc)
        print("nan nan nan" + tail)


if __name__ == "__main__":
    main()
