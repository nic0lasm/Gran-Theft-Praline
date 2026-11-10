"""Builds the walking logic simulator and runs it on a walkways.csv.

  python3 tools/pedestrians/check_walk_logic.py [walkways.csv]

Without an argument it makes walkways from a small street grid (a pedestrian
street crossing a car street and a side street), so it runs anywhere with a C++
compiler and the city tools' Python requirements. With the CSV written by
tools/city/pedestrians.py it checks the real street. Exit code 0 on PASS.
"""

import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "city"))
import pedestrians  # noqa: E402


def road(cid, coords, vitesse=30, voies=2, largeur=6.0, nature="Route à 1 chaussée"):
    return {"properties": {"cleabs": cid, "nature": nature, "nombre_de_voies": voies,
                           "largeur_de_chaussee": largeur, "position_par_rapport_au_sol": "0",
                           "vitesse_moyenne_vl": vitesse},
            "geometry": {"type": "MultiLineString", "coordinates": [coords]}}


def sample_csv(path):
    edges = pedestrians.build_walkways([
        road("P1", [[0, -300], [0, 0]], vitesse=0), road("P2", [[0, 0], [0, 300]], vitesse=0),
        road("C", [[-200, 0], [0, 0]]), road("D", [[0, 0], [200, 0]]),
        road("E", [[0, 300], [150, 300]], voies=1, largeur=4.0),
    ])
    pedestrians.write(edges, os.path.dirname(path), [0, 0, 0, 0])
    return os.path.join(os.path.dirname(path), "walkways.csv")


def main():
    with tempfile.TemporaryDirectory() as tmp:
        exe = os.path.join(tmp, "walk_sim")
        subprocess.run(["c++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-o", exe,
                        os.path.join(HERE, "walk_sim.cpp")], check=True)
        csv_path = sys.argv[1] if len(sys.argv) > 1 else sample_csv(os.path.join(tmp, "x.csv"))
        sys.exit(subprocess.run([exe, csv_path]).returncode)


if __name__ == "__main__":
    main()
