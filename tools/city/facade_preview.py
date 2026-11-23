"""Street-level picture of the backdrop facade shader, without Unreal.

M_City_Backdrop draws its facades with one block of HLSL (apply_city_materials.BACKDROP_HLSL),
which only the editor of the reference PC compiles. This script takes that same text, compiles
it as C++ with a small shim (tools/city/facade_preview/hlsl_shim.h) and renders a street seen
from the pavement: a typo fails here, and the look of a change can be judged before a gate.

  python tools/city/facade_preview.py --out facade.ppm [--size 1280 720] [--check]

--check holds the picture to a few figures (walls drawn, a plausible share of glass, contrast),
for CI. The picture is the shader's colour under a plain sun and sky: no texture, no engine
lighting, no weathering.
"""

import argparse
import ast
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "unreal", "apply_city_materials.py")
HARNESS = os.path.join(HERE, "facade_preview")
GLASS_SHARE = (0.08, 0.40)     # of the wall pixels: windows and shopfronts of a Lyon street
MIN_DEVIATION = 0.08           # of linear luminance: a facade, not a flat colour


def shader_text(source=SOURCE):
    """BACKDROP_HLSL read from the module's source: the module itself imports unreal."""
    with open(source, encoding="utf-8") as f:
        tree = ast.parse(f.read())
    for node in tree.body:
        if isinstance(node, ast.Assign) and getattr(node.targets[0], "id", "") == "BACKDROP_HLSL":
            return node.value.value
    raise SystemExit("BACKDROP_HLSL not found in " + source)


def render(out, size, source=SOURCE):
    """Compiles the shader into the harness, renders `out` (PPM) and returns the figures."""
    with tempfile.TemporaryDirectory() as tmp:
        with open(os.path.join(tmp, "facade_body.inc"), "w", encoding="utf-8") as f:
            f.write(shader_text(source))
        exe = os.path.join(tmp, "facade_preview")
        subprocess.run([os.environ.get("CXX", "g++"), "-O2", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-I", tmp, "-I", HARNESS, os.path.join(HARNESS, "facade_preview.cpp"), "-o", exe],
                       check=True)
        line = subprocess.run([exe, out, str(size[0]), str(size[1])], check=True, capture_output=True,
                              text=True).stdout.split()
    return {k: float(v) for k, v in zip(line[0::2], line[1::2])}


def problems(figures):
    out = []
    if figures["wall_pixels"] <= 0:
        out.append("no wall in the picture")
    if not GLASS_SHARE[0] <= figures["glass_share"] <= GLASS_SHARE[1]:
        out.append("glass share %.3f outside %s" % (figures["glass_share"], GLASS_SHARE))
    if figures["deviation"] < MIN_DEVIATION:
        out.append("contrast %.3f under %.2f: the facade pattern is gone" % (figures["deviation"], MIN_DEVIATION))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default="facade.ppm")
    ap.add_argument("--size", nargs=2, type=int, default=(1280, 720), metavar=("W", "H"))
    ap.add_argument("--source", default=SOURCE, help="another version of apply_city_materials.py, for a before/after")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    figures = render(args.out, args.size, args.source)
    print("facade preview: %s | %s" % (args.out, " ".join("%s %g" % kv for kv in sorted(figures.items()))))
    if args.check:
        bad = problems(figures)
        for b in bad:
            print("facade preview: " + b, file=sys.stderr)
        return 1 if bad else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
