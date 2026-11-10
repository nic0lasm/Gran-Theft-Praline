# Day-night cycle and weather

*French version: [day-night-weather.fr.md](day-night-weather.fr.md)*

Decision: [ADR-0008](../adr/adr-0008-day-night-weather.md). Art values:
[art bible](../art/art-bible.md), sections 3 and 9. Budget:
[ADR-0002](../adr/adr-0002-performance-budget.md).

## 1. Principle

The C++ world subsystem `UGTPEnvironmentSubsystem`
(`Source/GranTheftPraline/*/GTPEnvironmentSubsystem.*`) drives the lighting
**at run time**. No map is edited: it reuses the map's sun, sky, fog and
clouds, and adds its own components on a transient `GTP_Environment` actor
(moon, post-process, rain layer, night lights, Saone mist). A map without a
directional light is left alone; `-NoEnvironment` gives back the authored
lighting (the map's EV100 14 lock is the fallback).

All settings live in `Config/DefaultGame.ini`, section
`[/Script/GranTheftPraline.GTPEnvironmentSubsystem]`.

## 2. Time, sun and moon

- Sun position: NOAA equations (about 0.1 degree); moon: truncated series
  (about 1 degree) with its phase. Observer: 45.7597 N, 4.8320 E (place
  Bellecour).
- Lyon local time with European daylight saving (last Sunday of March and
  October at 01:00 UTC).
- Start: 15 October 2026, 2 pm (autumn, bible 3.3). Default speed 30: a day
  lasts 48 minutes.
- The sun only turns in 0.1 degree steps to keep the virtual shadow map
  cache; it stops casting shadows below the horizon and switches off below
  −10 degrees. The moon (no shadows, atmosphere light 1) takes over below −4,
  at 0.4 lux × phase × (1 − 0.85 × clouds).

## 3. Exposure

Histogram, EV100 bounds computed continuously: a target from the sun
elevation (`ExposureCurve`) plus the weather offset, ± 0.5 EV. Unbound
post-process component, priority 1000, above the map's volume. Adaptation 2
EV/s towards bright, 1 EV/s towards dark.

| Bible state | Sun | Target | Bible range |
| ---- | ---- | ---- | ---- |
| Noon (21 June, 1:30 pm) | 67.6° | 14.5 | 13.5 – 15 |
| Golden hour (15 Oct, 6:15 pm) | about 6° | 11.5 | 10 – 12.5 |
| Rain (9 Nov, 11 am) | 24.7° | 13.45 − 2.4 = 11.05 | 10.5 – 12.5 |
| Night (15 Jan, 10 pm) | −48° | 3.5 | 2 – 6 |

White balance 6500 K by day, 5400 K at night (warm without turning sepia).
Saturation follows the weather (0.8 in the rain). `tools/environment/environment_model.py`
redoes these computations outside Unreal and the tests check every state against the bible.

## 4. Night

- **Lights**: `Content/Environment/Data/NightLights.csv`, written by
  `tools/environment/build_night_lights.py` (not in Git, OpenStreetMap
  positions): one point light per W1-04 street lamp (lantern at 4.10 m,
  3500 lm, 3000 K, 18 m radius), one rect light per shop front (2.60 m high,
  60 cm in front of the facade, tilted 25 degrees down, 1200 lm, 4000 K, 9 m
  radius), one W14-43 wall sconce when the street dressing adds them. Map
  actors tagged `GTP_StreetLight` or `GTP_ShopLight` are added.
- **Budget**: every 0.2 s only the N lights nearest to the camera are on
  (lights behind count 2.5 times farther), the nearest shadow-capable ones
  cast shadows, with a fade over the last fifth of the range.

  | Profile | Lights | Shadowed | Range | Volumetric fog |
  | ---- | ---- | ---- | ---- | ---- |
  | m1 | 64 | 4 | 90 m | no |
  | fidelity | 96 | 6 | 120 m | yes |
  | ultra | 160 | 12 | 150 m | yes |

- **Cars**: every vehicle (traffic, police, the player's) gets two headlights
  (12,000 cd spots, 5000 K, 32 degree cone, 35 m range) and a red tail light,
  never shadowed; at night only the `MaxLitCars` nearest cars are lit (m1: 4,
  fidelity: 6, ultra: 10).
- **Clouds**: quality per profile (`bVolumetricClouds`, `CloudSampleScale`,
  `bCloudShadows`, `bDriveCloudCoverage`). Volumetric clouds cost about 12 ms
  GPU in m1 (measured 2026-11-10): in m1 the weather does not change their
  coverage (overcast goes through the sun, sky and fog) until the decision on
  their quality. Per weather, `CloudSampleScale` multiplies the profile's,
  and `bCloudDomeWhenOvercast` shows the 2D dome instead of the volumetric
  clouds in overcast, rain, storm and fog (`bPreferCloudDome`). Cheap fallback: `bCloudDome` turns the volumetric clouds
  off and shows an unlit 2D cloud dome around the camera (`M_CloudDome`,
  coverage `Cloudiness`, luminance matched to the exposure through
  `CloudLuminance`).
- **Cheap cloud shadows**: `bCloudShadowLightFunction` (off by default) puts the
  `M_CloudShadowLF` light function on the sun: two octaves of drifting noise on
  the ground plane, coverage from `Cloudiness`, a shadowed spot keeps 25 % of the
  sun. It replaces the volumetric cloud shadow map (8.6 ms of the 12 ms of the
  clouds on the M1): expected cost 0.3 to 0.5 ms, to be confirmed on the bench.
  A/B in one command: `tools/environment/sky-ab-test-mac.sh <build_dir>`
  (bench frame times and stills per mode, `index.html` side by side).
- **Switch-on**: between +2 and −4 degrees of sun (earlier under cloud).
- **Emissives**: `EmissiveScale` in `MPC_Environment` goes from 1 to 20 at
  night; shop window and sign materials multiply by it.
- **Sky**: the fog takes an orange city tint at night.

## 5. Weather

Six states blended over 60 s; the automatic weather draws one every 3 game
hours, weighted for a Lyon autumn.

| State | Clouds | Sun | Fog | Rain | EV | Weight |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Clear | 0.15 | 1 | 0.008 | 0 | 0 | 3 |
| Cloudy | 0.55 | 0.7 | 0.012 | 0 | −0.7 | 3 |
| Overcast | 0.95 | 0.15 | 0.02 | 0 | −2.2 | 2 |
| Rain | 1 | 0.1 | 0.035 | 0.7 | −2.4 | 2 |
| Storm | 1 | 0.06 | 0.05 | 1 | −3 | 0.5 |
| Fog | 0.7 | 0.35 | 0.12 | 0 | −1.5 | 1 |

- **Wet ground**: `Wetness` rises with the rain (about 40 s at rain 1) and
  dries in about 7 min; `Puddles` follows above 0.55. The material
  generators (city, prop import, street dressing) call
  `tools/environment/unreal/gtp_weather.py` `add_weather(mat, roles)` at the
  end of their graph: albedo darker (× 0.55) and roughness 0.12 with
  `Wetness` (less on walls), puddles on flat ground (roughness 0.03, 6 m
  noise), emissive × `EmissiveScale`. The wet look thus survives every
  regeneration; the collection and the puddle texture are created on first
  use. Until then, `GTP_ENV_WET=on` adds the same nodes to the
  existing materials (`M_City_Ground`, `M_City_Shell`, `M_City_Kit`,
  `M_Master_Surface`, `M_Sign_Fascia`), once per material.
- **Rain**: a lit translucent cylinder of 24 m around the camera, two layers
  of scrolling streaks, opacity × `RainIntensity`, depth fade.
- **Saone mist**: six local fog volumes along the Presqu'ile quays
  (`RiverFogCentres`, about 50 m accuracy, to be refitted on the extended
  map), weighted by the state plus the morning mist (5 to 10 am, peak at
  7:30, October to March).

## 6. Commands

Command line: `-TimeOfDay=22 -Date=2027-01-15 -Weather=Rain -TimeScale=0`
(0 freezes time for measurements), `-Ambience=<noon|golden|dusk|night|rain|night-rain|fog>`
(frozen test ambience, defined in `Ambiences`), `-CloudShadows=0` (no cloud shadows, exposure compensated by
`NoCloudShadowExposureOffset` EV), `-CloudDome=1` (2D cloud dome instead of the volumetric clouds, 0 = never),
`-Sky=volumetric|lightfunction|cheap` (sets the three sky switches for A/B), `-CloudShadowLF=0/1` (light function cloud shadow),
`-DayNight=0` or `-NoEnvironment` (switches the system off). At start a `daynight: on | ...` or `daynight: off` line goes to stdout, Shipping included.

Unattended chain: `tools/environment/setup-environment-mac.sh` (non-zero
exit on error, JSON report), then packaging, then
`GAME_ARGS="-Ambience=night" tools/build/test-build-mac.sh` or
`tools/environment/measure-ambiences-mac.sh`.

Console (not in Shipping): `gtp.Time 18.25`, `gtp.Date 2026-06-21`,
`gtp.TimeScale 600`, `gtp.Weather Rain 30`, `gtp.EnvStatus`.

## 7. Setup on the Mac and measurement

1. `tools/environment/setup-environment-mac.sh` (after the city import): the
   lights file, `MPC_Environment`, procedural textures, rain, wet materials only with `GTP_ENV_WET=on`.
   Report `Saved/Environment/setup_report.json`, with the cloud material
   parameters to copy into `CloudCoverageParameters`.
2. Build and package (`tools/build/package-mac.sh`).
3. `tools/environment/measure-ambiences-mac.sh <version>`: seven frozen
   ambiences (noon, golden hour, dusk, night, rain, rainy night, fog), 120 s
   of frame times and a 20 s film each, table
   `Builds/<version>/evidence/ambiences-<date>.md`.

Criteria (m1 profile): p95 ≤ 42 ms in every ambience; night ≤ 3 ms GPU over
day; rain ≤ 1 ms; daytime clouds, sky and fog ≤ 1.5 ms (budgets from the
optimisation work, to be confirmed).

## 8. Known limits

- No splashes or run-off yet, no rain sound, no dynamic water reflections;
  rain falls through awnings.
- The coverage parameter names of the native cloud material must be checked
  on the Mac (report of step 1).
- No glowing lamp heads (emissive) until the W1-04 model has a separate glass
  material.
