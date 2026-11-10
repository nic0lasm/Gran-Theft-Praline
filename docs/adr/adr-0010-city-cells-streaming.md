# ADR-0010: Loading the quarter by cells

- Status: Proposed (2026-11-10), to be accepted after the optimisation thread's M1 measure
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
the one-command regeneration per cell, runs headless, and its cost is predictable: at most
the 3 × 3 cells around the player in detail (~750 m across, more than the street's view
distance with the culling tables of `place_facades.py` / `place_props.py`), the rest as
proxies.

To be measured by the optimisation thread before acceptance:

- GPU and memory of 9 detailed cells + all proxies on M1 (target from ADR-0002);
- load hitches when a cell streams in at driving speed (50 km/h ≈ 14 m/s: a 250 m cell
  every ~18 s); async loading must stay under one frame of hitch;
- whether 250 m is the right cell size (smaller cells: smaller hitches, more levels).

Revisit World Partition (A) if the hitches cannot be held or if the quarter grows past what
per-cell levels can manage.

## Consequences

- `regen_city.sh` gains a cell argument; the default keeps building the current street map.
- New C++: a cell streaming subsystem (loads by distance, swaps proxies), owned by the world
  streaming thread; the cell list comes from `processed/<zone>/cells.json`.
- Traffic lanes, pedestrians and police need cross-cell data: lanes stay zone-wide
  (one `lanes.csv`), actors are kept or recycled when their cell unloads.
- The street map `L_City_RueDeLaRepublique` stays as the test map until 0.9.
