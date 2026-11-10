"""Python twin of GTPEnvironmentSubsystem's maths, for tests and tuning without Unreal.

Reads the [/Script/GranTheftPraline.GTPEnvironmentSubsystem] section of
Config/DefaultGame.ini and reproduces the sun position (NOAA), Lyon legal time
and the exposure target, so a changed curve or preset can be checked against
the art bible before it reaches the Mac.

Usage: python3 tools/environment/environment_model.py   (prints every test ambience)
"""

import datetime
import math
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
INI = os.path.join(REPO, "GranTheftPraline", "Config", "DefaultGame.ini")
SECTION = "[/Script/GranTheftPraline.GTPEnvironmentSubsystem]"


def _value(text):
    text = text.strip()
    if text.startswith("(") and text.endswith(")"):
        out = {}
        for key, val in re.findall(r'(\w+)=("[^"]*"|[^,()]+)', text[1:-1]):
            out[key] = _value(val)
        return out
    if text.startswith('"') and text.endswith('"'):
        return text[1:-1]
    if text in ("True", "False"):
        return text == "True"
    try:
        return float(text)
    except ValueError:
        return text


def load_config(path=INI):
    """Section as a dict; '+Key=' lines become lists."""
    cfg, inside = {}, False
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line.startswith("["):
                inside = line == SECTION
                continue
            if not inside or not line or line.startswith(";") or "=" not in line:
                continue
            key, val = line.split("=", 1)
            if key.startswith("+"):
                cfg.setdefault(key[1:], []).append(_value(val))
            else:
                cfg[key] = _value(val)
    return cfg


def _last_sunday_one_utc(year, month):
    day = datetime.datetime(year, month + 1, 1) - datetime.timedelta(days=1) if month < 12 else datetime.datetime(year, 12, 31)
    while day.weekday() != 6:
        day -= datetime.timedelta(days=1)
    return day.replace(hour=1)


def local_to_utc(local):
    summer = local - datetime.timedelta(hours=2)
    start, end = _last_sunday_one_utc(local.year, 3), _last_sunday_one_utc(local.year, 10)
    return summer if start <= summer < end else local - datetime.timedelta(hours=1)


def sun_position(lat, lon, utc):
    """(elevation, azimuth) in degrees, same equations as GTPSolar::Sun."""
    hour = utc.hour + utc.minute / 60.0 + utc.second / 3600.0
    days = 366.0 if (utc.year % 4 == 0 and (utc.year % 100 != 0 or utc.year % 400 == 0)) else 365.0
    g = 2 * math.pi / days * (utc.timetuple().tm_yday - 1 + (hour - 12) / 24)
    eqt = 229.18 * (0.000075 + 0.001868 * math.cos(g) - 0.032077 * math.sin(g)
                    - 0.014615 * math.cos(2 * g) - 0.040849 * math.sin(2 * g))
    decl = (0.006918 - 0.399912 * math.cos(g) + 0.070257 * math.sin(g) - 0.006758 * math.cos(2 * g)
            + 0.000907 * math.sin(2 * g) - 0.002697 * math.cos(3 * g) + 0.00148 * math.sin(3 * g))
    ha = math.radians((hour * 60 + eqt + 4 * lon) / 4 - 180)
    phi = math.radians(lat)
    cz = math.sin(phi) * math.sin(decl) + math.cos(phi) * math.cos(decl) * math.cos(ha)
    az = math.degrees(math.atan2(math.sin(ha), math.cos(ha) * math.sin(phi) - math.tan(decl) * math.cos(phi))) + 180
    return 90 - math.degrees(math.acos(max(-1.0, min(1.0, cz)))), az % 360


def exposure_target(cfg, elevation, weather):
    curve = sorted((p["X"], p["Y"]) for p in cfg["ExposureCurve"])
    if elevation <= curve[0][0]:
        base = curve[0][1]
    elif elevation >= curve[-1][0]:
        base = curve[-1][1]
    else:
        for (x0, y0), (x1, y1) in zip(curve, curve[1:]):
            if elevation <= x1:
                base = y0 + (y1 - y0) * (elevation - x0) / (x1 - x0)
                break
    daylight = max(0.0, min(1.0, (elevation + 6.0) / 12.0))
    return base + weather.get("ExposureOffset", 0.0) * daylight


def ambience(cfg, name):
    """(elevation, EV100 target, weather preset) of a test ambience."""
    amb = next(a for a in cfg["Ambiences"] if a["Name"] == name)
    weather = next(w for w in cfg["WeatherPresets"] if w["Name"] == amb["Weather"])
    y, m, d = (int(v) for v in amb["Date"].split("-"))
    local = datetime.datetime(y, m, d) + datetime.timedelta(hours=amb["Hour"])
    elev, _ = sun_position(cfg["Latitude"], cfg["Longitude"], local_to_utc(local))
    return elev, exposure_target(cfg, elev, weather), weather


def main():
    cfg = load_config()
    for amb in cfg["Ambiences"]:
        elev, ev, weather = ambience(cfg, amb["Name"])
        print("%-11s %s %5.2f h  %-8s sun %6.1f deg  EV100 %5.2f (%.2f - %.2f)" % (
            amb["Name"], amb["Date"], amb["Hour"], weather["Name"], elev, ev,
            ev - cfg["ExposureHalfRange"], ev + cfg["ExposureHalfRange"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
