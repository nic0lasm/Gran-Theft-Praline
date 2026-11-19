"""Reads, without the editor, what Unreal packages hold for a few properties: City Sample's vehicle
Blueprints and physics assets cannot be loaded in our project (their parent classes live in City
Sample's game module), yet their centre of mass settings are what our cars should take.

For each .uasset under FOLDER whose name matches PATTERN, the script finds the wanted property names
in the package's name table, every place the package refers to them, and from there:
  - the bytes that follow (hex), for a reader;
  - every run of three doubles that reads as a plausible vector (each 0 or between 1e-3 and 1e5 in
    size): an FVector property's value (UE 5 vectors are doubles);
  - for a bool property, that it is stored at all (Blueprints and assets only store values that
    differ from the class default).

Read only: it opens the files and prints. No Unreal code, no Epic data copied into the repository.

    python -I tools/vehicles/read_uasset_props.py FOLDER [PATTERN] > report.txt
"""

import os
import re
import struct
import sys

WANTED = (
    "bEnableCenterOfMassOverride", "CenterOfMassOverride", "COMNudge", "InertiaTensorScale",
    "bOverrideMass", "MassInKgOverride", "ChassisHeight", "ChassisWidth", "DragCoefficient",
)
DEFAULT_PATTERN = r"(?i)(veh|vehicle|car|police|phys)"


def name_entry(data, offset):
    """(name, next offset) of an ANSI name table entry at offset, or None."""
    if offset + 4 > len(data):
        return None
    (length,) = struct.unpack_from("<i", data, offset)
    if length < 2 or length > 1024 or offset + 4 + length + 4 > len(data):
        return None
    raw = data[offset + 4:offset + 4 + length]
    if raw[-1] != 0 or 0 in raw[:-1]:
        return None
    try:
        text = raw[:-1].decode("ascii")
    except UnicodeDecodeError:
        return None
    if not all(32 <= ord(c) < 127 for c in text):
        return None
    return text, offset + 4 + length + 4  # + the two 16-bit hashes


def name_table(data):
    """The name table, right after the package summary: the first run of 20 name entries or more
    (else the longest run in the first megabyte), as {name: index}."""
    best = {}
    offset = 0
    limit = min(len(data), 1 << 20)
    while offset < limit:
        names = []
        at = offset
        while True:
            entry = name_entry(data, at)
            if entry is None:
                break
            names.append(entry[0])
            at = entry[1]
        if len(names) > len(best):
            best = {name: index for index, name in enumerate(names)}
            if len(names) >= 20:
                return best
        offset = at + 1 if len(names) > 8 else offset + 1
    return best


def plausible(value):
    size = abs(value)
    return value == value and (size == 0.0 or 1e-3 <= size <= 1e5)


def vectors_after(data, start, span=160):
    """Offsets (from start) and values of runs of three plausible doubles, not all zero."""
    found = []
    for offset in range(start, min(start + span, len(data) - 24)):
        x, y, z = struct.unpack_from("<ddd", data, offset)
        if all(plausible(v) for v in (x, y, z)) and (x, y, z) != (0.0, 0.0, 0.0):
            found.append((offset - start, (round(x, 3), round(y, 3), round(z, 3))))
    return found


def read(path):
    with open(path, "rb") as handle:
        data = handle.read()
    names = name_table(data)
    lines = []
    for wanted in WANTED:
        index = names.get(wanted)
        if index is None:
            continue
        reference = struct.pack("<ii", index, 0)
        places = [m.start() for m in re.finditer(re.escape(reference), data)]
        lines.append("  %s: name %d, %d reference(s)" % (wanted, index, len(places)))
        for place in places[:6]:
            after = data[place + 8:place + 8 + 64]
            lines.append("    at %d: %s" % (place, after.hex(" ")))
            for offset, vector in vectors_after(data, place + 8)[:4]:
                lines.append("      vector +%d: %s" % (offset, vector))
    return len(names), lines


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    folder = argv[1]
    pattern = re.compile(argv[2] if len(argv) > 2 else DEFAULT_PATTERN)
    for root, _dirs, files in os.walk(folder):
        for name in sorted(files):
            if not name.endswith(".uasset") or not pattern.search(name):
                continue
            path = os.path.join(root, name)
            count, lines = read(path)
            if lines:
                print("%s (%d names)" % (os.path.relpath(path, folder), count))
                print("\n".join(lines))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
