# Plan to the test phase: version 0.9

*French version: [test-phase-plan.fr.md](test-phase-plan.fr.md). Roadmap: [roadmap.md](roadmap.md). Systems: [systems-inventory.md](systems-inventory.md).*
*Review of 2026-11-11, during the performance worksite (`next` = 3dcf33b). Complements roadmap v3: it sets what 0.9 must hold so it can be tested, what is missing, and the order of work.*

---

## 1. The objective, restated

| Point | What is decided | Source |
| ---- | ---- | ---- |
| The game | Original open-world crime game, feature and quality parity with GTA VI, GTA V and Red Dead Redemption 2, on a faithful Lyon; never their content | [concept](../design/gdd/game-concept.md) |
| The image | Photorealistic; the M1 profile derives from the Ultra profile, any visible downgrade goes through Nico | roadmap §1, ADR-0001 |
| The area | North Presqu'île, Bellecour → Terreaux, Saône and Rhône quays (~1.5 km²) | concept |
| The living city | 100+ pedestrians and 40+ perceived vehicles, dressed street, leafy plane trees, through open-world techniques, never by degrading the image | performance worksite |
| Reference machine | MacBook Pro M1 Pro 16 GB, m1 profile, **capped at 40 fps (steady 25.0 ms)**, no frame > 100 ms (decision of 2026-11-11, replaces the 30 fps target of ADR-0002) | performance worksite |
| Second machine | Windows PC (Ryzen 9 3900, RTX 3070 Ti): Windows build (~81 fps at Epic 1080p measured), compile checks, compute | performance worksite |
| Frame | Non-commercial, public MIT repository, free assets first, no real people, story written later with Nico | concept, project rules |
| Order | 0.4 living street, 0.5 police, 0.6 day/night, 0.7 combat, 0.8 shop, money, save, 0.9 full quarter, then 0.10 → 1.0 | roadmap v3 |

**Decided by Nico on 2026-11-11 (card):** 0.9 is
the **quarter test build**. The whole quarter can be crossed without
loading screens, with the systems of 0.4 to 0.8, plus the minimum to play a
real session (find your way, have a goal, set your controls). It is tested,
then 0.10 and later resume the rest of P0 and the final art.

---

## 2. Exit criteria: 0.9 is ready for the test phase when…

### A. The world
- [ ] Bellecour → Terreaux, quays and bridges of the area, generated in one command (city v2), crossed on foot and by car **without loading screens or streaming hitches** (ADR-0010 cells, HLOD).
- [ ] Memory ≤ 9 GB on the M1 without swap, 1.5 GB texture pool held, over a full route.
- [ ] Fully dressed street (details, shop windows, leafy plane trees), parked cars, far pedestrians (impostors) in place.
- [ ] One showcase street at photorealistic quality (rue de la République, Ultra profile) compared with photos (decided 2026-11-11).

### B. Systems (0.4 → 0.8)
- [ ] Living city: 100+ pedestrians and 40+ perceived vehicles, traffic lights, reactions, ragdoll, car-pedestrian impact, breakable street furniture.
- [ ] Police 1-2 stars, witnesses, sirens and lightbars, chase, arrest, minimap.
- [ ] Day, night, rain; headlights, brake lights, indicators; street lamps and shop windows at night.
- [ ] Combat: unarmed and pistol, cover, health, death and respawn; police shoot at level 2.
- [ ] One shop without loading (buy, rob), money, one safehouse, **autosave and safehouse save**, loading a game.

### C. Enough to play a session (pulled forward from 0.10 and 0.12)
- [ ] Full-screen map with **real street names** and a destination followed by a simple GPS.
- [ ] **Minimal mission framework** (objective, waypoint, failure, retry) and **3 activities** built with it: rob the shop, steal a car to order, lose the police.
- [ ] Controls: complete keyboard-mouse and gamepad, **key remapping**, sensitivity, gamepad glyphs; subtitles and texts in FR and EN.
- [ ] Menus: title screen (new game, continue), pause, settings, **credits**, version shown.

### D. Quality
- [ ] **M1**: 40 fps held (≥ 99 % of frames at 25.0 ms ± 1), no frame > 100 ms, over the full route on foot and by car, by day, night and rain, with streaming.
- [ ] **Windows**: 60 fps at Epic 1080p on the RTX 3070 Ti, no frame > 100 ms; no more default material (280 materials missing the Nanite flag fixed).
- [ ] One hour of free play without crash or blocker, on each machine.
- [ ] Launch-to-play time ≤ 60 s on the M1 (to be measured, no value today).

### E. Test release
- [ ] One-click macOS and Windows builds, archives ≤ 2 GiB per file, checksums, opening instructions (Gatekeeper, SmartScreen).
- [ ] **Test build that writes logs** (Shipping writes none) and keeps crash reports locally, with a key that exports frame times and position.
- [ ] Test protocol, scenarios, feedback form and bug template, for Nico and a few close people (decided 2026-11-11).
- [ ] Release notes and player guide in FR and EN, known issues.
- [ ] Complete licences and credits (IGN, Grand Lyon, OpenStreetMap ODbL, Fab and Megascans, Poly Haven, sounds, fonts, Unreal Engine); real-brands rule for a privately shared build (to be written before 0.9).
- [ ] Off-Git backup up to date (Unreal content outside the repository, masters).

---

## 3. Gaps per version

Main finding: **the code of versions 0.4 to 0.6 is already on `next`**
(0.3 previews, then the performance worksite), but none of these versions
has shipped, and the systems inventory no longer reflects it. Conversely,
**nothing exists yet for 0.7, 0.8 and 0.9**, and 0.9 carries the heaviest
risk (generation, import, streaming, memory).

### 0.4 The living street
| Done (on `next`) | Missing | Forgotten by the plan |
| ---- | ---- | ---- |
| Pedestrians, reactions, fear and flight; traffic lights; ragdoll and player reactions; breakable furniture; FR/EN string tables; car entry poses; tier C held (50 pedestrians, 25 driven + 24 far cars) | Far pedestrian (failed rig), parked cars (materials), leafy plane trees (d827191 to measure), street details 12, vaulting low obstacles, population variety (number of models decided), 100+ pedestrian goal | Measured version gate (tier C at 40 fps); Windows build of the version; `ProjectVersion` still 0.2.0 |

### 0.5 The police arrive
| Done | Missing | Forgotten by the plan |
| ---- | ---- | ---- |
| Stars, witnesses, chases, sirens, minimap and full-screen map, saved settings, respawn hitch fixed (09429f1) | Fictional police car model, arrest and penalty checked on video, 3-min chase measured, roadblocks (unmerged branch, planned for 0.11) | Audio cost never benched (audio pass) |

### 0.6 Day and night
| Done | Missing | Forgotten by the plan |
| ---- | ---- | ---- |
| Sun path, light-function cloud shadows, lighter night clouds, budgeted night lights, 6 weathers, headlights (`-LitCars`) | Ultra captures against photos, pedestrians adapting to rain (umbrellas, pace), cost of the 24 far cars at night and in rain (windows) | — |

### 0.7 Armed
| Done | Missing | Forgotten by the plan |
| ---- | ---- | ---- |
| Ragdoll, light hit reactions | All of combat; gameplay framework ADR (the roadmap says "ADR-0008", a number already taken: it will be **ADR-0011**); **animation source** (strikes, aiming, cover); Niagara effects and their budget | GPU budget for weapon effects; Combat worksite never opened |

### 0.8 Living in the city
| Done | Missing | Forgotten by the plan |
| ---- | ---- | ---- |
| Interaction base (`UGTPInteractionComponent`) | Interior without loading, shopkeeper, purchase, robbery, money, safehouse, **SaveGame** (no code); interiors ADR (the roadmap says "ADR-0005", taken by pedestrians: it will be **ADR-0012**) | Save versioning; robbery depends on combat (0.7) |

### 0.9 The whole quarter
| Done | Missing | Forgotten by the plan |
| ---- | ---- | ---- |
| ADR-0010 (cells), rue de la République, Rhône quay and Lafayette bridge, geo cache of the quarter, quarter streaming being measured | Generation of the whole quarter, reliable import (4 failures), HLOD, memory ≤ 9 GB, TCL buses as scenery | Map, GPS, missions, remapping (planned 0.10-0.12, required to test); QA, logs, credits, test protocol |

---

## 4. What the plan had forgotten

| Topic | State today | To do | Version | Proposed owner |
| ---- | ---- | ---- | ---- | ---- |
| **Performance gate on every merge** | Manual benches, unwritten thresholds | Written gate (§5.2), one-command bench that fails by itself | now | Optimisation |
| **Windows build** | Hand-made package, 280 materials missing the Nanite flag | Package every version, m1 + Ultra bench, D3D12 PSO cache | every version | Windows PC |
| **Off-Git content sync** (31 GB, two machines) | One copy from the Mac on 11/11 | One source per asset, hash manifest, archive of differences only | 0.4 | Windows PC + Import |
| **Game save** | None (only settings are saved) | Versioned SaveGame, autosave, safehouse, load test | 0.8 | Shops and save (*to open*) |
| **Controls** | Basic keyboard-mouse and gamepad | Remapping, sensitivity, glyphs, rumble | 0.9 (from 0.10) | Interface (Minimap thread widened) |
| **Menus** | Title screen, pause, settings | Continue, load, credits, version | 0.8-0.9 | Interface |
| **Audio** | Code and mix on `next`, never measured | Benched audio pass, combat and shop sounds | 0.5, 0.7 | Audio |
| **Game loop** | None | Minimal mission framework and 3 activities | 0.9 (from 0.12) | Missions and activities (*to open*) |
| **Map and GPS** | Full-screen map without street names | Regenerated `roads.geojson`, destination, GPS | 0.9 (from 0.10) | City v2 + Interface |
| **QA and test protocol** | Tests outside Unreal, scripted playtests from 0.1 | Automatic smoke test of every build, one-hour session, scenarios, bug template | 0.6 → 0.9 | QA and release (*to open*) |
| **Logs and crashes** | Shipping writes no log or CSV | Test configuration with logs, crash reports kept locally, export key | 0.7 | QA and release |
| **Version number** | `ProjectVersion=0.2.0` | Set by the package script from the tag | 0.4 | QA and release |
| **Licences and credits** | Credits in the README only | Credits screen, third-party licence file, brands rule | 0.8 | QA and release |
| **Docs** | Roadmap, ADRs, 0.3 notes | Player guide FR/EN, protocol, inventory up to date | 0.9 | README + QA |
| **ADR-0002** | Says 30 fps targeted | Amended: M1 capped at 40 fps, "no frame > 100 ms" rule | now | Optimisation |
| **Off-Git backup** | Block ready, not yet pasted | Copy to the SSD before the test | before 0.9 | Backup (Nico pastes the block) |

---

## 5. Adapted plan

### 5.1 Order and dependencies

```
Step 0  end of perf worksite ──┐
                               ├─► 0.4 ─► 0.5 ─► 0.6   (release train: mostly integration)
Cloud preparation ─────────────┤
  ADR-0011 combat, animations  ├─► 0.7 combat ─► 0.8 shop, money, save (robbery depends on combat)
  ADR-0012 interiors, SaveGame ┘
City v2 + import + PC ──────────────────────────────────► 0.9 quarter (critical path, starts now)
Interface, missions, QA ────────────────────────────────► 0.9 test build ─► test phase ─► 0.10…
```

1. **Step 0, now → worksite verdict.** Finish exit criteria 3 to 7 of the
   worksite (per-system budget measured A/B, one-command non-regression
   bench, crowd impostors, streaming, memory, content rules). No new
   feature on `next`.
2. **Release train 0.4 → 0.6.** The code is there: each version ships as
   soon as its gaps (§3) are closed and its gate (§5.2) passes. Target
   pace: a few days per version, not weeks.
3. **In parallel, in the cloud, from the verdict on**: ADR-0011 and combat
   code tested outside Unreal, animation survey (free first); ADR-0012,
   SaveGame and economy; mission framework; remapping and menus. None of
   it uses the Mac before integration.
4. **Critical path, from today**: generation of the whole quarter (city v2,
   geo cache already there), import repair (diagnosis outside Unreal
   first), HLOD and LOD computed on the PC. It is the longest and riskiest
   item; it must not wait for 0.8.
5. **0.7 then 0.8**, then **0.9** which brings together the quarter, the
   systems and the game loop, and ends with a **0.9-rc** run through the
   test protocol by the threads before it reaches Nico.

### 5.2 Quality gates

**On every merge to `next`** (no exception):
1. Green CI on the exact commit (Provenance check, tests outside Unreal).
2. Compile check on the PC (MSVC, Win64 editor and game) of the same commit.
3. M1, m1 profile capped at 40 fps, tier C world, 4 scenes of 120 s (fixed,
   deterministic route, night, rain): ≥ 99 % of frames at 25.0 ms ± 1 and
   **no frame > 100 ms**; GPU p95 ≤ 24 ms (headroom under the cap).
4. If the branch adds a system: on/off A/B against its line of the
   per-system budget.

The cap makes frame time insensitive to the ~2 ms variance of tier C; GPU
p95 is the headroom gauge.

**On every version**, in addition: 600 s per scene, a 30-min free session
without crash (60 min for 0.9), Windows package and bench (m1 and Ultra, no
frame > 100 ms), memory measurement, proof video, FR/EN notes, version
number set, systems inventory updated.

### 5.3 Mac, PC and cloud in parallel

| Machine | Role | No longer does |
| ---- | ---- | ---- |
| **MacBook Pro M1 Pro** | Reference bench (gate of every merge), macOS package, version captures | Asset import, long computations (move to the PC) |
| **Windows PC** | Compile check of every branch; Windows package and bench; **headless import** and computations (LOD, HLOD, impostors, PBR textures, Blender); fine GPU profiling; source of imported content, sent to the Mac as a difference archive | — |
| **Cloud (threads)** | Code, tests outside Unreal, geo data, docs; scripts dry-run before any machine slot | — |

Rule: the Mac only receives what already compiled on the PC and passed CI;
the PC is never idle (import, computations or packaging queued).

### 5.4 Who owns what

| Thread | Owns until 0.9 |
| ---- | ---- |
| Optimisation (lead) | Merge gate, non-regression bench, per-system budget, amended ADR-0002, memory |
| Windows PC | Compile checks, Windows package and bench, content sync, computations |
| Traffic | Lights, driving, TCL buses as scenery, cost of far cars at night |
| Pedestrians | 100+ perceived, far pedestrian, variety, rain |
| Police | Complete 0.5, shooting at level 2 with combat |
| Ragdoll | Physical reactions, vaulting, **non-locomotion animations** |
| Day/night | Ultra captures, car lights, night and rain cost |
| Audio | Benched pass, combat and shop sounds |
| City v2 | Quarter generation, cells and HLOD, `roads.geojson` |
| Realism | Details, shop windows, photorealistic showcase street |
| Trees, Import | Leafy plane trees; reliable import on the PC |
| Minimap → **Interface** | Map and GPS, remapping, glyphs, menus, credits |
| Backup | Off-Git copy before 0.9 |
| README | Player guide, release notes |
| *To open*: **Combat and weapons** | ADR-0011, melee, pistol, cover, health, effects |
| *To open*: **Shops, economy, game save** | ADR-0012, interior, robbery, money, safehouse, SaveGame |
| *To open*: **Missions and activities** | Minimal framework, 3 activities |
| *To open*: **QA and release** | Test build with logs, smoke test, protocol, credits and licences, version number, archives |

### 5.5 What moves after the test phase

Six-weapon arsenal and weapon wheel, police level 3 and roadblocks, 5 shop
types and 3 safehouses, two-wheelers, 6 activities, full accessibility, NPC
speech, final art of the quarter (0.13 → 0.16), polish (0.17).

---

## 6. Questions asked to Nico (all settled)

1. ~~What "test phase" after 0.9 means~~: **decided**, quarter test build (§1).
2. ~~Who tests~~: **decided**, Nico and a few close people, on Mac and Windows, builds shared privately. Hence: builds for both systems, logs and crash reports, feedback form, credits screen (§2 E).
3. ~~The visual level~~: **decided**, work quality everywhere, plus rue de la République at photorealistic quality (§2 A).

All three were answered by Nico on 2026-11-11, each on the recommended option.

---

## 7. Risks specific to 0.9

| Risk | Mitigation |
| ---- | ---- |
| Full quarter too heavy for 16 GB or for the M1 GPU | Small cells, HLOD at generation, memory measured at each step, 40 fps gate over the full route |
| Import fails again | Diagnosis outside Unreal, import on the PC, one source per asset |
| Combat animations not found | Survey before any combat code; hand-keyed poses (car-pose method) as fallback |
| Two machines drift apart | Hash manifest checked before each package |
| Real shop signs in a privately shared build | Rule written before 0.9; brand-free variant ready |
