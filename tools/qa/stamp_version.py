#!/usr/bin/env python3
"""Set ProjectVersion in DefaultGame.ini from a release tag.

The tag is the single source of truth for the version number: package scripts
call this before building, so the packaged app, the in-game version label and
the archive names can never disagree with the tag again (0.3.0 shipped with
ProjectVersion=0.2.0).

Accepted tags: v0.9.0, 0.9.0, v0.9 (read as 0.9.0), v0.9.0-rc1, v0.9.0-rc.2.
The ini receives the numeric part only (X.Y.Z); the release-candidate suffix
is printed with --label for archive names.

Usage:
  stamp_version.py [TAG] [--ini PATH] [--check] [--label]
    TAG       default: the latest tag reachable from HEAD (git describe)
    --check   do not write; exit 1 if the ini differs from TAG
    --label   print the full label (e.g. 0.9.0-rc1) instead of X.Y.Z
"""
import argparse
import pathlib
import re
import subprocess
import sys

TAG_RE = re.compile(r"^v?(\d+)\.(\d+)(?:\.(\d+))?(?:-(rc[.]?\d+))?$")
SECTION = "[/Script/EngineSettings.GeneralProjectSettings]"


def parse_tag(tag):
    """Return (numeric 'X.Y.Z', suffix or '') or raise ValueError."""
    match = TAG_RE.match(tag.strip())
    if not match:
        raise ValueError(f"not a release tag: {tag!r} (expected vX.Y.Z or vX.Y.Z-rcN)")
    major, minor, patch, suffix = match.groups()
    return f"{major}.{minor}.{patch or '0'}", (suffix or "").replace(".", "")


def read_version(text):
    in_section = False
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("["):
            in_section = stripped == SECTION
        elif in_section and stripped.startswith("ProjectVersion="):
            return stripped.split("=", 1)[1].strip()
    return None


def set_version(text, version):
    """Return text with ProjectVersion=version; the section and key must exist."""
    lines = text.splitlines(keepends=True)
    in_section = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("["):
            in_section = stripped == SECTION
        elif in_section and stripped.startswith("ProjectVersion="):
            ending = "\r\n" if line.endswith("\r\n") else "\n"
            lines[i] = f"ProjectVersion={version}{ending}"
            return "".join(lines)
    raise ValueError(f"no ProjectVersion key in {SECTION}")


def latest_tag():
    out = subprocess.run(
        ["git", "describe", "--tags", "--abbrev=0"], capture_output=True, text=True
    )
    if out.returncode != 0:
        raise ValueError("no tag given and git describe found none")
    return out.stdout.strip()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("tag", nargs="?")
    parser.add_argument("--ini", default=None)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--label", action="store_true")
    args = parser.parse_args(argv)

    repo = pathlib.Path(__file__).resolve().parents[2]
    ini = pathlib.Path(args.ini) if args.ini else repo / "GranTheftPraline/Config/DefaultGame.ini"
    try:
        version, suffix = parse_tag(args.tag or latest_tag())
        text = ini.read_text(encoding="utf-8")
        current = read_version(text)
        if args.check:
            if current != version:
                print(f"ProjectVersion is {current}, tag says {version}", file=sys.stderr)
                return 1
        elif current != version:
            ini.write_text(set_version(text, version), encoding="utf-8", newline="")
            print(f"ProjectVersion {current} -> {version}", file=sys.stderr)
    except (ValueError, OSError) as err:
        print(f"stamp_version: {err}", file=sys.stderr)
        return 2
    print(f"{version}-{suffix}" if args.label and suffix else version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
