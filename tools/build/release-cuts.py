#!/usr/bin/env python3
"""Lower the cost of a release build when it misses the M1 frame budget.

Edits GranTheftPraline/Config/DefaultGame.ini in place; a new package is
needed afterwards. Each level includes the ones before it, in the order
agreed for 0.3.0 (cheapest loss for the player first):

  1  frozen daytime (no time-lapse, no weather changes) and one police car
  2  pedestrians: 6 drawn, 8 in the M1 density; traffic: 2 cars

Usage: python3 tools/build/release-cuts.py <level> [--ini PATH]
The changes are printed so they can be copied into the release notes.
"""
import argparse
import pathlib
import re
import sys

LEVELS = {
    1: [
        ("/Script/GranTheftPraline.GTPEnvironmentSubsystem", "StartHour", "13.5"),
        ("/Script/GranTheftPraline.GTPEnvironmentSubsystem", "DefaultTimeScale", "0.0"),
        ("/Script/GranTheftPraline.GTPEnvironmentSubsystem", "bAutoWeather", "False"),
        ("/Script/GranTheftPraline.GTPPoliceDirector", "MaxUnits", "1"),
    ],
    2: [
        ("/Script/GranTheftPraline.GTPPedestrianSubsystem", "MaxVisible", "6"),
        ("/Script/GranTheftPraline.GTPPedestrianSubsystem", "DensityM1", "8"),
        ("/Script/GranTheftPraline.GTPTrafficNetwork", "NumCars", "2"),
    ],
}


def set_key(lines, section, key, value):
    """Sets key=value in [section], adding the key or the section if missing."""
    header = f"[{section}]"
    try:
        start = next(i for i, l in enumerate(lines) if l.strip() == header)
    except StopIteration:
        if lines and lines[-1].strip():
            lines.append("")
        lines += [header, f"{key}={value}"]
        return "added section"
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("[")), len(lines))
    pattern = re.compile(rf"^{re.escape(key)}=")
    for i in range(start + 1, end):
        if pattern.match(lines[i]):
            old = lines[i].split("=", 1)[1]
            lines[i] = f"{key}={value}"
            return f"was {old}"
    insert = end
    while insert > start + 1 and not lines[insert - 1].strip():
        insert -= 1
    lines.insert(insert, f"{key}={value}")
    return "added"


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("level", type=int, choices=sorted(LEVELS))
    root = pathlib.Path(__file__).resolve().parents[2]
    parser.add_argument("--ini", type=pathlib.Path, default=root / "GranTheftPraline/Config/DefaultGame.ini")
    args = parser.parse_args()
    lines = args.ini.read_text(encoding="utf-8").splitlines()
    for level in range(1, args.level + 1):
        for section, key, value in LEVELS[level]:
            note = set_key(lines, section, key, value)
            print(f"level {level}: [{section}] {key}={value} ({note})")
    args.ini.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
