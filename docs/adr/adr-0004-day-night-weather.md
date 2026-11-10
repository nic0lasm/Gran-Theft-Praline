# ADR-0004: Code-driven day-night cycle and weather

*French version: [adr-0004-day-night-weather.fr.md](adr-0004-day-night-weather.fr.md)*

## Status

Proposed. Becomes Accepted once the seven ambiences of
`tools/environment/measure-ambiences-mac.sh` hold the M1 budget on the M1 Pro
MacBook Pro (p95 ≤ 42 ms, night ≤ 3 ms over day, rain ≤ 1 ms). The same run
closes the last open point of ADR-0001 (golden hour and night exposure).

## Date

2026-11-10

## Context

Version 0.2 is frozen at noon: fixed sun, exposure locked at EV100 14
(bf8acf1) because eye adaptation brightened the dark ground. The art bible
asks for a physical sun at 45.76 N, four states (noon, golden hour, rain,
night) with their exposure ranges, a dark night with pools of light, rain and
wet ground, fog on the rivers. The map is a binary shared by several work
streams, the M1 budget is tight (ADR-0002) and MegaLights does not exist on
the Mac.

## Decision

1. A C++ world subsystem (`UGTPEnvironmentSubsystem`) drives the sun, moon,
   sky, clouds, fog, exposure and night lights at run time, without editing
   any map; its settings live in `DefaultGame.ini`.
2. Sun and moon positions are computed (NOAA, lunar series) for Lyon legal
   time; the sun turns in 0.1 degree steps (virtual shadow map cache).
3. Histogram exposure with a narrow range (± 0.5 EV) around an EV100 target
   from the sun elevation and the weather, on a priority 1000 post-process
   component; the map's EV100 14 lock stays as the fallback.
4. Night lights generated from the city data and managed by a per-profile
   budget: only the nearest are on (m1: 64, 4 with shadows); volumetric fog
   off in m1.
5. Weather as blended states; wetness and night emissives through a Material
   Parameter Collection (`MPC_Environment`) read by the materials.
6. Free first: native Unreal sky, clouds and fog, procedural textures, no
   purchase.

## Consequences

- Positive: every street map gets the cycle without editing; each ambience
  is reproduced and measured from the command line; the cost of the night is
  capped by the light budget.
- Negative: every material generator has to call `add_weather()`
  (`tools/environment/unreal/gtp_weather.py`); the lights follow OpenStreetMap positions (file kept out of
  Git); the rain is a simple layer without splashes.
- The city and street dressing work multiply their emissives by
  `EmissiveScale`; the M1 profile cvars stay with the optimisation work.

## Alternatives considered

- **One map per state** (as the reference scene): no transitions, four
  binaries to keep up to date.
- **Ultra Dynamic Sky or another paid sky**: against "free first", the native
  one is enough.
- **All night lights on**: 423 lights on rue de la République alone, over the
  M1 budget.
- **Niagara rain particles**: more expensive and not scriptable without the
  editor; left to an effects pass.
