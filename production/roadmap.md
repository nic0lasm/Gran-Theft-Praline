# Roadmap: Gran Theft Praline

*French version: [roadmap.fr.md](roadmap.fr.md)*
*Version 4, 2026-11-12: refocus. Replaces version 3 (the 0.4 → 1.0 order)
and pauses the [0.9 test-phase plan](test-phase-plan.md). Earlier versions
stay in the Git history.*
*Day-to-day tracking: [roadmap-status.md](roadmap-status.md).
Systems checklist: [systems-inventory.md](systems-inventory.md).
Concept: [game-concept.md](../design/gdd/game-concept.md).*

---

## 1. The goal

**Priority 1: a realistic GTA-like of central Lyon.** GTA's mechanics
(walk, steal a car, drive through traffic, fight, get wanted by the police
and lose them) and the intelligence of its non-player characters
(pedestrians who react, believable traffic, police who hunt you down), in a
Lyon street that looks like the real one.

The story comes **afterwards**. The direction is set: thefts of Lyon
specialities and getaways through the traboules. It will only be written
once priority 1 meets its bar. Meanwhile the map already plans for the
traboules (passages, courtyards, doors) so they can become escape routes.

## 2. Where we are, honestly

- **Latest public release: 0.3.0 "Behind the wheel"** (10 November 2026,
  macOS). You walk and drive on rue de la République, with an early look at
  pedestrians, police, the minimap, day, night and weather.
- **`next` branch** (pre-production): many systems exist in code
  (pedestrians and witnesses, traffic, police and wanted stars, bare-hands
  and pistol combat, ragdoll, till hold-ups, missions, minimap, a fast-food
  interior, day, night, weather). Most are **half finished** and none has
  been judged in actual play yet.
- **The image is far from the goal.** The first real photo / game capture
  board (candidate 17b, 12 November) scores 1 out of 5 on two views and 0 on
  the other three: black shadows, an empty map beyond 45 m, grey tiles
  instead of scanned surfaces, black shop windows and night, the player
  still a mannequin, no pedestrians or cars in the views. The real level is
  an **improved blockout**, not a game.
- **Why:** we optimised a prototype for an M1 laptop before knowing what it
  should look like, opened too many workstreams at once, and lacked art
  direction.

## 3. The target: the rue de la République slice

All effort goes into **300 to 400 m of street, from Bellecour to the
Jacobins.**

**It is done when:**

1. **10 minutes of play in the slice feel like GTA**: steal a car, drive
   through traffic that stops at lights and gets out of the way, get out
   among passers-by who live and react (flee, scream, call the police),
   fight, get wanted stars, the police hunt you and you lose them or get
   arrested, die and respawn. Proof: **a video of the 10 minutes**, unedited.
2. **The image holds up** on **5 real photo / game capture pairs** taken
   from the same viewpoints and redone for every candidate: Bellecour
   entrance, cinema façade, shop windows at eye level, the fountain on place
   de la République, night in front of the cinema.
3. **Stable on the reference machine**: PC package without crashes.

### At the core of priority 1

Driving, pedestrians, traffic, police, basic combat, and the look of the
street (scanned ground and façades, shop windows, lighting, night, a
realistic player).

### Frozen until the bar is met

Missions, audio (beyond what exists), menus, interiors, game saves, minimap,
key remapping, credits. The fast-food stays in maintenance (fixes only).
The neighbourhood test build (0.9) is postponed. Frozen systems only get
crash fixes.

## 4. How we work

| Rule | What it means |
| ---- | ---- |
| **PC first** | Image quality is tuned on a Windows PC (RTX 3070 Ti) with hardware Lumen and Nanite. A change lands on `next` when CI is green, the project compiles on the PC and a PC package runs 120 s without a crash. |
| **Mac later** | The MacBook Pro M1 Pro runs one nightly measurement on `next` as an early warning, with a low profile; it no longer blocks merges. The playable Mac port comes later. |
| **Free assets only** | Zero euros: Quixel Megascans, MetaHuman, City Sample, free Fab content, home-made. Every asset has its licence line in [assets/SOURCES.md](../assets/SOURCES.md). |
| **One candidate a day at most** | Each candidate comes with its 5 photo / capture pairs. A small gain is not presented as a success: we judge on the pairs and the video. |
| **Four active workstreams** | Street look; characters and NPCs; vehicles and police; machines (builds, packages, measurements). Everything else is paused. |

## 5. Timeline

Orders of magnitude, reviewed at every checkpoint, no promise beyond them.

| When | What |
| ---- | ---- |
| **End of November 2026** | The rue de la République slice meets its bar on PC |
| **Mid to late December 2026** | The north of the Presqu'île, Bellecour to Terreaux (~1.5 km²), at the same level of mechanics |
| **February 2027** | Mac port |
| **Around February 2027, with a wide margin** | A playable, good-looking neighbourhood on PC |
| Then | The story: thefts of Lyon specialities, getaways through the traboules |

Out of reach for this plan: all of Lyon, cinematics.

## 6. Next step (candidate 18)

- MetaHuman player instead of the mannequin.
- Scanned surfaces on the slice's road and pavements.
- Regenerated city: background buildings to the horizon, the cinema tower,
  the fountain, the stretch down to Bellecour, zinc roofs, façade relief.
- Ultra PC image profile with hardware Lumen.
- Second board of the 5 views and a 120 s PC package run.

## 7. Risks

| Risk | Mitigation |
| ---- | ---- |
| The image stays far off despite surfaces and the MetaHuman | The 5 pairs show the gap at every candidate; the biggest gap is fixed first |
| The PC (8 GB of video memory, tight disk) limits the image or the packages | One heavy job at a time, old packages purged, budgets measured |
| Half-finished systems spoil the 10 minutes | Frozen systems stay off in the slice; core systems are finished before others are opened |
| The Mac drifts while work happens on PC | Nightly early-warning measurement, port planned for February |

## 8. What has shipped

| Version | Content | Released |
| ---- | ---- | ---- |
| 0.1 Play in one click | Launch the game and walk on rue de la République | 9 Nov 2026 |
| 0.2 The dressed street | A faithful, dressed street, playable on an M1 laptop | 10 Nov 2026 |
| 0.3 Behind the wheel | Steal a car, cross the traffic to the Rhône quay | 10 Nov 2026 |

The next public release will ship when the slice meets its bar; its number
will be set then.
