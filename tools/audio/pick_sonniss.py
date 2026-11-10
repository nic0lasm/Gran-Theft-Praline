"""Choose recorded sounds for the banks from a library folder (Sonniss GDC bundle, CC0 packs).

Walks LIBRARY for WAV files, sorts them into banks by the words of their
folder and file names (footstep + surface, car + crash, glass + break...),
and writes a pick list for stage_recorded.py. Footstep recordings usually
hold a whole walk: each file is cut into single steps at its onsets.

  python3 -I tools/audio/pick_sonniss.py LIBRARY picks.csv [--max-files 12] [--max-steps 16]
  python3 -I tools/audio/stage_recorded.py picks.csv .scratch/audio/sfx-recorded

A report (picks.csv.txt) lists what each bank got, so a human can listen to a
bank and drop what does not fit before importing. Names are only a guess:
nothing enters a release unheard (docs/audio/audio-design.md).
"""

import argparse
import csv
import os
import re
import sys
import wave

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from stage_recorded import read_pcm  # noqa: E402

FOOT = {"footstep", "footsteps", "step", "steps", "walk", "walking", "run", "running", "jog", "shoe", "shoes", "feet", "foot"}
SURFACE_WORDS = [
    ("Cobble", {"cobble", "cobbles", "cobblestone", "cobblestones", "pave", "setts"}),
    ("Gravel", {"gravel", "pebbles", "sand"}),
    ("Grass", {"grass", "lawn"}),
    ("Metal", {"metal", "steel", "grate", "grating", "iron", "manhole"}),
    ("Wood", {"wood", "wooden", "plank", "planks", "deck", "boardwalk"}),
    ("Water", {"water", "puddle", "puddles", "splash", "wet", "mud"}),
    ("Stone", {"stone", "tile", "tiles", "marble", "slab", "slabs", "granite"}),
    ("Asphalt", {"asphalt", "concrete", "tarmac", "road", "street", "pavement", "sidewalk", "cement"}),
]
CAR = {"car", "cars", "vehicle", "auto", "automobile", "sedan", "van"}
IMPACT_HEAVY = {"crash", "collision", "wreck", "smash"}
IMPACT_MEDIUM = {"impact", "hit", "bang", "slam"}
IMPACT_LIGHT = {"bump", "knock", "tap", "thud"}
EXCLUDE = {"loop", "ambience", "ambiance", "amb", "music", "drone", "engine", "idle", "interior", "onboard"}


def words(path):
    return set(re.split(r"[^a-z0-9]+", path.lower())) - {""}


def bank_of(path):
    w = words(path)
    if w & EXCLUDE:
        return None
    if w & FOOT and not (w & (CAR | {"glass"})):
        for surface, keys in SURFACE_WORDS:
            if w & keys:
                return "Footsteps/" + surface
        return None
    if "glass" in w and w & {"break", "breaking", "shatter", "smash", "crash"}:
        return "Impacts/Glass"
    tier = "Heavy" if w & IMPACT_HEAVY else "Medium" if w & IMPACT_MEDIUM else "Light" if w & IMPACT_LIGHT else None
    if tier is None:
        return None
    if w & CAR:
        return f"Impacts/Car/{tier}"
    if w & {"body", "punch", "flesh", "fall", "bodyfall"}:
        return f"Impacts/Body/{tier}"
    if w & {"metal", "steel", "pole", "railing", "iron"}:
        return f"Impacts/Metal/{tier}"
    if w & {"wood", "wooden", "plank", "crate"}:
        return f"Impacts/Wood/{tier}"
    if w & {"concrete", "stone", "brick", "wall", "rock"}:
        return f"Impacts/Wall/{tier}"
    return None


def duration(path):
    try:
        with wave.open(path) as w:
            return w.getnframes() / float(w.getframerate())
    except (wave.Error, EOFError, OSError):
        return None


def onsets(x, rate, min_gap=0.25, rel=0.25):
    """Start times (s) of the loud events of a recording."""
    hop = max(1, int(0.005 * rate))
    n = len(x) // hop
    if n < 3:
        return []
    env = np.abs(x[: n * hop]).reshape(n, hop).max(axis=1)
    level = rel * env.max()
    out, last = [], -1e9
    for i in range(1, n):
        if env[i] >= level and env[i - 1] < level and (i * hop / rate - last) >= min_gap:
            last = i * hop / rate
            out.append(last)
    return out


def slices(path, max_steps):
    x, rate = read_pcm(path)
    starts = onsets(x, rate)
    total = len(x) / rate
    out = []
    for k, s in enumerate(starts):
        end = starts[k + 1] - 0.02 if k + 1 < len(starts) else total
        length = min(0.45, end - s + 0.005)
        if length > 0.08:
            out.append((max(0.0, s - 0.005), length))
        if len(out) >= max_steps:
            break
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("library")
    ap.add_argument("picks")
    ap.add_argument("--max-files", type=int, default=12)
    ap.add_argument("--max-steps", type=int, default=16)
    args = ap.parse_args()

    found = {}
    for folder, _dirs, files in os.walk(args.library):
        for name in sorted(files):
            if not name.lower().endswith(".wav"):
                continue
            path = os.path.join(folder, name)
            bank = bank_of(os.path.relpath(path, args.library))
            if bank:
                found.setdefault(bank, []).append(path)

    rows, report = [], []
    for bank in sorted(found):
        paths = found[bank]
        # Shorter files first: single hits rather than long sequences or loops.
        paths = sorted(paths, key=lambda p: duration(p) or 1e9)[: args.max_files]
        count = 0
        for path in paths:
            if bank.startswith("Footsteps/"):
                for start, length in slices(path, args.max_steps - count):
                    rows.append((path, bank, f"{start:.3f}", f"{length:.3f}"))
                    count += 1
            elif (duration(path) or 0) <= 6.0:
                rows.append((path, bank, "", ""))
                count += 1
            if count >= (args.max_steps if bank.startswith("Footsteps/") else args.max_files):
                break
        report.append(f"{bank}: {count} picks from {len(found[bank])} candidate files")

    with open(args.picks, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["source", "bank", "start_s", "length_s"])
        writer.writerows(rows)
    with open(args.picks + ".txt", "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report) or "nothing matched")
    print(f"{len(rows)} picks written to {args.picks}")


if __name__ == "__main__":
    main()
