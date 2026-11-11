# Development roadmap: Gran Theft Praline

*French version: [roadmap.fr.md](roadmap.fr.md)*
*Version 3, 2026-11-10: review after 0.2. Versions reordered around the open workstreams, per-system performance budget, gaps added.*
*Version 3.1, 2026-11-11: 0.9 becomes the quarter test build, quality gates on every merge, gaps on the way to testing. Details: [test-phase-plan.md](test-phase-plan.md).*
*Systems checklist: [systems-inventory.md](systems-inventory.md). Tracking: [roadmap-status.md](roadmap-status.md). Assets: [asset-catalog.md](asset-catalog.md).*
*Inputs: [concept](../design/gdd/game-concept.md), [rendering strategy](../docs/rendering/rendering-strategy.md),
[ADR-0001](../docs/adr/adr-0001-rendering-pipeline.md), [ADR-0002](../docs/adr/adr-0002-performance-budget.md),
[art bible](../docs/art/art-bible.md).*

---

## 1. How to read this roadmap

- The game moves forward in **playable versions**: 0.1, 0.2, 0.3… up to
  **1.0 = vertical slice** (north of the Presqu'île, Bellecour → Terreaux,
  every P0 feature of the concept at final quality).
- **Every version launches in one click** on the Mac and adds something you
  can see or do while playing.
- A version ships when its "done when" criterion is verified by a measurement
  or a video, its **performance budget** is met (§3), CI is green and the
  commit is fast-forwarded to `next` then `main`. A version is not held back
  for a system that is not ready: that system moves to the next version.
- **The project is paced by the Mac, not by the calendar.** Code, data,
  assets and documentation are prepared in parallel in the cloud; everything
  that needs Unreal (import, cook, packaging, measurement) goes through a
  single MacBook Pro, one job at a time. Versions are therefore ordered by
  **priority of access to the Mac**, and targets are orders of magnitude,
  reviewed at every version.

### Two quality levels: photorealism first

| Profile | Role | Criterion |
| ---- | ---- | ---- |
| **Ultra (photorealistic)** | The visual target, without compromise | Validated with captures compared to real photos, even at a few frames per second on the M1 |
| **M1 (playable)** | What we play on the MacBook Pro M1 Pro 16 GB | **Capped at 40 fps (steady 25.0 ms), no frame > 100 ms** (decision of 2026-11-11, measured at tier C: 50 pedestrians, 25 driven + 24 far cars, police); replaces the 30 fps target of [ADR-0002](../docs/adr/adr-0002-performance-budget.md), to be amended |

The M1 profile is derived from the Ultra profile (LODs, densities,
settings), never the other way round. Any visible loss of image quality goes
through a decision card to Nico.

### The four major stages

| Stage | Versions | What we get |
| ---- | ---- | ---- |
| **A. The living street** | 0.1 → 0.7 | A Lyon street where you walk, drive through traffic, meet pedestrians, get chased by the police, by day and by night, and fight |
| **B. The whole district as systems** | 0.8 → 0.12 | Full Bellecour → Terreaux, shops, money, saving, map and GPS, arsenal, activities: every P0 line ticked |
| **C. Final quality** | 0.13 → 0.16 | Facades and landmarks, interiors, population and vehicles, sound, at Ultra level |
| **D. Vertical slice** | 0.17 → 1.0 | Polish, one hour of play without blockers, parity review |
| Then | 1.x | New districts, P1 features, story written with Nico |

### Ongoing workstreams (every version)

These workstreams are not versions: they deliver a batch in every version.

| Workstream | What it delivers in each version | Owner |
| ---- | ---- | ---- |
| **M1 optimisation** | The version's budget met, measured over 600 s in the build | Optimisation thread (engine config, budget) |
| **Street realism** | A batch of details checked against real photos (small details first) | Street realism thread (set dressing) |
| **City generation** | Geo data, lanes, map extension | City v2 thread |
| **Assets** | Free first (Fab, Megascans, CC0), then Tripo; import, LODs, collision | Free packs, Tripo, Import threads |
| **Audio** | The sound of the version's systems (footsteps, engines, sirens, impacts), mixing | *to open* (gap in v2) |
| **Animation** | The animations of the version's systems (getting into cars, reactions, combat) | *to open* (gap in v2) |

---

## 2. Where we stand (2026-11-10)

**Update of 2026-11-11.** 0.3.0 shipped on 2026-11-10 with previews of the
0.4 to 0.6 systems (pedestrians, traffic lights, ragdoll, police, minimap,
saved settings, day/night, sounds). The performance worksite that followed
holds tier C capped at 40 fps on the M1 (`next` = 3dcf33b). The code of
versions 0.4 to 0.6 is therefore already there: they will ship as a train
once their gaps are closed. Nothing exists yet for 0.7, 0.8 and 0.9. See the
[plan to the test phase](test-phase-plan.md).

*Table of 2026-11-10:*

| Version | State |
| ---- | ---- |
| **0.1 Play in one click** | ✅ shipped 2026-11-09 (release v0.1.0, 702 MB) |
| **0.2 The dressed street** | ✅ shipped 2026-11-10 (release v0.2.0, 1.36 GB): 600 s on the M1 profile, p50 29.9 ms, p95 40.3 ms, 84.9 % of frames ≤ 33 ms. Known issues: leafless plane trees, missing logos |
| **0.3 Behind the wheel** | 🟡 driving, traffic on 112 lanes, carjacking with pull-out and the extended map work; **blocked by the budget**: 6 NPCs cost about 10 ms of GPU, and the extended map keeps only 65 % of frames under 33 ms |
| Workstreams opened today | Pedestrians, Police, Day/night + weather, Optimisation, Street realism, City v2, Import, Free packs, Tripo |

Roadmap v2 planned three weeks per version; the first two shipped in two days
thanks to parallel threads. The real constraint has surfaced: the **M1 GPU is
already full** with an empty street (p50 29.9 ms), and every living system
(traffic, pedestrians, night, effects) must pay for its place.

---

## 3. Per-system performance budget

Until now only the total frame time was measured, so one system could eat
the margin of all the others. From 0.3 on, every system has a GPU envelope
on the M1 profile, measured at every version by the Optimisation thread
(`stat gpu`, `ProfileGPU`, capture at the same spot in the street).

Proposed envelope for the 30 fps target (33.3 ms), to refine by measurement:

| Item | GPU envelope (M1 profile) | Content |
| ---- | ---- | ---- |
| City (buildings, ground, props, vegetation) | 14 ms | facades, props, plane trees, culling and HLOD |
| Lighting, shadows, global illumination | 7 ms | software Lumen, shadows, sky |
| Traffic | 2 ms | at most 12 visible cars close to the player, impostors or hidden beyond |
| Pedestrians | 2.5 ms | 40 visible pedestrians, aggressive LODs, skeleton-less Mass beyond |
| Player, police, nearby characters | 1.5 ms | |
| Night, weather, effects (rain, headlights, street lamps, Niagara) | 2 ms | night lights budgeted separately |
| Post-processing, TSR, UI | 2.3 ms | |
| Margin (hitches, spikes) | 2 ms | |
| **Total** | **33.3 ms** | |

Rules:

- A system over its envelope does not go to `next`; it reduces its density or
  cost, or the Optimisation thread frees room elsewhere.
- The CPU (game thread, AI, physics) has an envelope too: 25 ms per frame at
  most, measured with `stat unit`.
- While the provisional target (p95 ≤ 42 ms) applies, envelopes are
  multiplied by 1.25.

---

## 4. Versions at a glance

| Version | Name | You can… | Order of magnitude |
| ---- | ---- | ---- | ---- |
| **0.1** | Play in one click | launch the game and walk down Rue de la République | ✅ 2026-11-09 |
| **0.2** | The dressed street | stroll down a faithful street at 25 fps on the M1 profile | ✅ 2026-11-10 |
| **0.3** | Behind the wheel | steal a car and drive through traffic to the Rhône quay and Pont Lafayette | November 2026 |
| **0.4** | The living street | meet pedestrians who react, stop at traffic lights, knock street furniture over | December 2026 |
| **0.5** | The police arrive | get wanted (1-2 stars), chased by car, arrested or lose the police | December 2026 |
| **0.6** | Day and night | drive by day, at dusk, at night and in the rain | January 2027 |
| **0.7** | Armed | fight bare-handed and with a pistol, take cover, die and respawn; police shoot back | February 2027 |
| **0.8** | City life | enter a shop without loading, buy, rob it, earn money, save in a safe house | March 2027 |
| **0.9** | The whole district: test build | roam Bellecour → Terreaux without loading, find your way on the map, play 3 activities, save; then the **test phase** | April 2027 |
| **0.10** | Map and comfort | open the Lyon map, follow the GPS, set keys, accessibility, gamepad | May 2027 |
| **0.11** | The arsenal | choose among 6 weapons, face level-3 police and roadblocks | June 2027 |
| **0.12** | Your own city | play free activities and missions, ride a scooter and a Vélo'v: **P0 complete milestone** | summer 2027 |
| **0.13** | Facades and landmarks | see the district at final quality, landmarks included | autumn 2027 |
| **0.14** | Interiors | enter 30 shops, 10 buildings, 3 safe houses at final quality | autumn 2027 |
| **0.15** | People and cars | meet a final population and police, 6 final vehicles | winter 2027 |
| **0.16** | The sound of Lyon | hear the final city: per-street ambiences, recordings made in Lyon | winter 2027 |
| **0.17** | Polish | play one hour without blockers, budget met everywhere | early 2028 |
| **1.0** | Vertical slice | play the full vertical slice, parity review | early 2028 |

Orders of magnitude assume the pace seen in November 2026 (parallel threads,
Mac available several hours a day). They are **low confidence** and reviewed
at the end of every version.

### Why this order

*Decided by Nico on 2026-11-10 (card): the living street first.*

1. **The living street before weapons.** Traffic, pedestrians, police and
   day/night already have open workstreams; integrating them first uses that
   work right away and measures early the most GPU-hungry items (crowd,
   traffic, night lights). It is also what sets a GTA apart from a corridor
   action game.
2. **Police before combat.** Stealing a car or running a pedestrian over is
   enough for 1-2 stars; the police directly reuse traffic and pedestrians.
   They will shoot once combat exists (0.7).
3. **Day and night before combat.** The workstream is open and touches
   rendering, hence the budget: better to know the cost of night lights
   before adding weapon effects.
4. **Combat in 0.7** leaves time to sort out the animation source and
   physical reactions (0.4), without which combat does not feel like a GTA.
5. **The full district in 0.9**, after the systems, because streaming is
   tuned better once we know what it must carry (crowd, traffic, police).

---

## 5. Version details

Each sheet: goal, "done when", tasks, workstreams. Ongoing workstreams (§1)
deliver their batch in every version and are not repeated.

### Stage A: the living street

#### 0.1 Play in one click: ✅ shipped

One-click macOS app, title screen, pause menu, GASP character, GTA
third/first-person camera, Rue de la République greybox, evidence in
[evidence/0.1](evidence/0.1/).

#### 0.2 The dressed street: ✅ shipped

Rue de la République from LiDAR HD and Grand Lyon data, 30-module facade kit,
202 shop signs, 1,203 props, fixed ground, two quality profiles, 600 s
measured on the M1 profile (p95 40.3 ms). On-foot feel (camera, "F Get in"
prompt, HUD, ambience) added.

#### 0.3 Behind the wheel: 🟡 in progress

**Goal.** Steal a car and drive through Rue de la République traffic to the
Rhône quay and Pont Lafayette.

**Done when:**
- Drivable Clio V, 208 II and Kangoo II; get in, get out, break the window,
  pull out an NPC driver.
- NPC traffic on lanes from BD TOPO; engine sound; driving panel.
- Map extended to the Rhône quay and Pont Lafayette, with collision.
- **Falling into the Rhône handled**: a player or car that falls into the
  water respawns on the quay (swimming stays in 1.x). Water surface: our own mesh with
  the `M_City_Water` material (small waves, opaque); the native Water plugin is
  ruled out (extra landscape and passes, over the M1 budget).
- Video of a 3-minute drive; **p95 ≤ 42 ms** on the M1 profile with traffic
  (threshold set to ship 0.3 without waiting for the rest).
- §3 GPU envelopes measured a first time.
- Distribution: if the archive exceeds 2 GiB (GitHub's per-file release
  limit), splitting documented in the README.

**Workstreams:** 0.3 (lanes, NPC driving, HUD), City v2 (quay, bridge, water),
Optimisation (budget), Vehicles.

#### 0.4 The living street

**Goal.** The street lives: pedestrians walk and react, cars stop at the
lights, street furniture breaks when you hit it.

**Done when:**
- Mass + StateTree pedestrians on the pavements, adjustable density; 40
  visible near the player within the 2.5 ms envelope.
- Reactions: look, step aside, flee an accident; a pedestrian who is hit goes
  **ragdoll** and gets up or stays down.
- **Player physical reactions**: stumble, fall out of a moving car, get hit.
- Traffic lights and pedestrian crossings on Rue de la République; NPC
  drivers obey them, honk, avoid.
- **Breakable props**: bollards, signs, bins, barriers tip over or break on
  impact (Chaos, broken version of the existing meshes).
- Vault over low obstacles (low wall, bollard) with GASP.
- Population variety: number of models and MetaHuman / LOD strategy decided
  and measured.
- Game text moved to string tables, in French and English.

**Workstreams:** Pedestrians, 0.3 (lights, NPC driving), Animation and
physical reactions (*to open*), Street realism (breakable props).

#### 0.5 The police arrive

**Goal.** A crime seen by a witness triggers a level 1 or 2 wanted level you
can shake off.

**Done when:**
- Crimes: car theft, pedestrian run over, serious crash, hitting an officer;
  reported by witnesses (no magic detection).
- Stars 1 and 2, search area, line of sight, escape and hiding.
- Foot and car patrols with fictional police cars, **sirens and flashing
  lights**, chases, arrest with penalty and quick restart.
- **Minimap** in the HUD (streets, police, search area).
- **Saved settings** (graphics profile, mouse, volumes).
- Get-in / get-out car animations replacing the fixed durations.
- Video of a 3-minute chase; budget met with police, traffic and pedestrians.

**Workstreams:** Police, Interface and settings (*to open*), Audio (*to
open*), Animation.

#### 0.6 Day and night

**Goal.** Time passes: sun at Lyon's latitude, dusk, lit night, rain and wet
ground.

**Done when:**
- Adjustable day/night cycle (time speed), real sun path over Lyon, exposure
  correct by day and night (ADR-0001).
- **Night lighting**: street lamps, shop windows, lit windows, car
  **headlights, brake lights and indicators**.
- Rain, puddles, wet ground, fog; traffic and pedestrians adapt (umbrellas,
  speed).
- Ultra captures at noon, golden hour, night and rain compared with photos;
  "night, weather, effects" envelope met.

**Workstreams:** Day/night + weather, Vehicles (lights), Street realism (shop
windows and windows), Optimisation.

#### 0.7 Armed

**Goal.** You fight: bare hands and pistol, cover, health, death and
respawn; police shoot at level 2.

**Done when:**
- ADR-0011 (gameplay framework: Gameplay Ability System, StateTree; number 0008 is taken by day/night).
- Melee (punches, dodge, takedown); pistol (aim, shoot, reload, recoil),
  **gamepad aim assist**, shoulder swap.
- Cover against walls and cars, moving between covers, blind fire.
- Health, armour, **stamina**, death, respawn at a (fictional) hospital.
- Localised hit reactions, ragdoll on death.
- **Visual effects**: per-surface impacts, sparks, glass, restrained blood,
  smoke; car fire and explosion.
- Pedestrians flee weapons; police shoot at level 2.
- Video of a 2-minute fight; budget met.

**Workstreams:** Combat and weapons (*to open*), Animation, Audio, Police.

### Stage B: the whole district as systems

| Version | Done when | Workstreams |
| ---- | ---- | ---- |
| **0.8 City life** | ADR-0012 (interiors without loading; city cells are in ADR-0010); save versioning; a shop without a loading screen (Level Instance or data layer), NPC shopkeeper who serves, gives in or calls the police, purchase, hold-up; persistent money; 1 safe house, **autosave and safe-house save**; doors and containers | Shops and interiors, Economy and saving |
| **0.9 The whole district** | Bellecour → Terreaux, quays and bridges in one command (City v2); World Partition, HLOD; full route filmed on foot and by car, budget met, memory ≤ 9 GB; TCL buses as scenery in traffic. **Pulled forward for testing**: map with real street names and simple GPS, minimal mission framework and 3 activities, remapping and gamepad glyphs, credits, test build with logs, macOS and Windows builds. Full criteria: [plan to the test phase](test-phase-plan.md) | City v2, World streaming, Optimisation, Interface, Missions, QA and release |
| **0.10 Map and comfort** | What remains after 0.9: GPS that follows one-way streets; full key remapping, sensitivity, accessibility (subtitles, aim assist, colours); gamepad glyphs and rumble; cinematic car camera; first NPC speech in French | Interface and settings, City v2, Audio |
| **0.11 The arsenal** | 6 fictional weapons, weapon wheel, limited inventory; level 3, roadblocks, armed chases; 5 shop types (grocery, clothes, gun shop, garage, café), 3 safe houses | Combat and weapons, Police, Shops |
| **0.12 Your own city** | Complete mission framework (the minimum arrives in 0.9); the concept's 6 free activities built with it; scooter, motorbike, Vélo'v; **"P0 systems complete" milestone**: every P0 line of the [inventory](systems-inventory.md) ticked at working quality | Missions and activities (*to open at 0.8*), Vehicles |

### Stage C: final quality

Final art is produced **ahead of time** during A and B (Tripo waves, kits,
photos, free packs) and integrated here.

| Version | Done when | Workstreams |
| ---- | ---- | ---- |
| **0.13 Facades and landmarks** | Facade kit v2 over the whole district; Hôtel de Ville, Opéra, Bellecour and its statue, bridges; Ultra captures by day, night, rain; M1 budget met across the district | City v2, Street realism, Landmarks |
| **0.14 Interiors** | 30 shops, 10 buildings (halls, stairwells, courtyards, roofs, car park), 3 safe houses at final quality | Shops and interiors, Assets |
| **0.15 People and cars** | Final population and police (fictional faces), 6 final vehicles from the French fleet, fictional police car, TCL bus | Pedestrians, Vehicles, Assets |
| **0.16 The sound of Lyon** | Per-street ambiences, facade reverb, recordings made in Lyon, final mix | Audio |

### Stage D: vertical slice

| Version | Done when | Workstreams |
| ---- | ---- | ---- |
| **0.17 Polish** | Hitches, memory, bugs; one hour of free play without blockers on the M1 profile; budget met everywhere; 60 fps mode study | Optimisation, Quality, Nico's playtests |
| **1.0 Vertical slice** | Line-by-line parity review against the [inventory](systems-inventory.md); showcase video; Nico's decision on what comes next | Parity review, Showcase |

### After 1.0

One district at a time (south Presqu'île, Vieux-Lyon, Croix-Rousse, left
bank, Fourvière), P1 features (swimming, phone, radio, garages,
customisation, helicopter, greeting and antagonising NPCs, ambient events)
and the story written with Nico.

---

## 6. Assets

- **Free first**: Fab and Megascans (free for life), Megaplants, Poly Haven
  and CC0 textures, MetaHuman; check these sources before any generation. No
  GTA V mod packs (rights of Rockstar and of the modders, public repository
  exposed to takedowns).
- **Tripo Studio Max** next for isolated objects (props, accessories,
  interiors, car bodies), "no logo"; real shop signs come from photos and
  stay outside Git (`Plugins/CityBrands/`). No spending without a decision
  card to Nico.
- **Flow**: catalogue → generation → cleanup (scale, pivot, LODs, collision)
  → headless import → line in `assets/SOURCES.md` → status in the
  [catalogue](asset-catalog.md).
- **Masters** kept at high quality outside Git; light variant for the M1
  profile.
- **New from 0.4**: every street object that can be hit has a breakable or
  movable version.

---

## 7. Systems, versions and dependencies

| # | System | Versions | Depends on | Starting point |
| ---- | ---- | ---- | ---- | ---- |
| 1 | Core code, packaging, measurement | 0.1 | nothing | `tools/build/package-mac.sh` |
| 2 | On foot and camera | 0.1, 0.2, 0.7 | 1 | Game Animation Sample |
| 3 | Contextual interactions | 0.2, 0.8 | 2 | `UGTPInteractionComponent` |
| 4 | Vehicles | 0.3, 0.6, 0.12, 0.15 | 2, 3 | Chaos Vehicles, `AGTPVehicle` |
| 5 | Traffic | 0.3, 0.4, 0.9 | 4, geo lanes | `AGTPTrafficNetwork` |
| 6 | Physical reactions, ragdoll | 0.4, 0.7 | 2 | Physics Asset, Physical Animation |
| 7 | Animations beyond locomotion | 0.4 → 0.11 | 2 | to survey (free Epic content, Fab) |
| 8 | Pedestrians and NPC AI | 0.4, 0.15 | 6 | Mass, StateTree, Smart Objects |
| 9 | Breakable props, visual effects | 0.4, 0.7 | 6 | Chaos, Niagara |
| 10 | Police and wanted level | 0.5, 0.7, 0.11 | 5, 8 | in-house on StateTree |
| 11 | Interface, settings, FR/EN text | 0.4, 0.5, 0.10 | 1 | UMG, Common UI, string tables |
| 12 | Time and weather, night lighting | 0.6 | rendering | Sky Atmosphere, SunPosition |
| 13 | Combat, health, death | 0.7, 0.11 | 2, 6, 7 | Gameplay Ability System |
| 14 | Shops, interiors, economy, saving | 0.8, 0.11, 0.14 | 3, 8, 15 | Level Instances, SaveGame |
| 15 | World streaming | 0.8, 0.9 | city generation | World Partition, HLOD |
| 16 | Map, GPS | 0.5 (minimap), 0.10 | geo data | in-house tiles |
| 17 | Missions and activities | 0.12 | 10, 13, 14 | in-house |
| 18 | Audio | ongoing, 0.16 | all | MetaSounds |

---

## 8. Risks

| Risk | Probability | Impact | Mitigation |
| ---- | ---- | ---- | ---- |
| **M1 Pro GPU already full** (p50 29.9 ms on an empty street; +10 ms for 6 NPCs; 65 % of frames < 33 ms on the extended map) | Certain | Very high | Per-system envelope (§3); Optimisation thread owns the budget; culling, impostors, aggressive LODs, skeleton-less Mass at distance; no version on `next` over budget |
| **A single Mac for everything** (editor, cook, packaging, measurement, and sometimes Nico playing) | Certain | High | Mac queue kept by the coordinator; everything prepared in the cloud before the slot; headless scripts; `caffeinate` for long jobs; one Unreal at a time |
| **16 GB of memory** (editor + cook, full map in 0.9) | High | High | Small streaming cells, HLOD from 0.8, memory measured at every version |
| **No animations available** for combat, vehicles, NPCs | High | High | Animation workstream opened from 0.4; survey of free sources before any combat development |
| **Clones diverging on the Mac** (maps and materials copied between clones) | High | Medium | Check before every packaging; a single source for each shared asset |
| **Game size** (1.36 GB zip at 0.2, GitHub limit of 2 GiB per file) | High | Medium | Documented archive splitting; per-profile texture compression |
| **Experimental frameworks** (Chaos Vehicles 5.8, Mass, Mover) | High | Medium | Behind our interfaces; actor fallback |
| **Integrating parallel workstreams** (traffic, pedestrians, police touch the same NPCs) | High | Medium | Owners set by the coordinator; shared interfaces (perception, crimes) defined before the code |
| **Quality of generated models** | High | Medium | Free first, cleanup, visual review |
| **Licences and brands** | Medium | High | `assets/SOURCES.md`; real brands kept out of the public repository, replaceable in one go; no real people |
| **Scope** compared with a studio of thousands | Certain | High | P0 only until 1.0; the [inventory](systems-inventory.md) is the guard rail |
| **No Unreal build in CI** | Certain | Low | Packaging and measurements scripted on the Mac, evidence versioned |

---

## 9. Steering

- [Dashboard](roadmap-status.md): current version and workstream status,
  updated whenever a task is done.
- [Systems inventory](systems-inventory.md): reviewed at the end of every
  version; any system with neither a version nor a workstream shows up as a
  gap.
- End of version: criterion and budget verified, build stored in
  `GTAL/Builds/`, evidence versioned, one-line summary to Nico.
- Structural decisions (version order, purchases, performance budget,
  visible image-quality loss): decision cards to Nico with a recommendation.
