"""Reads the logs and frame times of the audio pass (tools/audio/mac_audio_pass.sh check).

From the video run log (-DebugFeet -AudioStats):
- steps of the player per second while walking, the surfaces stepped on;
- foot heights above the ground: the lowest values are the planted foot, so
  FootPlantHeight should sit a few cm above them (suggestion printed);
- the last "[Audio] steps=... impacts=... sirens_on=..." counters.
From the two soak runs (-Audio=0 and on): game thread p50 / p95 difference,
against the 0.3 ms budget of ADR-0009.
Writes <dir>/audio-report.md and prints it. Exit code 1 if a check fails.

Usage: python3 -I tools/audio/analyze_audio_run.py <evidence dir>
"""

import csv
import os
import re
import sys

import numpy as np

BUDGET_MS = 0.3


def game_ms(path):
    with open(path, newline="") as f:
        rows = list(csv.reader(f))
    values = []
    for row in rows:
        try:
            values.append(float(row[3]))
        except (ValueError, IndexError):
            continue
    return np.array(values)


def main(folder):
    lines, failures = [], []
    log_path = os.path.join(folder, "video-run.log")
    text = open(log_path, errors="replace").read() if os.path.isfile(log_path) else ""

    feet = [(m.group(1), float(m.group(2)), float(m.group(3)), float(m.group(4)))
            for m in re.finditer(r"\[Feet\] ([LR]) h=(-?[\d.]+) speed=([\d.]+) t=([\d.]+)", text)]
    steps = re.findall(r"\[Step\] (step|land) (\w+) gain=([\d.]+)", text)
    stats = re.findall(r"\[Audio\] steps=.*", text)
    banks = re.findall(r"\[Audio\] game audio on.*", text)
    lines.append("# Audio pass report")
    lines.append("Banks: " + (banks[-1] if banks else "**no '[Audio] game audio on' line**"))
    if not banks:
        failures.append("audio subsystem did not start")
    if "no surface map" in text:
        lines.append("Surface map: **missing** (every ground is stone)")
    lines.append("Counters: " + (stats[-1] if stats else "none"))
    zones = sorted(set(re.findall(r"zone=(\w+)", text)))
    lines.append("Ambience zones heard: " + (", ".join(zones) or "none"))
    if "no zone map" in text:
        lines.append("Zone grid: **missing** (street loop everywhere)")

    walking = [h for _, h, sp, _ in feet if 60 < sp < 700]
    if walking:
        low = float(np.percentile(walking, 5))
        high = float(np.percentile(walking, 95))
        suggestion = round(low + 4.0, 1)
        lines.append(f"Foot height while walking: 5th pct {low:.1f} cm, 95th pct {high:.1f} cm "
                     f"-> suggested FootPlantHeight {suggestion}")
        left = [(t, sp) for side, _, sp, t in feet if side == "L"]
        walk_s = sum(t1 - t0 for (t0, sp), (t1, _) in zip(left, left[1:]) if 60 < sp < 700 and 0 < t1 - t0 < 0.5)
        n_steps = sum(1 for k, _, _ in steps if k == "step")
        rate = n_steps / walk_s if walk_s > 1 else 0.0
        lines.append(f"Steps: {n_steps} in {walk_s:.0f} s of walking = {rate:.2f} per second (expected 1.6 to 3.2)")
        if not 1.2 <= rate <= 3.6:
            failures.append(f"step rate {rate:.2f}/s outside 1.2-3.6: recalibrate FootPlantHeight")
    else:
        lines.append("Foot heights: none logged (no -DebugFeet, or the player never walked)")
        failures.append("no foot height logged")
    surfaces = {}
    for _, surface, _ in steps:
        surfaces[surface] = surfaces.get(surface, 0) + 1
    lines.append("Surfaces stepped on: " + (", ".join(f"{k} {v}" for k, v in sorted(surfaces.items())) or "none"))
    if "test siren on" in text:
        lines.append("Test siren: " + re.findall(r"\[Audio\] test siren on .*", text)[-1])
    if "recording written" in text:
        lines.append("Recording: " + re.findall(r"\[Audio\] recording written .*", text)[-1])
    else:
        failures.append("no audio recording written")

    off, on = os.path.join(folder, "frametimes-audio-off.csv"), os.path.join(folder, "frametimes-audio-on.csv")
    if os.path.isfile(off) and os.path.isfile(on):
        a, b = game_ms(off), game_ms(on)
        d50 = float(np.percentile(b, 50) - np.percentile(a, 50))
        d95 = float(np.percentile(b, 95) - np.percentile(a, 95))
        lines.append(f"Game thread with audio - without: p50 {d50:+.2f} ms, p95 {d95:+.2f} ms (budget {BUDGET_MS} ms, "
                     f"{len(b)} / {len(a)} frames)")
        if d50 > BUDGET_MS:
            failures.append(f"game thread p50 +{d50:.2f} ms over the {BUDGET_MS} ms budget")
    else:
        lines.append("A/B: frame time files missing")

    lines.append("")
    lines.append("Verdict: " + ("PASS" if not failures else "FAIL: " + "; ".join(failures)))
    report = "\n\n".join(lines) + "\n"
    with open(os.path.join(folder, "audio-report.md"), "w") as f:
        f.write(report)
    print(report)
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
