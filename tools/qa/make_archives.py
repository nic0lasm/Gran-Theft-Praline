#!/usr/bin/env python3
"""Pack a packaged build into zip archives of at most 2 GiB each, with checksums.

Every archive is a plain zip that extracts into the same top folder, so a
build too big for one file becomes GranTheftPraline-0.9.0-rc1-macos-part1of2.zip
and -part2of2.zip: extract both into the same place. Symbolic links and the
executable bit are kept (needed for a macOS .app). SHA256SUMS lists every
archive; verify with `shasum -a 256 -c SHA256SUMS` (macOS) or
`Get-FileHash` (Windows, see docs/qa/test-build-instructions.md).

Usage: make_archives.py SOURCE_DIR_OR_APP OUT_DIR --name NAME [--limit-bytes N]
  NAME   e.g. GranTheftPraline-0.9.0-rc1-macos
"""
import argparse
import hashlib
import os
import pathlib
import stat
import sys
import time
import zipfile

LIMIT = 2 * 1024**3 - 64 * 1024**2  # 2 GiB minus a safety margin for zip overhead


def collect(source):
    """Return [(path, arcname, size)] for files and symlinks under source's folder."""
    source = pathlib.Path(source)
    top = source.name
    entries = []
    for root, dirs, files in os.walk(source):
        dirs.sort()
        for name in sorted(files) + [d for d in dirs if os.path.islink(os.path.join(root, d))]:
            path = pathlib.Path(root) / name
            arc = f"{top}/{path.relative_to(source).as_posix()}"
            size = len(os.readlink(path)) if path.is_symlink() else path.stat().st_size
            entries.append((path, arc, size))
    return entries


def plan(entries, limit):
    """Greedy split into groups whose raw size stays under limit (zip never grows much)."""
    groups, current, used = [], [], 0
    for entry in entries:
        size = entry[2]
        if size > limit:
            raise ValueError(f"{entry[1]} is {size} bytes, above the archive limit {limit}")
        if current and used + size > limit:
            groups.append(current)
            current, used = [], 0
        current.append(entry)
        used += size
    if current:
        groups.append(current)
    return groups


def write_zip(target, group):
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=1, allowZip64=True) as zf:
        for path, arc, _ in group:
            st = os.lstat(path)
            info = zipfile.ZipInfo(arc, time.localtime(st.st_mtime)[:6])
            info.create_system = 3  # Unix: keeps modes and symlinks
            info.external_attr = (st.st_mode & 0xFFFF) << 16
            if stat.S_ISLNK(st.st_mode):
                zf.writestr(info, os.readlink(path))
            else:
                info.compress_type = zipfile.ZIP_DEFLATED
                with open(path, "rb") as src, zf.open(info, "w", force_zip64=True) as dst:
                    while chunk := src.read(1 << 20):
                        dst.write(chunk)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(1 << 20):
            digest.update(chunk)
    return digest.hexdigest()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("source")
    parser.add_argument("out_dir")
    parser.add_argument("--name", required=True)
    parser.add_argument("--limit-bytes", type=int, default=LIMIT)
    args = parser.parse_args(argv)

    source = pathlib.Path(args.source)
    if not source.is_dir():
        print(f"make_archives: not a folder: {source}", file=sys.stderr)
        return 2
    out = pathlib.Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    try:
        groups = plan(collect(source), args.limit_bytes)
    except ValueError as err:
        print(f"make_archives: {err}", file=sys.stderr)
        return 2
    count = len(groups)
    names = []
    for i, group in enumerate(groups, 1):
        name = f"{args.name}.zip" if count == 1 else f"{args.name}-part{i}of{count}.zip"
        write_zip(out / name, group)
        if (out / name).stat().st_size > 2 * 1024**3:
            print(f"make_archives: {name} is above 2 GiB, lower --limit-bytes", file=sys.stderr)
            return 1
        names.append(name)
        print(f"wrote {name}", file=sys.stderr)
    lines = [f"{sha256(out / n)}  {n}\n" for n in names]
    (out / "SHA256SUMS").write_text("".join(lines), encoding="utf-8")
    print("".join(lines), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
