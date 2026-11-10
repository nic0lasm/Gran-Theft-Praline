# Inventory of GTA / RDR open-world systems

*French version: [systems-inventory.fr.md](systems-inventory.fr.md). Roadmap: [roadmap.md](roadmap.md).*
*Review of 2026-11-10, after shipping 0.2. Updated at the end of every version.*

This document lists **every** system a player expects from a GTA- or Red
Dead Redemption-style open world, and states where each one stands in Gran
Theft Praline. It is a checklist: a system with neither a version nor a
workstream is a **gap**.

Legend: ✅ done · 🟡 in progress (a workstream is on it) · 📅 planned (a
version carries it, nobody works on it yet) · ❗ gap (missing from roadmap v2
or without a workstream, added by this review) · ⛔ out of scope.

Priority: P0 = needed for 1.0 (vertical slice), P1 = alpha, P2 = later; taken
from the [concept](../design/gdd/game-concept.md).

## 1. On foot

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Walk, run, sprint, jump, crouch (motion matching) | P0 | ✅ | 0.1 | Game Animation Sample |
| GTA-style third-person camera, first-person toggle (V) | P0 | ✅ | 0.1, 0.2 | 86° FOV, recentring, no camera inside walls |
| Shoulder swap, aim camera | P0 | 📅 | 0.7 | with combat |
| Vault and climb low obstacles | P0 | ❗ | 0.4 | GASP can do it; never enabled or tested on our low walls and bollards |
| Stamina | P0 | ❗ | 0.7 | missing from the roadmap v2 tasks |
| Physical reactions, ragdoll, stumble, shove | P0 | ❗ | 0.4 | no task; needed by pedestrians, traffic and combat |
| Contextual interactions (E: doors, sit, pick up) | P0 | 🟡 | 0.2, 0.8 | `UGTPInteractionComponent` base done; doors and containers remain |
| Swimming, falling into the Rhône or Saône | P1 (falling: P0) | ❗ | 0.3 | the Rhône quay is in the map since today: falling into the water must at least be handled (respawn on the quay) |
| Player appearance (clothes, hair) | P1 | 📅 | 1.x | clothes shop in 0.8 |

## 2. Vehicles

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Chaos physics driving (city car, saloon, van) | P0 | ✅ | 0.3 | Clio V, 208 II, Kangoo II with LODs |
| Get in, get out, break the window, pull the driver out | P0 | ✅ | 0.3 | without dedicated animations (fixed durations) |
| Zone damage, windows, wreck | P0 | ✅ | 0.3 | fire, explosion, flat tyres: 0.7 |
| Chase camera, interior view, look back | P0 | ✅ | 0.3 | cinematic and bonnet cameras: 0.10 |
| Synthesised engine sound, driving panel | P0 | 🟡 | 0.3 | branch `feat/vehicle-hud-sound` |
| Headlights, brake lights, indicators, horn | P0 | ❗ | 0.6 | essential at night; nothing planned |
| Get-in / get-out animations | P0 | ❗ | 0.5 | no animation source identified (see §9) |
| Two-wheelers: scooter, motorbike, Vélo'v | P0 / P1 | 📅 | 0.12 | Vélo'v decided by Nico |
| TCL buses (scenery then drivable), metro and funicular as scenery | P2 → P1 | 📅 | 0.9, 1.x | TCL fleet decided by Nico |
| Car radio | P1 | 📅 | 1.x | royalty-free or original music |
| Garages, customisation | P1 | 📅 | 1.x | |
| Boats on the rivers | P2 | ⛔ | | |

## 3. Living world

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Traffic on the real lanes (112 lanes, NPC drivers) | P0 | 🟡 | 0.3 | branch `feat/traffic-drivers`; NPC GPU cost being reduced |
| Traffic lights, right of way, pedestrian crossings | P0 | 📅 | 0.4 | |
| Traffic reactions (horn, avoidance, fleeing, accidents) | P0 | 📅 | 0.4 | |
| Mass + StateTree pedestrians on the pavements | P0 | 🟡 | 0.4 | Pedestrians workstream opened 2026-11-10 |
| Pedestrian reactions (flee, fear, witnesses, looking) | P0 | 🟡 | 0.4 | with the Pedestrians workstream |
| Car-pedestrian impact, running a pedestrian over | P0 | ❗ | 0.4 | depends on ragdoll |
| Population variety (faces, outfits, ages) | P0 | ❗ | 0.4, 0.15 | MetaHuman is costly on M1: LOD strategy and number of models to decide |
| NPC speech (barks, insults, screams) | P0 | ❗ | 0.10 | French voices to record or synthesise |
| Greet, antagonise an NPC (RDR style) | P1 | ❗ | 1.x | |
| Ambient events (arguments, thefts, deliveries) | P1 | 📅 | 1.x | |
| Wildlife (pigeons, dogs) | P2 | 📅 | 1.x | pigeons are worth having early for realism: consider in set dressing |
| Day, night, sun path over Lyon | P0 | 🟡 | 0.6 | Day/night + weather workstream opened 2026-11-10 |
| Rain, wet ground, fog | P0 | 🟡 | 0.6 | same workstream |
| Night street lighting (street lamps, shop windows, windows) | P0 | ❗ | 0.6 | cost of lights on M1 to budget |

## 4. Combat

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Melee (punches, dodge, takedown) | P0 | 📅 | 0.7 | no workstream |
| Firearms (pistol first), aiming, recoil, reload | P0 | 📅 | 0.7, 0.11 | no workstream |
| Gamepad aim assist | P0 | 📅 | 0.7 | |
| Cover, blind fire | P0 | 📅 | 0.7 | |
| Weapon wheel, limited inventory | P0 | 📅 | 0.11 | |
| Health, armour, death, hospital respawn | P0 | 📅 | 0.7 | the pulled-out NPC has no health yet |
| Localised hit reactions | P0 | 📅 | 0.7 | depends on ragdoll (0.4) |
| Visual effects: impacts, sparks, blood, smoke, fire, explosions | P0 | ❗ | 0.7 | Niagara; no workstream, no GPU budget |
| Breakable props (bollards, signs, street lamps, shop windows) | P0 | ❗ | 0.4 | a GTA signature when driving; all 284 imported meshes are static |
| Throwables | P1 | 📅 | 1.x | |

## 5. Police and wanted level

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Crimes and witnesses who report them | P0 | 🟡 | 0.5 | Police workstream opened 2026-11-10 |
| Stars 1 to 3, search area, line of sight | P0 | 🟡 | 0.5, 0.11 | |
| Patrols, car chases | P0 | 🟡 | 0.5 | reuses the 0.3 traffic |
| Arrest, penalty, quick restart | P0 | 🟡 | 0.5 | |
| Police who shoot (level 2-3), roadblocks | P0 | 📅 | 0.7, 0.11 | depends on combat |
| Sirens, flashing lights | P0 | ❗ | 0.5 | sound and light, nothing planned |
| Helicopter, levels 4-5, special units | P1 | 📅 | 1.x | |
| Fictional French police (vehicles, uniforms, fictional insignia) | P0 | 📅 | 0.5, 0.15 | fictional police Mégane or 308 to model |

## 6. City and places

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Faithful Rue de la République (LiDAR, 215 shop signs, facade kit) | P0 | ✅ | 0.2 | |
| Rhône quay, Pont Lafayette | P0 | 🟡 | 0.3 | `0.3-city` build at 65 % of frames < 33 ms |
| Realistic set dressing (small details against photos) | P0 | 🟡 | ongoing | Street realism workstream opened 2026-11-10 |
| Plane trees with foliage | P0 | 🟡 | 0.4 | Megaplants maple to be measured first |
| Whole Bellecour → Terreaux district | P0 | 📅 | 0.9 | |
| World streaming (World Partition, HLOD, data layers) | P0 | 📅 | 0.8, 0.9 | |
| Landmarks (Hôtel de Ville, Opéra, Bellecour) | P0 | 📅 | 0.13 | |
| River water (surface, lower quays) | P0 | ❗ | 0.3 | native Water plugin without simulation; no workstream |
| Shops without loading screens, hold-ups | P0 | 📅 | 0.8 | |
| Halls, stairwells, courtyards, roofs, car parks | P0 (selection) | 📅 | 0.8, 0.14 | |
| Safe houses | P0 | 📅 | 0.8 | |

## 7. Interface, menus and services

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Title screen, pause menu, graphics profiles | P0 | ✅ | 0.1, 0.2 | |
| HUD: centre dot, prompts, subtitles, driving panel | P0 | ✅ | 0.2, 0.3 | |
| Minimap | P0 | ❗ | 0.5 | planned for 0.12 only; essential as soon as chases exist |
| Full-screen map, real street names, GPS | P0 | 📅 | 0.10 | `lanes.csv` has empty names: `roads.geojson` to regenerate (City v2 workstream) |
| Money, economy | P0 | 📅 | 0.8 | |
| Autosave and safe-house saves | P0 | 📅 | 0.8 | |
| Saved settings (profile, mouse, volumes, keys) | P0 | ❗ | 0.5 | today everything is lost on exit |
| Key remapping, sensitivity, inversion | P0 | 📅 | 0.10 | |
| Accessibility (subtitles, aim assist, colours) | P0 | 📅 | 0.10 | |
| Game text in French and English | P0 | ❗ | 0.4 | string tables now, otherwise a costly retrofit |
| Phone (contacts, messages, camera) | P1 | 📅 | 1.x | |
| Photo mode | P2 | 📅 | 1.x | |

## 8. Missions and activities

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Mission framework (objectives, checkpoints, failure, retry) | P0 | ❗ | 0.12 | no story yet, but the tool is needed for activities, tests and the future story |
| Free activities from the concept (6) | P0 | 📅 | 0.12 | |
| Stats, skills | P1 | 📅 | 1.x | |
| Story, characters, dialogue | P1 | ⛔ | 1.x | written later with Nico |
| Online play | | ⛔ | | outside the project |

## 9. Production and technology (invisible but blocking)

| System | Prio | State | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| One-click packaging, GitHub release | P0 | ✅ | 0.1 | |
| Per-system performance budget | P0 | ❗ | 0.3 | see [roadmap §3](roadmap.md); only the total is tracked today |
| M1 optimisation (culling, shadows, Lumen, PSO) | P0 | 🟡 | ongoing | Optimisation workstream opened 2026-11-10 |
| Automatic performance test on every build | P0 | 🟡 | 0.3 | Shipping soak logs exist; to be made blocking |
| Animation source (combat, vehicles, NPCs, cover) | P0 | ❗ | 0.4 | GASP covers locomotion only; to survey (free Epic content, Fab, video capture) |
| Audio: mixing, MetaSounds, surface footsteps, sirens, impacts | P0 | ❗ | ongoing | pushed to 0.18 in v2; sound is half of the game feel |
| Distributing builds above 2 GiB | P0 | ❗ | 0.3 | GitHub caps each release file at 2 GiB; 0.2 weighs 1.36 GB |
| Launching on another Mac (Gatekeeper, ad hoc signing) | P1 | ❗ | 0.3 | to document in the README |
| Backing up sources outside Git (masters, GTAL/assets) | P0 | 🟡 | | in Nico's to-do list |
| Gamepad (haptics, glyphs) | P0 | 🟡 | 0.10 | gamepad works, glyphs missing |

## 10. Summary

- **Done**: on-foot base, driving, faithful street, packaging. This is the
  right foundation: GTA and RDR rest first on the feel on foot and at the
  wheel.
- **In progress today**: traffic, pedestrians, police, day/night, realism,
  optimisation. Roadmap v2 placed them in 0.6, 0.8, 0.9 and 0.13; the new
  roadmap moves them up to 0.4-0.6.
- **Most serious gaps** (without them the game does not feel like a GTA, even
  with everything else):
  1. **Physical reactions and ragdoll**, car-pedestrian impacts.
  2. **Animation source** beyond locomotion.
  3. **Audio** as an ongoing workstream, not an end-of-project one.
  4. **Visual effects and breakable props.**
  5. **Per-system performance budget**: every workstream adds cost to an
     already full GPU.
  6. **Night lighting**: headlights, street lamps, shop windows.
  7. **Minimap, saved settings, FR/EN text** (small, but costly to retrofit).
  8. **River water and falling into it**, now that the quay is in the map.
  9. **Distribution** above 2 GiB per file.
  10. **Mission framework**, even without a story.
