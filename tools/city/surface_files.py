"""Find the image files of the scanned street surfaces (tools/city/surfaces.json) in loose Fab
exports, without Unreal: shared by tools/city/unreal/import_surfaces.py (in the editor) and
tools/city/surfaces_build.py --pack (the 2K files to carry from one machine to another)."""

import os
import re

# Texture role from its name, Megascans first (T_<name>_<id>_2K_D / _N / _ORDp), then generic.
ROLES = (
    ("rough", re.compile(r"(_ORDp?|_ORM|_ARD|_ORD)$", re.I), "G"),
    ("rough", re.compile(r"(_R|rough(ness)?)$", re.I), "R"),
    ("ao", re.compile(r"(_AO|ambient_?occlusion)$", re.I), None),
    ("opacity", re.compile(r"(_O|opacity|alpha)$", re.I), None),     # decals
    ("normal", re.compile(r"(_N|normal(gl|dx)?)$", re.I), None),
    ("basecolor", re.compile(r"(_D|_BC|basecolou?r|albedo|diffuse)$", re.I), None),
)
IMAGE = (".png", ".jpg", ".jpeg", ".tga", ".exr", ".tif", ".tiff")


def role_of(name):
    stem = re.sub(r"_(1|2|4|8)K", "", name, flags=re.I)
    for role, rx, ch in ROLES:
        if rx.search(stem):
            return role, ch
    return None, None


def word_sets(entry):
    """match, then each set of match_any: a folder matches a set when it holds every word."""
    sets = [entry.get("match", [])] + entry.get("match_any", [])
    return [[w.lower() for w in ws] for ws in sets if ws]


def norm(path):
    return re.sub(r"[\s_\-]+", "", path.lower())


def loose_images(entry, loose):
    """Images of one loose export: <loose>/<id>/, else the first folder under loose (any depth)
    whose path matches a word set (Fab exports keep their listing name, e.g. "Roman Limestone
    Wall"). One file per role, the 2K one when there are several resolutions."""
    src = os.path.join(loose, entry["id"])
    roots = [src] if os.path.isdir(src) else []
    if not roots:
        for words in word_sets(entry):
            for d, _, _ in sorted(os.walk(loose)):
                rel = norm(os.path.relpath(d, loose))
                if rel != "." and all(norm(w) in rel for w in words):
                    roots.append(d)
                    break
            if roots:
                break
    if not roots:
        return []
    found = {}
    for d, _, files in sorted(os.walk(roots[0])):
        for fn in sorted(files):
            if not fn.lower().endswith(IMAGE):
                continue
            role, _ = role_of(os.path.splitext(fn)[0])
            if not role or role == "ao":     # AO is not used by the layers
                continue
            path = os.path.join(d, fn)
            if role not in found or ("2k" in fn.lower() and "2k" not in os.path.basename(found[role]).lower()):
                found[role] = path
    return sorted(found.values())
