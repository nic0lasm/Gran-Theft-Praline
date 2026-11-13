"""Read-only survey of the City Sample crowd characters, to pick the player's face (Nicolito) among them.

Plain Python, no editor, nothing written next to the sample: it walks the sample's Content folder and reports
1. the crowd folders and their weight on disk (what copying them would cost);
2. the head, body, outfit and hair assets by name, with sizes (heads first: the face candidates);
3. which packages reference C++ classes of the sample itself (/Script/CitySample...): those would not load in
   our project without that code, so the character must be rebuilt from its meshes and materials.

    python -I tools/character/citysample_crowd_survey.py <CitySample>/Content [report.txt]

The report goes to stdout and, when given, to report.txt (put it under the project's Saved folder).
"""
import os
import re
import sys
from collections import defaultdict

KINDS = [
    ("head", re.compile(r"(^|_)(head|face)(_|$)", re.I)),
    ("body", re.compile(r"(^|_)body(_|$)", re.I)),
    ("hair", re.compile(r"(^|_)(hair|groom|beard|mustache|eyebrows)(_|$)", re.I)),
    ("outfit", re.compile(r"(^|_)(top|bottom|shoes?|outfit|jacket|shirt|pants|coat|hoodie)(_|$)", re.I)),
]
CROWD_WORDS = ("crowd", "character", "metahuman")
SAMPLE_SCRIPT = re.compile(rb"/Script/CitySample[A-Za-z]*")
EXTENSIONS = (".uasset", ".uexp", ".ubulk", ".uptnl")


def human(size):
    for unit in ("B", "KB", "MB", "GB"):
        if size < 1024 or unit == "GB":
            return "%.1f %s" % (size, unit)
        size /= 1024.0
    return "%d B" % size


def main():
    if len(sys.argv) < 2 or not os.path.isdir(sys.argv[1]):
        sys.exit("usage: citysample_crowd_survey.py <CitySample>/Content [report.txt]")
    content = os.path.abspath(sys.argv[1])
    out = []

    def say(text):
        out.append(text)
        print(text)

    folder_size = defaultdict(int)
    packages = defaultdict(int)  # package path without extension -> bytes
    for root, _dirs, files in os.walk(content):
        rel = os.path.relpath(root, content).replace(os.sep, "/")
        if not any(word in rel.lower() for word in CROWD_WORDS):
            continue
        top = "/".join(rel.split("/")[:2])
        for name in files:
            if not name.endswith(EXTENSIONS):
                continue
            path = os.path.join(root, name)
            size = os.path.getsize(path)
            folder_size[top] += size
            packages[(rel, os.path.splitext(name)[0])] += size

    say("== 1. Crowd folders (Content/...), size on disk")
    for top, size in sorted(folder_size.items(), key=lambda item: -item[1]):
        say("%10s  %s" % (human(size), top))
    say("total %s in %d packages" % (human(sum(folder_size.values())), len(packages)))

    say("== 2. Assets by kind (name, folder, size)")
    by_kind = defaultdict(list)
    for (rel, name), size in packages.items():
        for kind, pattern in KINDS:
            if pattern.search(name):
                by_kind[kind].append((rel, name, size))
                break
    for kind, _pattern in KINDS:
        items = sorted(by_kind.get(kind, []))
        say("-- %s: %d" % (kind, len(items)))
        for rel, name, size in items[:400]:
            say("%10s  %s/%s" % (human(size), rel, name))

    say("== 3. Packages that need the sample's own C++ classes")
    needing = defaultdict(set)
    for (rel, name), _size in packages.items():
        path = os.path.join(content, rel.replace("/", os.sep), name + ".uasset")
        if not os.path.exists(path):
            continue
        with open(path, "rb") as handle:
            data = handle.read()
        for match in set(SAMPLE_SCRIPT.findall(data)):
            needing[match.decode("ascii", "replace")].add("%s/%s" % (rel, name))
    if not needing:
        say("none: the crowd assets use engine classes only")
    for module, users in sorted(needing.items()):
        say("-- %s: %d packages, e.g. %s" % (module, len(users), ", ".join(sorted(users)[:8])))

    if len(sys.argv) > 2:
        with open(sys.argv[2], "w", encoding="utf-8") as handle:
            handle.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
