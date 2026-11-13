"""Local patch of City Sample's Traffic plugin (Epic code, copied into Plugins/Traffic, out of Git).
Plain Python, idempotent, editor closed. No Epic code here: only the lines we add.

    python tools/vehicles/patch_citysample_traffic.py <project>/GranTheftPraline

MassTrafficSubsystem.cpp: MSVC's optimiser reports C4723 ("potential divide by 0") on a division
guarded by NumLanes > 0 (line 870 of the 5.8 copy). The game target treats the warning as an error
and the Development package fails. The script puts, at the top of that file only:
    #ifdef _MSC_VER / #pragma warning(disable: 4723) / #endif
under a marker line.

- First run: keeps the original as MassTrafficSubsystem.cpp.orig, inserts the block.
- Run again: nothing to do ("already applied").
- The file no longer matches the saved original (plugin copied again, or updated): says so, keeps
  the old .orig as .orig.<md5>, saves the new original and patches it.
Report (md5 before and after): <project>/Saved/CitySamplePatch/traffic_patch_report.json.
Exit code 1 when the file is missing.
"""

import hashlib
import json
import os
import shutil
import sys

RELATIVE = os.path.join("Plugins", "Traffic", "Source", "MassTraffic", "Private", "MassTrafficSubsystem.cpp")
MARK = "// GTP local patch (tools/vehicles/patch_citysample_traffic.py): MSVC C4723 false positive"
BLOCK = MARK + "\n#ifdef _MSC_VER\n#pragma warning(disable: 4723)\n#endif\n"


def md5_bytes(data):
    return hashlib.md5(data).hexdigest()


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    project = sys.argv[1]
    path = os.path.join(project, RELATIVE)
    orig = path + ".orig"
    report = {"file": RELATIVE.replace(os.sep, "/")}
    if not os.path.exists(path):
        report["result"] = "missing: copy City Sample's Plugins/Traffic first"
        print(json.dumps(report, indent=2))
        sys.exit(1)
    with open(path, "rb") as f:
        current = f.read()
    report["md5_before"] = md5_bytes(current)
    # A UTF-8 byte order mark stays first; the block goes right after it.
    bom = b"\xef\xbb\xbf" if current.startswith(b"\xef\xbb\xbf") else b""
    body = current[len(bom):]
    if body.startswith(MARK.encode("utf-8")):
        unpatched = bom + body[len(BLOCK.encode("utf-8")):]
        if os.path.exists(orig):
            with open(orig, "rb") as f:
                if md5_bytes(f.read()) != md5_bytes(unpatched):
                    report["warning"] = "patched file differs from the saved original below the patch: edited by hand?"
        report["result"] = "already applied"
    else:
        if os.path.exists(orig):
            with open(orig, "rb") as f:
                saved = f.read()
            if md5_bytes(saved) != md5_bytes(current):
                kept = orig + "." + md5_bytes(saved)[:8]
                shutil.move(orig, kept)
                report["warning"] = "file changed since the last patch (plugin copied again or updated); old original kept as " + os.path.basename(kept)
        if not os.path.exists(orig):
            shutil.copy2(path, orig)
        with open(path, "wb") as f:
            f.write(bom + BLOCK.encode("utf-8") + body)
        report["result"] = "applied"
    with open(path, "rb") as f:
        report["md5_after"] = md5_bytes(f.read())
    out = os.path.join(project, "Saved", "CitySamplePatch")
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "traffic_patch_report.json"), "w") as f:
        json.dump(report, f, indent=2)
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
