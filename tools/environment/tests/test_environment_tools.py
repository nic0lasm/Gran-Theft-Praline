"""Run: python3 -m pytest tools/environment/tests -q"""

import csv
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import build_night_lights  # noqa: E402
import geo  # noqa: E402
import textures  # noqa: E402

PRESQU_ILE_ORIGIN = (842498, 6519809)


def test_lambert93_origin_of_the_projection():
    x, y = geo.lambert93(46.5, 3.0)
    assert abs(x - 700000) < 1e-3 and abs(y - 6600000) < 1e-3


def test_lambert93_known_point():
    # Paris, Notre-Dame square, value of pyproj (EPSG:4326 -> EPSG:2154)
    x, y = geo.lambert93(48.8534, 2.3488)
    assert abs(x - 652216.63) < 0.05 and abs(y - 6861681.50) < 0.05


def test_city_hall_is_at_the_north_end_of_the_street():
    # rue de la Republique axis ends near (18500, -50000) cm in the generated city
    x, y, _ = geo.to_unreal(45.7676, 4.8357, PRESQU_ILE_ORIGIN)
    assert abs(x - 18500) < 6000 and abs(y + 50000) < 6000


def test_textures_are_valid_pngs(tmp_path):
    w, h, px = textures.rain_streaks(width=32, height=64, count=20)
    assert len(px) == w * h and max(px) > 200
    path = tmp_path / "streaks.png"
    textures.write_png(str(path), w, h, px)
    assert path.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
    w, h, px = textures.puddle_noise(size=32)
    assert min(px) == 0 and max(px) == 255


def _write(path, header, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def test_night_lights_from_city_outputs(tmp_path):
    gen = tmp_path / "rue-de-la-republique"
    gen.mkdir()
    _write(gen / "props_placement.csv", ["catalog_id", "ue_x", "ue_y", "ue_z", "ue_yaw"],
           [["W1-04", "100", "200", "10", "0"], ["W1-07", "0", "0", "0", "0"]])
    _write(gen / "facade_modules.csv", ["kind", "side", "ue_x", "ue_y", "ue_z", "ue_yaw", "width_m"],
           [["shop", "street", "0", "0", "0", "90", "3"], ["window", "street", "0", "0", "0", "0", "1"]])
    out = tmp_path / "NightLights.csv"
    assert build_night_lights.main(["--out", str(out), str(gen)]) == 0
    rows = list(csv.DictReader(open(out)))
    assert [r["kind"] for r in rows] == ["street", "shop"]
    street, shop = rows
    assert street["map"] == "L_City_RueDeLaRepublique" and float(street["z"]) == 420.0 and street["shadow"] == "1"
    # shop light 60 cm out of a facade facing +Y (yaw 90), 80 % of the front wide
    assert abs(float(shop["y"]) - 60.0) < 0.01 and abs(float(shop["x"])) < 0.01 and float(shop["width_cm"]) == 240.0
