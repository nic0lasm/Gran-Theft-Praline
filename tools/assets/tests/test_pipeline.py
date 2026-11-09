import json
import os
import sys

import numpy as np
import pytest
import trimesh
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)

from gtp_assets.geometry import decimate, load_glb, normalize  # noqa: E402
from gtp_assets.manifest import parse_catalog  # noqa: E402
from gtp_assets.prepare import prepare_asset  # noqa: E402
from make_sample import make_sample_glb  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))


@pytest.fixture(scope="module")
def sample(tmp_path_factory):
    return make_sample_glb(str(tmp_path_factory.mktemp("src") / "W1-01.glb"))


def test_normalize_scale_pivot(sample):
    a = load_glb(sample)
    normalize(a, 1.8, "bottom_center")
    lo, hi = a.bounds()
    assert np.max(hi - lo) == pytest.approx(1.8, abs=1e-4)
    assert lo[1] == pytest.approx(0.0, abs=1e-4)
    assert (lo[0] + hi[0]) / 2 == pytest.approx(0.0, abs=1e-4)
    assert (lo[2] + hi[2]) / 2 == pytest.approx(0.0, abs=1e-4)


def test_back_center_pivot(sample):
    a = load_glb(sample)
    normalize(a, 1.0, "back_center")
    lo, hi = a.bounds()
    assert lo[2] == pytest.approx(0.0, abs=1e-4)
    assert (lo[1] + hi[1]) / 2 == pytest.approx(0.0, abs=1e-4)


def test_decimation_hits_budget_and_keeps_shape(sample):
    a = load_glb(sample)
    normalize(a, 1.8)
    before = a.tri_count
    d = decimate(a, 3000)
    assert before > 3000 * 4
    assert 1500 <= d.tri_count <= 4500
    lo0, hi0 = a.bounds()
    lo1, hi1 = d.bounds()
    assert np.allclose(lo0, lo1, atol=0.03) and np.allclose(hi0, hi1, atol=0.03)
    for p in d.parts:
        assert p.faces.max() < len(p.vertices)
        assert np.isfinite(p.uv).all() and p.uv.min() >= -1e-4 and p.uv.max() <= 1 + 1e-4


def test_prepare_outputs(sample, tmp_path):
    item = {"id": "W1-01", "size_m": 1.8, "family": "StreetFurniture",
            "asset": "Bench_WoodCastIron", "ultra_tris": 6000}
    meta = prepare_asset(sample, item, str(tmp_path))
    out = tmp_path / "W1-01"
    for f in ("asset.json", "W1-01_ultra.glb"):
        assert (out / f).exists()
    assert not (out / "W1-01_m1.glb").exists()
    t = meta["triangles"]
    assert t["master"] > 6000 and 3000 <= t["ultra"] <= 7500  # capped to the Nanite budget
    assert t["m1_lod0"] == 10000  # medium prop budget (art bible 5.1)
    assert meta["category"] == "medium" and len(meta["lod_plan"]) == 4
    pct = [e["percent_triangles"] for e in meta["lod_plan"]]
    assert pct[0] == 1.0 and pct[1] > pct[2] > pct[3]
    assert [e["screen_size"] for e in meta["lod_plan"]] == [1.0, 0.5, 0.25, 0.1]
    assert meta["collision"]["type"] == "convex" and meta["collision"]["hulls"] >= 1
    tex = meta["materials"][0]["textures"]
    assert tex == {"base_color": "T_Bench_WoodCastIron_BC.png",
                   "orm": "T_Bench_WoodCastIron_ORM.png",
                   "normal": "T_Bench_WoodCastIron_N.png"}
    assert meta["materials"][0]["instance_name"] == "MI_Bench_WoodCastIron"
    for key in tex:
        assert (out / "textures" / tex[key]).exists()
    orm = np.asarray(Image.open(out / "textures" / tex["orm"]))
    assert abs(int(orm[..., 1].mean()) - 180) <= 1 and abs(int(orm[..., 2].mean()) - 60) <= 1
    scene = trimesh.load(str(out / "W1-01_ultra.glb"), force="scene", process=False)
    assert max(scene.extents) == pytest.approx(1.8, abs=1e-3)
    json.load(open(out / "asset.json"))


def test_small_prop_budget_and_box_collision(tmp_path):
    m = trimesh.creation.icosphere(subdivisions=5)  # 20k tris
    path = str(tmp_path / "W1-09.glb")
    trimesh.Scene(m).export(path)
    meta = prepare_asset(path, {"id": "W1-09", "size_m": 0.5, "ultra_tris": 6000},
                         str(tmp_path / "o"))
    assert meta["category"] == "small" and len(meta["lod_plan"]) == 3
    assert meta["triangles"]["m1_lod0"] == 3000
    assert meta["collision"] == {"type": "box", "hulls": 1}


def test_incremental_skip_and_force(sample, tmp_path):
    item = {"id": "W1-01", "size_m": 1.8}
    assert prepare_asset(sample, item, str(tmp_path))["skipped"] is False
    assert prepare_asset(sample, item, str(tmp_path))["skipped"] is True
    assert prepare_asset(sample, {**item, "size_m": 2.0}, str(tmp_path))["skipped"] is False
    assert prepare_asset(sample, item, str(tmp_path), force=True)["skipped"] is False


def test_flat_object_has_no_collision(tmp_path):
    flat = trimesh.creation.box(extents=[0.7, 0.01, 0.7])
    path = str(tmp_path / "W1-28.glb")
    trimesh.Scene(flat).export(path)
    meta = prepare_asset(path, {"id": "W1-28", "size_m": 0.7}, str(tmp_path / "out"))
    assert meta["collision"]["type"] == "none"
    # untextured source still yields all three maps
    t = meta["materials"][0]["textures"]
    assert set(t) == {"base_color", "orm", "normal"}


def test_catalog_manifest_covers_wave_1():
    with open(os.path.join(ROOT, "production", "asset-catalog.md"), encoding="utf-8") as f:
        items = parse_catalog(f.read(), "W1")
    ids = [i["id"] for i in items]
    assert ids == [f"W1-{n:02d}" for n in range(1, 61)]
    by_id = {i["id"]: i for i in items}
    assert by_id["W1-04"]["size_m"] == 4.5
    assert by_id["W1-32"]["size_m"] == 1.1  # longest of 0.6 x 0.7 x 1.1
    assert by_id["W1-28"]["collision"] == "none"
    assert by_id["W1-01"]["family"] == "StreetFurniture" and by_id["W1-01"]["asset"] == "Bench_WoodCastIron"
    assert all("asset" in i and i["category"] in ("small", "medium", "large") for i in items)
    names = [(i["family"], i["asset"]) for i in items]
    assert len(set(names)) == 60, "asset names must be unique"
    assert by_id["W1-06"]["pivot"] == "back_center"
    committed = json.load(open(os.path.join(ROOT, "assets", "manifests", "wave-01.json")))
    assert committed["items"] == items, "regenerate assets/manifests/wave-01.json"
