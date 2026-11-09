#!/usr/bin/env python3
"""Summarise Unreal CSV profiler captures against the ADR-0002 budget.

Usage: summarize_csv.py <folder with <State>.csv files>
Prints a Markdown table: frame time and GPU time (median, 95th percentile,
max), the number of hitches above 100 ms and of GPU timer outliers dropped
(above 1000 ms), with pass or fail.
"""

import csv
import os
import statistics
import sys

BUDGET_MS = 33.3
HITCH_MS = 100.0
GPU_OUTLIER_MS = 1000.0  # the GPU timer sometimes returns ~89 000 ms
SKIP_FRAMES = 30  # first second after load


def percentile(values, p):
    values = sorted(values)
    k = (len(values) - 1) * p / 100.0
    lo = int(k)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (k - lo)


def read(path):
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    header = rows[0]
    data = []
    for row in rows[1:]:
        if row and row[0] == header[0]:
            break  # header repeated before the metadata footer ([HasHeaderRowAtEnd])
        if len(row) < len(header) - 1:
            break  # metadata footer
        data.append(row)  # rows without an event have no trailing EVENTS cell
    return header, data[SKIP_FRAMES:]


def column(header, data, *names):
    for name in names:
        if name in header:
            i = header.index(name)
            return [float(r[i]) for r in data if i < len(r) and r[i]]
    for i, h in enumerate(header):
        if any(n.lower() in h.lower() for n in names):
            return [float(r[i]) for r in data if i < len(r) and r[i]]
    return []


def main():
    folder = sys.argv[1]
    print("| State | Frames | Frame p50 (ms) | Frame p95 (ms) | GPU p50 (ms) | GPU p95 (ms) | Max frame (ms) | Hitches > 100 ms | GPU outliers dropped | Result |")
    print("| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |")
    for name in ("Noon", "GoldenHour", "Rain", "Night"):
        path = os.path.join(folder, name + ".csv")
        if not os.path.exists(path):
            continue
        header, data = read(path)
        frame = column(header, data, "FrameTime")
        gpu = column(header, data, "GPUTime", "GPU/Total")
        outliers = sum(1 for v in gpu if v > GPU_OUTLIER_MS)
        gpu = [v for v in gpu if v <= GPU_OUTLIER_MS]
        if not frame:
            print("| %s | 0 | - | - | - | - | - | - | %d | no data |" % (name, outliers))
            continue
        gpu_p95 = percentile(gpu, 95) if gpu else float("nan")
        hitches = sum(1 for v in frame if v > HITCH_MS)
        ok = (gpu_p95 <= BUDGET_MS if gpu else percentile(frame, 95) <= BUDGET_MS) and hitches == 0
        print("| %s | %d | %.1f | %.1f | %s | %s | %.1f | %d | %d | %s |" % (
            name, len(frame), statistics.median(frame), percentile(frame, 95),
            "%.1f" % statistics.median(gpu) if gpu else "-", "%.1f" % gpu_p95 if gpu else "-",
            max(frame), hitches, outliers, "pass" if ok else "FAIL"))
    print()
    print("Budget: GPU p95 <= %.1f ms (30 fps), no frame above %.0f ms (ADR-0002)." % (BUDGET_MS, HITCH_MS))


if __name__ == "__main__":
    main()
