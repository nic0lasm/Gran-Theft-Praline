#!/usr/bin/env python3
"""Scan a game log for errors: the log part of the automatic smoke test.

A packaged Test-configuration build writes Saved/Logs/GranTheftPraline.log.
This tool fails when the log holds fatal errors, failed ensures, crashes or
unexpected Error lines, after removing the patterns listed in an allow-list
file (one regular expression per line, # comments) for known benign noise.

Usage: check_game_log.py LOG [--allow FILE] [--max-warnings N] [--min-lines N]
Exit: 0 pass, 1 fail, 2 unreadable log.
"""
import argparse
import re
import sys

FATAL = re.compile(r"Fatal error|Assertion failed|Ensure condition failed|Unhandled Exception|"
                   r"LogWindows: Error: appError|Critical error|Signal 11|SIGSEGV|EXC_BAD_ACCESS", re.I)
ERROR = re.compile(r"^\[[^\]]*\]\[[^\]]*\]\s*\w+: Error: |^\w+: Error: ")
WARNING = re.compile(r"^\[[^\]]*\]\[[^\]]*\]\s*\w+: Warning: |^\w+: Warning: ")


def load_allow(path):
    patterns = []
    if path:
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    patterns.append(re.compile(line))
    return patterns


def scan(lines, allow):
    """Return (fatals, errors, warnings, total_lines); allow-listed lines are ignored."""
    fatals, errors, warnings = [], [], []
    for line in lines:
        if any(p.search(line) for p in allow):
            continue
        if FATAL.search(line):
            fatals.append(line.rstrip())
        elif ERROR.search(line):
            errors.append(line.rstrip())
        elif WARNING.search(line):
            warnings.append(line.rstrip())
    return fatals, errors, warnings


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("log")
    parser.add_argument("--allow")
    parser.add_argument("--max-warnings", type=int, default=-1, help="-1: do not limit")
    parser.add_argument("--min-lines", type=int, default=50,
                        help="a log shorter than this means the game did not really run")
    args = parser.parse_args(argv)
    try:
        with open(args.log, encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
        allow = load_allow(args.allow)
    except OSError as err:
        print(f"check_game_log: {err}", file=sys.stderr)
        return 2
    fatals, errors, warnings = scan(lines, allow)
    print(f"lines={len(lines)} fatals={len(fatals)} errors={len(errors)} warnings={len(warnings)}")
    for line in (fatals + errors)[:20]:
        print(f"  {line}")
    ok = not fatals and not errors and len(lines) >= args.min_lines
    if 0 <= args.max_warnings < len(warnings):
        ok = False
    if len(lines) < args.min_lines:
        print(f"log too short ({len(lines)} < {args.min_lines} lines)")
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
