#!/usr/bin/env python3
"""Per-pass GPU breakdown from an Unreal CSV profiler capture.

Usage: gpu_passes.py <capture.csv> [skip_frames]
Reads the GPU/* columns written with -csvGpuStats (Development or Test
builds) and prints a Markdown table of passes sorted by median cost:
median, 95th percentile and share of the GPU frame. The first skip_frames
frames (default 300, about 10 s) are left out: loading and PSO warm-up.
"""
import csv
import statistics
import sys


def pct(values, p):
    values = sorted(values)
    k = (len(values) - 1) * p / 100.0
    lo = int(k)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (k - lo)


def main():
    path = sys.argv[1]
    skip = int(sys.argv[2]) if len(sys.argv) > 2 else 300
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    header = rows[0]
    data = []
    for row in rows[1:]:
        if row and row[0] == header[0]:
            break  # header repeated before the metadata footer
        if len(row) < len(header) - 1:
            break
        data.append(row)
    data = data[skip:]
    cols = [(i, h) for i, h in enumerate(header) if h.startswith("GPU/")]
    if not cols:
        raise SystemExit("no GPU/* column: was the capture made with -csvGpuStats on a Development/Test build?")
    total_i = next((i for i, h in cols if h in ("GPU/Total", "GPU/total")), None)
    stats = []
    for i, h in cols:
        vals = []
        for r in data:
            try:
                vals.append(float(r[i]))
            except (IndexError, ValueError):
                pass
        vals = [v for v in vals if 0 <= v < 1000]
        if vals:
            stats.append((h, statistics.median(vals), pct(vals, 95)))
    total = next((m for h, m, _ in stats if total_i is not None and h == header[total_i]), None)
    stats.sort(key=lambda s: -s[1])
    print("| Pass | Median (ms) | p95 (ms) | Share of GPU frame |")
    print("| ---- | ---- | ---- | ---- |")
    for h, m, p in stats:
        share = "%.0f %%" % (100 * m / total) if total and h != header[total_i] else ""
        print("| %s | %.2f | %.2f | %s |" % (h[4:], m, p, share))
    print()
    print("%d frames analysed (%d skipped)." % (len(data), skip))


if __name__ == "__main__":
    main()
