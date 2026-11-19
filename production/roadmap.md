# Roadmap: Gran Theft Praline

*French version: [roadmap.fr.md](roadmap.fr.md)*
*Version 5, 2026-11-19: review one week after the refocus. The slice is
split into three milestones, and the game comes before the image. Replaces
version 4 (12 November). The [0.9 test-phase plan](test-phase-plan.md)
stays paused. Earlier versions stay in the Git history.*
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
police.**

The story comes **afterwards**. The direction is set: thefts of Lyon
specialities and getaways through the traboules. It will only be written
once priority 1 meets its bar.

## 2. Where we are, honestly (19 November)

`next` and `main` are on candidate 46. Everything below comes from the gate
reports of the packaged game on the reference PC; what has not been
measured is written as such.

### Proven in the packaged game

- **Stable.** Six gates passed in a row on 15 and 16 November (candidates
  40 to 46), without a single crash over the 7 to 9 runs of each gate. A
  frame takes about 15 ms on the PC (measured on 14 November), of which
  11.8 ms of game logic on candidate 46.
- **An automated run of 7 steps scores 7 out of 7** since candidate 41:
  stealing a moving car, a chase, stealing a parked car with its alarm,
  walking with a gun drawn, holding up a passer-by from far and from close,
  shooting.
- **Traffic**: about a hundred cars moving out of 120 simulated, held over
  300 s.
- **Epic's models have replaced ours**: the player is no longer a mannequin
  (Nicolito, candidate 42), the crowd is made of City Sample characters
  (candidate 43), traffic and police drive City Sample cars (13 November).
- **Chase**: 3 stars, 5 police units, a 285 m getaway and a 73 km/h peak on
  candidate 46. The target is 105 to 135 km/h, and the test's automatic
  driver is put back on a lane when it gets stuck.

### Not proven, or not there

- **Nobody has played.** No hands-on session is on record since 0.3
  (10 November). The feel of driving, shooting and stealing has been judged
  by no one.
- **Driving was broken from 13 to 16 November**: the four wheels of the
  City Sample cars sat at the same point, hence the wheelspin and the body
  scraping the road. The fix is in candidate 47, whose gate is running as
  this review is written.
- **Do the police really follow?** Their average speed in a chase was
  1 km/h on candidate 43. It has not been measured again since: unknown.
- **Pedestrians do not react in a readable way.** The hands-up pose is
  unreadable on the PC's images, and the fear animations (run, cower) were
  not loaded on candidate 40; not rechecked since.
- **No packaged test covers**: traffic lights and traffic giving way,
  witnesses calling the police, bare-hands combat, losing the police,
  arrest, death and respawn. The code has existed since 13 November; its
  behaviour in the packaged game: unknown.
- **The image has not been measured since 12 November.** The only board of
  the 5 pairs dates from candidate 17b (1 out of 5 on two views, 0 on the
  other three). Since then: shop windows and night lighting (13 November),
  Epic's player, crowd and cars. No second board, no image work on `main`
  since 13 November, and no capture has left the PC since that date. The
  real level of the image today: unknown. City generation with Epic's
  building blocks (CitySamplePCG) has never been tried, and the far
  backdrop has never been opened in the editor.
- **The 10-minute video has not been shot.**

### Why we are here

- **Seven days since version 4, four of actual work**: one day to move the
  project to a dedicated disk (14 to 15 November), two days of complete
  stop (16 to 18 November).
- **Replacing home-made content with Epic's models was the right call, but
  each replacement broke something** (car wheels, head material, hands-up
  pose) and cost one to three days of diagnosis through logs.
- **One PC, one queue.** Nothing compiles anywhere else; every trial waits
  its turn.
- **The gate measured the test robot more than the game.** A step could
  pass without proving the behaviour: the chase of candidate 41 was
  validated with a car standing still.
- **The "5 pairs per candidate" rule was not kept** once the order of
  priorities was set, while version 4 still held the image as an equal
  criterion.

## 3. The target: the slice in three milestones

The ground does not change: **300 to 400 m of street, from Bellecour to the
Jacobins**, on PC. The whole slice of version 4 will not hold by the end of
November. It is split, in the order of priorities.

### Milestone A: steal a car and flee the police (end of November 2026)

It is done when, on the PC package:

1. You steal a car, moving or parked, and it drives well: 0 to 100 km/h
   measured, a peak between 105 and 135 km/h, a body that no longer touches
   the road.
2. The stars go up, and the police really follow: the average speed of
   their units is measured, not only their number.
3. You lose them, or you get arrested.
4. **Someone plays it hands-on** and says it feels like GTA. Proof: an
   unedited 3-minute video, taken off the PC.

### Milestone B: the 10 minutes (mid-December 2026)

The whole gameplay bar of version 4: traffic that stops at lights and gets
out of the way, passers-by who live and react (flee, raise their hands,
call the police), combat, stars, arrest, death and respawn. Each behaviour
has its test in the packaged game. Proof: **a video of the 10 minutes**,
unedited.

### Milestone C: the street holds up (end of January 2027)

The **5 real photo / game capture pairs** from the same viewpoints:
Bellecour entrance, cinema façade, shop windows at eye level, the fountain
on place de la République, night in front of the cinema.

A second board is taken **right away**, to know where the image stands: it
is a measurement, not a workstream. Image work resumes after milestone B,
with a trial of city generation using Epic's building blocks.

### At every gate

PC package without crashes, as today.

### Frozen until milestone B

Missions, audio (beyond what exists), menus, interiors, game saves, minimap,
key remapping, credits. The fast-food stays in maintenance. The
neighbourhood test build (0.9) is postponed. Frozen systems only get crash
fixes.

## 4. How we work

| Rule | What it means |
| ---- | ---- |
| **PC first** | A change lands on `next` when CI is green, the project compiles on the PC, a PC package runs 120 s without a crash and the gate's thresholds hold (frame time, automated run, traffic). |
| **A test proves a behaviour** | Each step of the automated run measures what the player would see (metres driven, speed, hands raised on screen), not only an internal state. A 7 out of 7 is not worth a hands-on session. |
| **Proof leaves the PC** | A capture or a video that stays on the PC proves nothing for the project. Milestone images and videos are filed with the report, logos blurred. |
| **One replacement at a time** | When an Epic model replaces home-made content, it comes alone in its candidate, with the measurement showing nothing regressed. |
| **Free assets only** | Zero euros: Quixel Megascans, MetaHuman, City Sample, free Fab content, home-made. Every asset has its licence line in [assets/SOURCES.md](../assets/SOURCES.md). |
| **Mac later** | The MacBook Pro M1 Pro does not block merges. The playable Mac port comes after the neighbourhood. |
| **Three gameplay workstreams, one for machines** | Driving and traffic; pedestrians; police; machines (builds, packages, gates). The image is paused until milestone B, except for the measurement board. |

## 5. Timeline

Orders of magnitude, reviewed at every milestone, no promise beyond them.

| When | What | Version 4 |
| ---- | ---- | ---- |
| **End of November 2026** | Milestone A: steal a car and flee the police | The whole slice |
| **Mid-December 2026** | Milestone B: the 10 minutes | End of November |
| **End of January 2027** | Milestone C: the 5 image pairs | End of November |
| **February 2027** | The north of the Presqu'île, Bellecour to Terreaux (~1.5 km²), at the level of milestone B. To be confirmed after the first city generation trial | Mid to late December |
| **March 2027** | Mac port | February |
| **Spring 2027, with a wide margin** | A playable, good-looking neighbourhood on PC | Around February |
| Then | The story: thefts of Lyon specialities, getaways through the traboules | |

Out of reach for this plan: all of Lyon, cinematics.

## 6. Next steps

1. **Candidate 47** (gate running): City Sample car wheels in their place,
   hands-up animation found by the game, bollards taken off the
   carriageway.
2. **Candidate 48**: centre of mass of the cars (the police car rears up),
   torque of the player's car, a reliable armed-walk test.
3. Measure the police's speed in a chase, then fix it.
4. Second board of the 5 views and first getaway video, taken off the PC.
5. First hands-on session of milestone A.
6. Crowd fear animations, a readable hands-up pose.

## 7. Risks

| Risk | Mitigation |
| ---- | ---- |
| The single PC is off or its queue is blocked | Batches are written without it and wait grouped; any stop longer than a day is reported with its effect on the dates |
| Another complete stop of the work (two days lost from 16 to 18 November) | Dates given with their margin; slippage announced the same day |
| A test passes without proving the behaviour | The "a test proves a behaviour" rule; every gate re-reads what the step measured |
| Driving or the chase disappoint hands-on, found out late | The hands-on session is the bar of milestone A, not a step after it |
| The image is further off than we think | Measurement board right away; the city method is decided on a trial, not on a plan |
| A replacement by an Epic model breaks a system | One replacement per candidate, with its measurement |
| The PC (8 GB of video memory) limits the image | Budgets measured when image work resumes |

## 8. What has shipped

| Version | Content | Released |
| ---- | ---- | ---- |
| 0.1 Play in one click | Launch the game and walk on rue de la République | 9 Nov 2026 |
| 0.2 The dressed street | A faithful, dressed street, playable on an M1 laptop | 10 Nov 2026 |
| 0.3 Behind the wheel | Steal a car, cross the traffic to the Rhône quay | 10 Nov 2026 |

The next public release will ship when milestone B is met; its number will
be set then.
