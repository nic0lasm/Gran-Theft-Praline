# ADR-0010: Loading the quarter by cells

- Status: Proposed (2026-11-10); principle approved by the optimisation thread, accepted once its M1 measure meets the conditions below
- Context owners: city generation (Ville v2), optimisation, world streaming
- Related: ADR-0002 (performance budget), roadmap 0.9 "Tout le quartier"

## Context

Version 0.9 asks for Bellecour → Terreaux, both quays and the bridges, without loading
screens, within the M1 budget (ADR-0002: 25 fps floor, 30 fps target, ≤ 9 GB). One street
(rue de la République extended to the Rhone) already costs the whole GPU budget on M1:
21 299 facade instances, ~3 400 props and details, 65 % of frames under 33 ms. The full
quarter, cut into 30 cells of 250 m, holds 2 156 buildings and 174 185 facade modules: about
eight times the street. It cannot be one
level loaded at once.

Two constraints from the project:

1. **The map stays regenerable in one command.** Nothing is placed by hand: every actor
   comes from replayable data (tools/city/regen_city.sh). A cell must be regenerable on
   its own without touching the others.
2. **Headless and on one 16 GB M1.** Every step runs from a script on Nico's Mac, one
   Unreal at a time, with no interactive editor session.

The data side is ready: `tools/city/cells.py` cuts the quarter into 250 m cells on a grid
anchored at the zone origin (names such as `cell-p0000-m0250` never change when the quarter
grows) and generates each one outside the engine (buildings, facade modules, signs, ground
mask on carriageways fitted to the kerbs) into `gen/<cell>/`, the same layout as a street.

## Options

**A. World Partition** (one world, runtime grid, HLOD, data layers).
Native streaming and HLOD. But the world is one asset with one file per actor: regenerating
a cell means finding and deleting its actors among tens of thousands, then rebuilding the
HLOD of the whole grid (WorldPartitionHLODsBuilder, long and memory hungry on 16 GB). Python
placement into a World Partition map works but needs the cells loaded in the editor. HLOD
of instanced facades and translucent shop windows is hard to control.

**B. One streaming level per cell** (persistent level + `L_City_<cell>` sublevels,
loaded and unloaded by distance).
Each cell is its own level, written by the same scripts as today (`import_city.py`,
`place_facades.py`, `place_props.py`, `apply_city_materials.py` with the cell as focus):
regenerating a cell rewrites one file. The persistent level keeps the terrain, lighting,
sky, river and the far view: one cheap proxy per cell (the generated shells, already
produced as `shell.glb`, with the building colours), always loaded. A small C++ subsystem
loads the cells within a radius around the player (`ULevelStreamingDynamic`) and hides
their proxy when the detailed cell is visible. No engine HLOD build.

**C. Level Instances / Packed Level Actors** placed in a World Partition world.
Combines A and B but inherits the World Partition build and editor constraints; no gain at
our scale.

## Decision (proposed)

**B: one streaming level per cell, with a generated far proxy per cell**, for 0.9. It keeps
the one-command regeneration per cell, runs headless, and its cost is predictable: the 2 × 2
cells around the player in detail (conditions below), the rest as proxies.

Budget conditions (optimisation thread, 2026-11-10), to be met for acceptance:

1. **2 × 2 detailed cells around the player by default** (facades cull at 250 m, so a
   ~300 m radius is enough); 3 × 3 only if the measure allows it. City budget unchanged:
   **≤ 14 ms GPU**, about today's extended street, not eight times it.
2. **Far proxies**: generated shells with no dynamic shadow, no Lumen mesh cards, no
   collision, one material per cell, **≤ 100 draw calls** for all of them.
3. **No hitch**: asynchronous loading, never building actors or instances on the game thread
   in one block (HISM instances serialised in the cell level, or added in batches of
   ≤ 2 000 per frame), no Blueprint construction scripts, PSOs precached; **no frame over
   100 ms** when crossing cells at 50 km/h.
4. **Memory**: ≤ 1.5 GB for 4 detailed cells and all proxies.

Measured by the optimisation thread on a packaged build with a `-DemoRoute` that crosses at
least two cell borders; 250 m stays the cell size unless that measure says otherwise.

Revisit World Partition (A) if the hitches cannot be held or if the quarter grows past what
per-cell levels can manage.

## Consequences

- `regen_city.sh` gains a cell argument; the default keeps building the current street map.
- New C++: a cell streaming subsystem (loads by distance, swaps proxies), owned by the world
  streaming thread; the cell list comes from `processed/<zone>/cells.json`.
- Traffic lanes, pedestrians and police need cross-cell data: lanes stay zone-wide
  (one `lanes.csv`), actors are kept or recycled when their cell unloads.
- The street map `L_City_RueDeLaRepublique` stays as the test map until 0.9.

- 2026-11-11: first implementation on feat/city-perf (cell_proxies.py, GTP_CITY_LEVEL, UGTPCityStreaming, quarter_slot_mac.sh), see docs/city/generation.md; not measured yet.
