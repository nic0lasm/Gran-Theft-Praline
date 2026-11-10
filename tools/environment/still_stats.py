#!/usr/bin/env python3
"""Luminance statistics of a still (JPEG from -CaptureVideo), standard library only.

Usage: still_stats.py <image.jpg>
Prints: mean ground_mean ground_patch  (0-255, Rec. 601 luma)
  mean          whole image (same measure as mean-luma.swift, within 1)
  ground_mean   lower half of the image (street, facades' feet)
  ground_patch  standard deviation of 16 x 6 block means over the lower half: the
                large-scale light variation that cloud shadows add (texture detail
                averages out inside a block). Compare with the reference sky.
On macOS the JPEG is decoded by sips (built in) to a BMP; elsewhere Pillow is used.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile


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
        tmp = tempfile.mkdtemp()
        try:
            bmp = os.path.join(tmp, "still.bmp")
            subprocess.run(["sips", "-Z", "480", "-s", "format", "bmp", path, "--out", bmp],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            return read_bmp(bmp)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    from PIL import Image  # noqa: PLC0415 (fallback off macOS)
    img = Image.open(path).convert("RGB")
    img.thumbnail((480, 480))
    w, h = img.size
    px = img.load()
    return [[0.299 * px[x, y][0] + 0.587 * px[x, y][1] + 0.114 * px[x, y][2] for x in range(w)]
            for y in range(h)]


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
    if len(sys.argv) != 2 or not os.path.isfile(sys.argv[1]):
        print("nan nan nan")
        return
    try:
        print("%.0f %.0f %.1f" % stats(load(sys.argv[1])))
    except Exception as exc:  # a broken still must not stop the pass
        sys.stderr.write("still_stats: %s\n" % exc)
        print("nan nan nan")


if __name__ == "__main__":
    main()
