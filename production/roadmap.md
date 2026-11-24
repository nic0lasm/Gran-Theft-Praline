# Roadmap: Gran Theft Praline

*French version: [roadmap.fr.md](roadmap.fr.md)*
*Version 6, 2026-11-24: review after 18 gates in two days. The goal and the
milestones do not change. What changes: the ground we judge (one fixed
getaway circuit), the judge (a 10-point grid read on the video), the gate
(a short one and a full one) and the organisation (two content workstreams
instead of seven). Replaces version 5 (19 November). The
[0.9 test-phase plan](test-phase-plan.md) stays paused. Earlier versions
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

**Order of priorities, set on 13 November:** non-player characters first,
then feel (driving, shooting, stealing a car), then the image. The first
moment that must make you say "this is GTA": **steal a car and flee the
police.** Since 23 November the image moves forward alongside the game, on
what the camera sees during that moment.

The story comes **afterwards**. The direction is set: thefts of Lyon
specialities and getaways through the traboules. It will only be written
once priority 1 meets its bar.

## 2. Where we are, honestly (24 November)

`next` and `main` are on candidate 76. Everything below comes from the gate
logs of the packaged game on the reference PC and from the frames of the
videos of candidates 76 and 77; what has not been measured is written as
such.

### Proven in the packaged game, video watched

- **The theft.** The player takes a moving car in 6 seconds (candidate 76).
- **The police are recognisable** since candidate 75: a white car with a
  blue band, the warning "POLICE ! NE BOUGEZ PLUS !" on screen, an arrest
  at 63 seconds in candidate 76.
- **The getaway peaks at 89 km/h** (candidate 76). In the launch test the
  player's car goes from 0 to 100 km/h in 6.6 seconds.
- **Stars, minimap and speedometer** are on screen.
- **Traffic around**: at least 8 moving cars within 150 m during the
  getaway (12.4 in candidate 77).
- **Stable**: no crash and no error line over the 12 runs of gate 76; game
  logic holds in about 11 ms.
- **The dressed square looks like a street** (candidate 77, 90 seconds):
  balconied façades, shop windows, passers-by running away.

### Not there

- **The getaway is slow on average**: 35 km/h in candidate 76, 25 in 77.
  It is driven by an autopilot that gets stuck: over the four minutes
  filmed, the car was put back on a lane 4 times in 76 and 8 times in 77.
- **The getaway leaves the dressed street within seconds.** At 89 km/h the
  300 to 400 dressed metres are crossed in 16 seconds. The rest plays out
  between backdrop volumes and on a bare quay.
- **The distance is empty**: bare ground, blocks without façades, a blue
  band under the horizon (candidate 76, 60 seconds). A "distance" batch is
  written; it has not been seen in the game yet.
- **The camera** ends up on the car's roof, inside walls (a black frame at
  110 seconds in 77), or behind a street lamp that fills the screen.
- **Characters with shaved heads**, bodies under cars.
- **Police units drive at 17 to 21 km/h on average**, for 25 expected.
- **Nobody has played.** No hands-on session is on record since 0.3
  (10 November).
- **Not re-read for this review**: traffic lights, unarmed combat, death
  and respawn, losing the police.
- **Image**: no second board of the 5 pairs since 12 November.

### Why we are here

From 22 November 15:43 to 24 November 11:30 the PC ran 18 gates, 24 hours
of compute. The PC is no longer what is missing: it is what the gates
judge, and the surface we work on.

1. **A gate takes 81 minutes on average.** Gate 76: 23 minutes of
   compiling, 10 of packaging, 41 for 12 runs, then the video. The
   automated run is played five times in it (three runs, two videos). Two
   runs, 12 minutes, measure a second traffic system that is not in the
   default game.
2. **The numeric criteria and "it looks like GTA" are two different
   things.** Of the last 13 gates, six failed on a number: five in a row
   (65 to 69) on error lines, three of them on the same uncooked paint
   shader, and one (74) on 7.6 cars instead of 8. Of the seven that met
   every criterion, two (72 and 77) were turned down on watching the video:
   a New York taxi and exhausted video memory; 14 façade modules removed by
   a preparation step. The only real judge is a look at the video, and it
   comes after 81 minutes.
3. **Ground without an edge.** The getaway is free over the map's
   235 lanes, while 300 to 400 m are dressed. Any wall, post or pit in the
   whole centre can jam the autopilot or give an empty frame. Three
   workstreams each wrote their own remedy (learned blocked lanes, police
   steering round scenery, furniture taken off the lanes, covered pits,
   flipped façades); one of them punched holes in the map in 77.
4. **The map is a binary file outside Git**, changed before each gate by
   steps from several workstreams. The state of one gate depended on the
   one before.
5. **Too many hands for one bottleneck.** Seven content workstreams
   (police, traffic, pedestrians, combat, dressing, image, assets) feed
   one queue and one PC. Since 22 November, 37 of the 93 commits on `main`
   only change the gate specification, and the text one gate produces grew
   from 0.5 MB (70) to 0.9 MB (77).
6. **Nothing compiles the C++ before the PC.** Three candidates (73, 78,
   79) were dropped or redone over a compile error.

## 3. What changes

### 3.1 One getaway circuit, and nothing else until milestone A

The getaway of milestone A plays on **one fixed loop of 1.5 to 2.5 km**,
chosen once, outside the game, on the lane file:

- it starts where the theft happens and goes through the dressed street
  and square;
- at least 70 % main roads (carriageway of 7 m or more), no dead end;
- its lanes are checked clear of any fixed scenery before the gate, on the
  data, not in the game;
- it is committed to Git as a list of lanes.

The video's autopilot follows that loop: it no longer draws a lane at
random and is never put back on one. The police chase freely.

All scenery and image work concentrates on **the corridor**: what the
camera sees from the loop. Outside the corridor a defect is written down,
not fixed.

### 3.2 A 10-point grid, read on the video

The gate films the getaway on the circuit. One frame is taken every
10 seconds over the first 3 minutes. Each point is met or not.

| # | What must be seen | Judged on |
| ---- | ---- | ---- |
| 1 | **The theft**: the player takes a moving car in under 10 s | log, frame |
| 2 | **It moves**: at least 50 km/h on average over the first minute of the getaway, a 90 km/h peak | log |
| 3 | **Unaided**: the car is never put back on a lane, no stop longer than 3 s against scenery | log |
| 4 | **The police are there**: a recognisable police car in frame during the getaway, its units at 25 km/h on average or more | frames, log |
| 5 | **Company**: at least 8 moving cars within 150 m | log |
| 6 | **A readable ending**: arrest or police lost, announced on screen, before 120 s | log, frame |
| 7 | **The camera holds**: never inside a wall or a car, no black frame, no foreground object over more than a quarter of the screen | frames |
| 8 | **No emptiness**: no frame shows bare ground, an empty horizon or blocks without façades | frames |
| 9 | **Whole characters**: no default shaved head, no frozen pose, no body under the car | frames |
| 10 | **Smooth and clean**: 95 % of frames under 33 ms during the filmed run, no engine text on screen | log, frames |

Candidate 76 meets 3 out of 10 (points 1, 5 and 6). Milestone A is 10 out
of 10, then the hands-on session.

A candidate states in one sentence **what the video will show
differently**, and which points it targets. A candidate that changes
nothing on the grid waits for the next one.

### 3.3 Two gates instead of one

| | Short gate | Full gate |
| ---- | ---- | ---- |
| When | every candidate | once a day, on the last head that passed the short gate; before any public release |
| Content | compile, package, default run, automated run filmed on the circuit (the measured run **is** the video) | today's runs, without those of the second traffic system |
| Duration | about 40 min (estimate, to be measured) | about 70 min |
| Moves | `next` | `main` |
| Blocks on | crash, error line, frame time, automated run under 6 of 7, grid going backwards | the same, plus the breakdown measurements |

- **A numeric threshold missed by less than 10 % does not fail the gate on
  its own**: the grid decides (gate 74: 7.6 for 8).
- **A branch is compiled on the PC before it enters a candidate**, as a
  background job, not at the gate.
- **A gate summary fits on one page**: the grid, six numbers, the frames.
  The full report stays on disk for searches.

### 3.4 The map is replayed

A candidate's map is **a frozen base map** (identified by its checksum)
**plus steps replayed in order**, all in Git. Never the result of the
previous gate. A step that removes actors carries its check (number of
façade modules unchanged). The base is only refrozen after a passed gate
and a watched video.

### 3.5 Two content workstreams instead of seven

| Workstream | What it owns |
| ---- | ---- |
| **The game** | The whole sequence: theft, player driving, traffic, police, chase camera, passers-by on the circuit. A getaway bug has one owner. |
| **The scenery** | The map and its steps, the dressing of the corridor, materials, the distance, free assets. |
| **The machines** | The PC queue, both gates, the grid, the videos. |

### 3.6 What stops until milestone A

- The runs and the work on the second traffic system (MassTraffic): the
  code stays, the decision is taken up again at milestone B
  ([ADR-0015](../docs/adr/adr-0015-mass-crowd-and-traffic.md)).
- Any scenery or lane fix outside the corridor.
- Probes and readings that explain a defect the grid does not see.
- Weapons and combat, apart from the camera.

## 4. The milestones

The ground of the slice does not change: **300 to 400 m of street, from
Bellecour to Jacobins**, on PC. The getaway circuit runs through it and
beyond.

### Milestone A: steal a car and flee the police (around 4 December 2026)

Three steps, each judged on the gate video:

| Step | Target | Grid points |
| ---- | ---- | ---- |
| **A1. The circuit holds**: the loop is chosen, the autopilot follows it unaided, the police follow | 27 November | 1 to 6 |
| **A2. What the camera sees**: camera, distance, dressed corridor, characters | 1 December | 7 to 9 |
| **A3. The video**: 3 unedited minutes, taken off the PC, then the hands-on session | 4 December | 10, then the player |

The milestone is met when **someone plays it hands-on** and says it feels
like GTA. A five-minute session earlier, on the candidate of the day, is
welcome: it will tell what the autopilot does not.

### Milestone B: the 10 minutes (re-dated at milestone A; order of magnitude, end of December 2026)

Traffic that stops at lights and gets out of the way, passers-by who live
and react (flee, raise their hands, call the police), combat, stars,
arrest, death and respawn. The grid extends to those behaviours. Proof:
**a video of the 10 minutes**, unedited.

### Milestone C: the street holds up (end of January 2027)

The **5 real photo / game capture pairs** from the same viewpoints:
Bellecour entrance, cinema façade, shop windows at eye level, the fountain
on place de la République, night in front of the cinema. A second board is
taken with the video of milestone A.

### Frozen until milestone B

Missions, audio (beyond what exists), menus, interiors, game saves, minimap,
key remapping, credits. The fast-food stays in maintenance. The
neighbourhood test build (0.9) is postponed. Frozen systems only get crash
fixes.

## 5. How we work

| Rule | What it means |
| ---- | ---- |
| **The video judges** | A candidate is judged on the 10-point grid, read on the frames of the gate video. Numbers serve the grid, not the other way round. |
| **Bounded ground** | Until milestone A we only work on the circuit and its corridor. |
| **One candidate, one visible difference** | Each candidate says what the video will show differently. Batches are grouped: six candidates a day at most. |
| **PC first** | `next` moves through the short gate, `main` through the full gate. The Mac blocks nothing. |
| **The map is replayed** | Frozen base plus steps in Git; never a state inherited from the previous gate. |
| **Proof leaves the PC** | Frames and video are filed with the gate summary, logos blurred. |
| **Build on what exists** | Epic's models and systems before home-made ones; one replacement at a time, with its measurement. |
| **Free assets only** | Zero euros: Quixel Megascans, MetaHuman, City Sample, free Fab content, CC0, home-made. Every asset has its licence line in [assets/SOURCES.md](../assets/SOURCES.md). |

## 6. Timeline

Orders of magnitude, reviewed at every milestone, no promise beyond them.

| When | What | Version 5 |
| ---- | ---- | ---- |
| **27 November 2026** | A1: the circuit holds | |
| **1 December 2026** | A2: what the camera sees | |
| **Around 4 December 2026** | Milestone A: steal a car and flee the police | End of November |
| **End of December 2026** | Milestone B: the 10 minutes | Mid-December |
| **End of January 2027** | Milestone C: the 5 image pairs | End of January |
| **February 2027** | The north of the Presqu'île, Bellecour to Terreaux (~1.5 km²), at the level of milestone B | February |
| **March 2027** | Mac port | March |
| **Spring 2027, with a wide margin** | A playable, good-looking neighbourhood on PC | Spring |
| Then | The story: thefts of Lyon specialities, getaways through the traboules | |

Out of reach for this plan: all of Lyon, cinematics.

## 7. Next steps

1. Choose the loop on the lane file, commit it to Git, check its lanes
   against fixed scenery.
2. Make the autopilot follow the loop; a car put back on a lane makes the
   video invalid.
3. Write the short gate and the one-page summary; remove the runs of the
   second traffic system.
4. Freeze the base map and the order of the steps.
5. Police units at 25 km/h on average on the loop.
6. Chase camera: no wall, no roof, no foreground.
7. Distance and corridor: no empty frame left on the loop.
8. Characters: hair, poses.
9. A 3-minute video, then the hands-on session.

## 8. Risks

| Risk | Mitigation |
| ---- | ---- |
| The ideal loop does not exist in today's lanes (dead ends, narrow streets) | It is chosen on the data before any code; a 1.5 km loop is enough for 3 minutes with two laps |
| The autopilot follows the loop but the game disappoints hands-on | A five-minute session from A1, without waiting for the final video |
| The short gate lets a performance regression through | The daily full gate sees it the same day; `main` only moves through it |
| The distance costs too much for 8 GB of video memory | Measured by point 10 of the grid at every gate |
| The single PC is stopped | The queue runs on its own; any stop longer than an hour is reported with its effect on the dates |
| Merging workstreams loses know-how | Each closed workstream leaves its handover note; the code and tools stay |
| A defect outside the corridor gets in the way later | It is written in the dashboard; it is handled when the ground widens (milestone B) |

## 9. What has shipped

| Version | Content | Released |
| ---- | ---- | ---- |
| 0.1 Play in one click | Launch the game and walk on rue de la République | 9 Nov 2026 |
| 0.2 The dressed street | A faithful, dressed street, playable on an M1 laptop | 10 Nov 2026 |
| 0.3 Behind the wheel | Steal a car, cross the traffic to the Rhône quay | 10 Nov 2026 |

The next public release will ship when milestone B is met; its number will
be set then.
