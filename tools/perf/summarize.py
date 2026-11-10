#!/usr/bin/env python3
"""One Markdown table row for a bench run.

Usage: summarize.py <frametimes.csv> <scenario> <variant>
Reads the CSV written by UGTPFrameTimeRecorder (columns frame_ms and gpu_ms).
GPU timer outliers above 1000 ms are dropped, like the other tools.
"""
import csv
import sys

BUDGET_MS = 33.3
HITCH_MS = 100.0
GPU_OUTLIER_MS = 1000.0


def pct(values, p):
    values = sorted(values)
    k = (len(values) - 1) * p / 100.0
    lo = int(k)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (k - lo)


def main():
    path, scenario, variant = sys.argv[1:4]
    frame, gpu = [], []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            try:
                frame.append(float(row["frame_ms"]))
                g = float(row["gpu_ms"])
            except (KeyError, ValueError):
                continue
            if 0 <= g <= GPU_OUTLIER_MS:
                gpu.append(g)
    if not frame:
        raise SystemExit("no frames in %s" % path)
    within = 100.0 * sum(1 for v in frame if v <= BUDGET_MS) / len(frame)
    hitches = sum(1 for v in frame if v > HITCH_MS)
    print("| %s | %s | %.1f | %.1f | %.1f | %.1f | %.0f %% | %d |" % (
        scenario, variant,
        pct(gpu, 50) if gpu else float("nan"), pct(gpu, 95) if gpu else float("nan"),
        pct(frame, 50), pct(frame, 95), within, hitches))


if __name__ == "__main__":
    main()
