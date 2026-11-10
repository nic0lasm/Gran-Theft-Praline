"""Builds the impact logic checker and runs it (no Unreal needed).

  python3 tools/ragdoll/check_impact_logic.py

Exit code 0 on PASS.
"""

import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "impact_sim")
        subprocess.run(["c++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-o", exe,
                        os.path.join(HERE, "impact_sim.cpp")], check=True)
        sys.exit(subprocess.run([exe]).returncode)


if __name__ == "__main__":
    main()
