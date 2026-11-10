"""Procedural greyscale textures of the weather (pure Python, no dependency).

  rain_streaks()  tileable falling-rain streaks for the rain layer material
  puddle_noise()  tileable value noise for the puddle mask of wet materials

Both are generated, not downloaded, so they need no licence entry.
"""

import math
import random
import struct
import zlib


def write_png(path, width, height, pixels):
    """8-bit greyscale PNG; pixels is a flat list of 0..255 values, row by row."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw.extend(bytes(pixels[y * width:(y + 1) * width]))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def rain_streaks(width=128, height=512, count=260, seed=7):
    """Thin vertical streaks with soft ends, wrapping on both axes."""
    rng = random.Random(seed)
    acc = [0.0] * (width * height)
    for _ in range(count):
        x = rng.randrange(width)
        y0 = rng.randrange(height)
        length = rng.randint(18, 70)
        strength = rng.uniform(0.35, 1.0)
        for i in range(length):
            t = i / float(length - 1)
            v = strength * math.sin(math.pi * t) ** 0.7
            y = (y0 + i) % height
            acc[y * width + x] = max(acc[y * width + x], v)
            # one-pixel halo keeps the streak visible after mipmapping
            for dx in (-1, 1):
                k = y * width + (x + dx) % width
                acc[k] = max(acc[k], v * 0.3)
    return width, height, [int(round(255 * min(1.0, a))) for a in acc]


def puddle_noise(size=256, cells=(6, 13), seed=11):
    """Two octaves of smooth value noise, tileable, normalised to 0..255."""
    rng = random.Random(seed)
    octaves = [[[rng.random() for _ in range(c)] for _ in range(c)] for c in cells]

    def smooth(t):
        return t * t * (3 - 2 * t)

    values = []
    for y in range(size):
        for x in range(size):
            v, amp, norm = 0.0, 1.0, 0.0
            for grid in octaves:
                c = len(grid)
                gx, gy = x * c / float(size), y * c / float(size)
                ix, iy = int(gx), int(gy)
                fx, fy = smooth(gx - ix), smooth(gy - iy)
                a, b = grid[iy % c][ix % c], grid[iy % c][(ix + 1) % c]
                d, e = grid[(iy + 1) % c][ix % c], grid[(iy + 1) % c][(ix + 1) % c]
                v += amp * ((a * (1 - fx) + b * fx) * (1 - fy) + (d * (1 - fx) + e * fx) * fy)
                norm += amp
                amp *= 0.45
            values.append(v / norm)
    lo, hi = min(values), max(values)
    return size, size, [int(round(255 * (v - lo) / (hi - lo))) for v in values]
