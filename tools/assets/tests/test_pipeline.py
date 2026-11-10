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
    assert 1500 <= d.tri_count <= 3000  # a hard cap
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
    assert pct == [1.0, 0.5, 0.25, 0.125]  # fractions of the M1 LOD0 / Nanite fallback
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


def test_calibration_helpers():
    from gtp_assets import calibrate
    # normals: green flipped to DirectX
    n = Image.fromarray(np.full((4, 4, 3), [128, 200, 255], dtype=np.uint8), "RGB")
    assert np.asarray(calibrate.flip_normal_green(n))[0, 0].tolist() == [128, 55, 255]
    # albedo: near-black and near-white non-metal pixels pulled into 30-240
    rng = np.random.default_rng(1)
    base = Image.fromarray(rng.integers(0, 256, (64, 64, 3), dtype=np.uint8), "RGB")
    orm = Image.fromarray(np.zeros((64, 64, 3), dtype=np.uint8), "RGB")
    out, info = calibrate.calibrate_albedo(base, orm)
    assert info["albedo_corrected"]
    lum = np.asarray(out).astype(float) @ [0.2126, 0.7152, 0.0722]
    assert np.percentile(lum, 0.5) >= 28 and np.percentile(lum, 99.5) <= 242
    # a flat roughness map gets variation, a varied one is left alone
    flat = Image.fromarray(np.full((128, 128, 3), 180, dtype=np.uint8), "RGB")
    varied, did = calibrate.vary_roughness(flat)
    assert did and np.asarray(varied)[..., 1].std() > 2
    assert calibrate.vary_roughness(varied)[1] is False


def test_density_drives_texture_size(tmp_path):
    from gtp_assets.calibrate import target_side
    m = trimesh.creation.box(extents=[1.0, 1.0, 1.0])  # 6 m2 = 60000 cm2
    path = str(tmp_path / "W1-99.glb")
    big = Image.fromarray(np.full((4096, 4096, 3), 120, dtype=np.uint8), "RGB")
    from trimesh.visual import TextureVisuals
    from trimesh.visual.material import PBRMaterial
    uv = np.c_[(m.vertices[:, 0] + .5), (m.vertices[:, 2] + .5)]
    m.visual = TextureVisuals(uv=uv, material=PBRMaterial(baseColorTexture=big))
    trimesh.Scene(m).export(path)
    a = load_glb(path)
    normalize(a, 1.0)
    side = target_side(a.parts[0])
    assert side in (1024, 2048, 4096)
    meta = prepare_asset(path, {"id": "W1-99", "size_m": 1.0}, str(tmp_path / "o"))
    assert max(meta["materials"][0]["size_px"]) <= 4096
    assert meta["materials"][0]["density_px_cm"] > 0


def test_batch_cli_runs_one_process_per_asset(sample, tmp_path):
    import shutil
    import subprocess
    masters = tmp_path / "masters"
    masters.mkdir()
    shutil.copy(sample, masters / "W1-01_bench.glb")
    manifest = tmp_path / "m.json"
    manifest.write_text(json.dumps({"wave": "w1", "items": [
        {"id": "W1-01", "size_m": 1.8, "ultra_tris": 6000},
        {"id": "W1-02", "size_m": 1.8}]}))
    staging = tmp_path / "staging"
    r = subprocess.run([sys.executable, "-I", os.path.join(os.path.dirname(HERE), "prepare_props.py"),
                        "--manifest", str(manifest), "--masters", str(masters),
                        "--staging", str(staging)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    report = json.load(open(staging / "report.json"))
    assert report["prepared"] == ["W1-01"] and report["no_master_yet"] == ["W1-02"]


def test_floor_keeps_uvs_intact_and_is_flagged(sample, tmp_path, monkeypatch):
    import gtp_assets.geometry as g
    real = g.decimate_part

    def floor(part, share, **kw):  # simulate a master that cannot go below 70 %
        return real(part, max(share, int(len(part.faces) * 0.7)), **kw)

    monkeypatch.setattr(g, "decimate_part", floor)
    meta = prepare_asset(sample, {"id": "W1-01", "size_m": 1.8, "ultra_tris": 3000},
                         str(tmp_path))
    assert meta["over_cap"] is True and meta["triangles"]["ultra"] > 3000
    a = load_glb(str(tmp_path / "W1-01" / "W1-01_ultra.glb"))
    assert a.parts[0].uv.min() >= -1e-4 and a.parts[0].uv.max() <= 1 + 1e-4


def test_budgets_match_the_art_bible():
    """Art bible 5.1 / 6: Nanite source caps, M1 LOD0 budgets, LOD counts and screen sizes."""
    from gtp_assets.manifest import BUDGETS, SCREEN_SIZES, lod_plan
    assert BUDGETS == {
        "small": {"ultra_tris": 300_000, "m1_tris": 3_000, "lods": 3},
        "medium": {"ultra_tris": 1_000_000, "m1_tris": 10_000, "lods": 4},
        "large": {"ultra_tris": 2_000_000, "m1_tris": 30_000, "lods": 4},
    }
    assert SCREEN_SIZES == [1.0, 0.5, 0.25, 0.1]
    for cat, b in BUDGETS.items():
        plan = lod_plan(b["lods"])
        assert len(plan) == b["lods"]
        # each LOD halves the triangles of the previous one, from the M1 LOD0 budget
        assert [e["percent_triangles"] for e in plan] == [0.5 ** k for k in range(b["lods"])]
        assert [e["screen_size"] for e in plan] == SCREEN_SIZES[:b["lods"]]


def test_unreal_script_sets_the_nanite_fallback_the_way_ue_requires():
    """Regression: with the default AUTO target UE ignores the percentage (found on 5.8.4)."""
    src = open(os.path.join(HERE, "..", "unreal", "import_props.py"), encoding="utf-8").read()
    assert "NaniteFallbackTarget.PERCENT_TRIANGLES" in src
    assert src.index("fallback_target") < src.index("fallback_percent_triangles")
    assert 'set_nanite(mesh, True, min(1.0, tris["m1_lod0"] / max(1, tris["ultra"])))' in src
    assert src.index("set_nanite(mesh, True, min") < src.index("build_lods(mesh, meta")  # fallback first


def test_wave2_manifest_has_names_and_sizes():
    from gtp_assets.manifest import parse_catalog
    with open(os.path.join(os.path.dirname(__file__), "..", "..", "..", "production", "asset-catalog.md"), encoding="utf-8") as f:
        items = parse_catalog(f.read(), "W2")
    assert len(items) == 30
    assert all(i.get("family") and i.get("asset") and i["size_m"] > 0 for i in items)


@pytest.mark.parametrize("wave,count", [("W13", 60), ("W14", 80), ("W6", 50), ("W8", 40)])
def test_spec_waves_have_names_and_sizes(wave, count):
    from gtp_assets.manifest import parse_catalog
    path = os.path.join(os.path.dirname(__file__), "..", "..", "..", "production", "asset-catalog.md")
    with open(path, encoding="utf-8") as f:
        items = parse_catalog(f.read(), wave)
    assert len(items) == count
    assert all(i.get("family") and i.get("asset") and i["size_m"] > 0 for i in items)


def test_rotate_x_lays_a_standing_master_flat():
    import numpy as np
    from gtp_assets.geometry import Asset, Part, normalize
    v = np.array([[-1, 0, 0], [1, 0, 0], [1, 2, 0], [-1, 2, 0]], dtype=np.float32)
    part = Part(vertices=v, faces=np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32),
                uv=np.zeros((4, 2), np.float32), normals=np.tile([0, 0, 1], (4, 1)).astype(np.float32),
                material=None, name="p")
    a = Asset(parts=[part])
    normalize(a, 2.0, rotate_x_deg=90)
    lo, hi = a.bounds()
    assert (hi - lo)[1] < 1e-4
