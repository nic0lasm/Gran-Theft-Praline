#!/usr/bin/env python3
"""Non-regression gate: compare two bench results.csv files.

Usage: compare.py <reference results.csv> <new results.csv> [max_regression_ms]
Exits 1 when, for any scenario/variant present in both, the GPU or frame p95
grew by more than max_regression_ms (default 2), or hitches above 100 ms grew.
"""
import csv
import sys


def load(path):
    with open(path, newline="") as f:
        return {(r["scenario"], r["variant"]): r for r in csv.DictReader(f)}


def num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def main():
    ref, new = load(sys.argv[1]), load(sys.argv[2])
    limit = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0
    failed = False
    print()
    print("| Scenario | Variant | GPU p95 ref -> new | Frame p95 ref -> new | Hitches ref -> new | Gate |")
    print("| ---- | ---- | ---- | ---- | ---- | ---- |")
    for key in sorted(new):
        if key not in ref:
            continue
        r, n = ref[key], new[key]
        bad = []
        for col in ("gpu_p95", "frame_p95"):
            a, b = num(r[col]), num(n[col])
            if a is not None and b is not None and b - a > limit:
                bad.append(col)
        if int(n["hitches"]) > int(r["hitches"]):
            bad.append("hitches")
        failed = failed or bool(bad)
        print("| %s | %s | %s -> %s | %s -> %s | %s -> %s | %s |" % (
            key[0], key[1], r["gpu_p95"], n["gpu_p95"], r["frame_p95"], n["frame_p95"],
            r["hitches"], n["hitches"], "FAIL (" + ", ".join(bad) + ")" if bad else "pass"))
    print()
    print("Gate: p95 regression <= %.1f ms, no new hitch > 100 ms: %s" % (limit, "FAIL" if failed else "pass"))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
