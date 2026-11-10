"""The ambiences of DefaultGame.ini against the art bible (section 3.2) and ADR-0002."""

import datetime
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import environment_model as em  # noqa: E402

CFG = em.load_config()

# Art bible 3.2: exposure range (EV100 min, max) of each lighting state.
BIBLE = {"noon": (13.5, 15.0), "golden": (10.0, 12.5), "rain": (10.5, 12.5), "night": (2.0, 6.0)}


def test_bible_states_stay_inside_their_exposure_range():
    half = CFG["ExposureHalfRange"]
    for name, (lo, hi) in BIBLE.items():
        _, ev, _ = em.ambience(CFG, name)
        assert lo <= ev - half and ev + half <= hi, (name, ev)


def test_summer_solstice_noon_sun_height_at_lyon():
    # 90 - latitude + obliquity = 67.7 degrees at solar noon
    utc = em.local_to_utc(datetime.datetime(2026, 6, 21, 13, 42))
    elev, az = em.sun_position(CFG["Latitude"], CFG["Longitude"], utc)
    assert abs(elev - 67.7) < 0.3 and abs(az - 180) < 3


def test_golden_hour_sun_is_low_in_the_west():
    elev, ev, _ = em.ambience(CFG, "golden")
    assert 3 <= elev <= 9


def test_lyon_legal_time():
    assert em.local_to_utc(datetime.datetime(2026, 7, 1, 12)).hour == 10
    assert em.local_to_utc(datetime.datetime(2026, 12, 1, 12)).hour == 11
    # 2026 changes: 29 March and 25 October
    assert em.local_to_utc(datetime.datetime(2026, 3, 29, 12)).hour == 10
    assert em.local_to_utc(datetime.datetime(2026, 10, 25, 12)).hour == 11


def test_ambiences_and_presets_are_consistent():
    presets = [w["Name"] for w in CFG["WeatherPresets"]]
    assert len(presets) == len(set(presets))
    assert CFG["StartWeather"] in presets
    names = [a["Name"] for a in CFG["Ambiences"]]
    assert set(names) == {"noon", "golden", "dusk", "night", "rain", "night-rain", "fog"}
    for a in CFG["Ambiences"]:
        assert a["Weather"] in presets, a
    for w in CFG["WeatherPresets"]:
        assert w["Weight"] > 0 and 0 <= w["Rain"] <= 1 and 0 <= w["CloudCoverage"] <= 1, w


def test_light_budgets_follow_adr_0002():
    budgets = {b["Profile"]: b for b in CFG["LightBudgets"]}
    assert set(budgets) == {"m1", "fidelity", "ultra"}
    m1 = budgets["m1"]
    # art bible 5.3 / ADR-0002: at most 8 shadowed local lights and 100 unshadowed in m1
    assert m1["MaxShadowedLights"] <= 8 and m1["MaxLights"] <= 100
    assert m1["bVolumetricFog"] is False
    # volumetric clouds cost about 12 ms in m1: the weather must not make them denser
    assert m1["bDriveCloudCoverage"] is False


def test_measure_script_knows_every_ambience():
    script = open(os.path.join(em.REPO, "tools", "environment", "measure-ambiences-mac.sh")).read()
    for a in CFG["Ambiences"]:
        assert a["Name"] in script
