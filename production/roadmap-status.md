# Dashboard: where the game stands

*French version: [roadmap-status.fr.md](roadmap-status.fr.md). Roadmap: [roadmap.md](roadmap.md). Systems: [systems-inventory.md](systems-inventory.md).*
*Updated at every milestone and every review. Last update: 2026-11-24 (`next` = `main` = candidate 76; roadmap version 6).*

Legend: ✅ proven in the packaged game · 🟡 partly · ⬜ not done · ❓ not measured, unknown · ❄️ frozen

## Current target: milestone A on the getaway circuit

Full criteria: [roadmap §3](roadmap.md#3-what-changes) and
[§4](roadmap.md#4-the-milestones).

| Milestone | Target | Status | Latest proof |
| ---- | ---- | ---- | ---- |
| A1. The circuit holds (grid points 1 to 6) | 27 November 2026 | 🟡 3 points out of 6; the circuit is not chosen yet | candidate 76, 24 Nov |
| A2. What the camera sees (points 7 to 9) | 1 December 2026 | ⬜ 0 out of 3 | candidate 76, 24 Nov |
| A3. The 3-minute video, then the hands-on session (point 10) | Around 4 December 2026 | ⬜ | |
| B. The 10 minutes | End of December 2026, re-dated at milestone A | ⬜ | |
| C. The 5 photo / capture pairs | End of January 2027 | 🟡 a single board: 1/5 on two views, 0 on three | candidate 17b, 12 Nov |
| PC package without crashes (every gate) | Permanent | ✅ no crash at gates 70 to 77 | 24 Nov |

## The 10-point grid, candidate 76

Read on the gate's logs and on the frames of its video.

| # | Point | Candidate 76 | |
| ---- | ---- | ---- | ---- |
| 1 | The theft in under 10 s | 6 s | ✅ |
| 2 | 50 km/h on average, a 90 km/h peak | 35 on average, an 89 peak | ⬜ |
| 3 | Unaided | put back on a lane 4 times in 4 filmed minutes | ⬜ |
| 4 | Police in frame, units at 25 km/h | recognisable police car; units at 17 km/h | 🟡 |
| 5 | 8 moving cars within 150 m | gate criterion met | ✅ |
| 6 | A readable ending before 120 s | arrest at 63 s, warning on screen | ✅ |
| 7 | The camera holds | camera on the car's roof at the arrest | ⬜ |
| 8 | No emptiness | bare ground, blocks without façades, a blue band on the horizon (60 s) | ⬜ |
| 9 | Whole characters | shaved heads | ⬜ |
| 10 | Smooth and clean | automated run: 64.8 % of frames under 33 ms (99.8 % in candidate 77) | ⬜ |

**3 out of 10.**

## Gates from 22 to 24 November

18 gates, 24 hours of PC, 81 minutes per gate on average.

| Candidate | Verdict | What decided |
| ---- | ---- | ---- |
| 59 to 62 | passed (5 gates) | From 60 the gate judges the traffic of the default game |
| 64 | criteria met | |
| 65 | failed | Incomplete run, 288 error lines after the dressing of the getaway |
| 66 | failed | 288 error lines |
| 67, 68, 69 | failed | The same uncooked police paint shader (6 error lines); 7.1 cars for 8 in 67 |
| 70, 71 | criteria met | Police paint switched off, matt ground |
| 72 | criteria met, turned down on the video | Game logic +49 %, video memory exhausted, a New York taxi |
| 74 | failed | 7.6 moving cars for 8 |
| 75 | passed | White police car with a blue band, recognisable |
| 76 | passed, `next` = `main` | Theft, getaway peaking at 89 km/h, arrest at 63 s |
| 77 | criteria met, not promoted | A preparation step removed 14 façade modules |

## Gates from 15 to 21 November

| Candidate | Date | Automated run | Game logic, median | Traffic moving | What it brings |
| ---- | ---- | ---- | ---- | ---- | ---- |
| 40 | 15 Nov | 6/7 | 11.6 ms | 98 | First gate on the dedicated disk |
| 41 | 16 Nov | 7/7 | 12.5 ms | 104 | Crowd head material fixed |
| 42 | 16 Nov | 7/7 | 10.8 ms | 94 | Nicolito as the default player |
| 43 | 16 Nov | 7/7 | 11.8 ms | 100 | City Sample crowd by default; a 150 m chase required |
| 45 | 16 Nov | 7/7 | 11.3 ms | 103 | Original wheels and differential; 70 km/h peak |
| 46 | 16 Nov | 7/7 | 11.8 ms | 103 | 285 m getaway, 73 km/h peak |
| 47 | 19 Nov | 7/7 | 12.9 ms | 50 | Failed on traffic; wheel bones, bollards off the carriageway |
| 48 | 19 Nov | 7/7 | 12.6 ms | 95 | Traffic physics kept as at 46; 340 m chase, 84 km/h peak |
| 49 | 19 Nov | 6/7 | 12.2 ms | 97 | Street props moved off the traffic lanes |
| 50 | 19 Nov | 7/7 | 13.4 ms | 103 | Ammo on the combat HUD; the witness check fails |
| 51 | 20 Nov | 7/7 | 12.8 ms | 95 | New police pursuit check |
| 52 | 20 Nov | 7/7 | 13.0 ms | **50** upright | The count no longer includes tipped-over cars: traffic is broken; witness fixed |
| 53, 54 | testing | | | | Real wheels and City Sample's own physics for traffic, police spawns, hold-up camera probe |

Up to candidate 51, "traffic moving" also counted cars lying on their side
and sliding: with the honest count, 52 has 49.7 upright cars moving, and
71 to 100 % of the cars within 100 m of the player are tipped over.

Gate thresholds at that date: game logic median at most 1.3 times that of the previous
gate; automated run at least 6 out of 7; from candidate 53, at least 75.6
upright cars moving on average from 120 to 300 s and at most 5 % of the
cars within 100 m tipped over; no crash.

## Priority 1 systems

| System | Measured on the PC | What is missing |
| ---- | ---- | ---- |
| Walk, run, camera | 🟡 Nicolito as the player; camera inside walls and on car roofs | Chase camera, hands-on session |
| Stealing a car | ✅ a moving car, in 6 s (candidate 76) | Hands-on session |
| Driving | 🟡 0 to 100 km/h in 6.6 s, 102 km/h after 134 m (launch test, candidate 76) | Hands-on session |
| Traffic | 🟡 default traffic upright, 8 to 12 moving cars within 150 m during the getaway | Lights and avoidance ❓ |
| Pedestrians | 🟡 City Sample crowd, passers-by who flee and raise their hands | Shaved heads, bodies under cars |
| Police, stars | 🟡 3 stars, recognisable car, warning, arrest | Units at 17 to 21 km/h for 25; losing the police ❓ |
| Basic combat | 🟡 hold-up and pistol fire tested | Unarmed ❓ |
| Death and respawn | ❓ | Not re-read for this review |

## Image

The image has moved forward alongside the game since 23 November, on what
the camera sees during the getaway.

| Gap | State |
| ---- | ---- |
| Façades of the getaway streets read as "painted boxes" | Lyon stone tone and recessed windows in `main` (candidate 76); the stone still reads as planks at 89 km/h |
| Empty distance (bare ground, blue band, smooth blocks) | "Distance" batch written, not seen in the game yet ⬜ |
| Bare quay, white pavements | Ground albedo capped; neither trees nor furniture on the quay ⬜ |
| The 5 photo / capture pairs | A single board (12 Nov); the second is taken with the video of milestone A ⬜ |

## Defects noted outside the corridor

To be filled in once the circuit is chosen: a defect seen outside the
corridor is written here, not fixed before milestone A.

## Frozen

❄️ Missions · audio beyond what exists · menus · interiors (fast-food in
maintenance) · game saves · minimap · remapping · credits · 0.9 test build.

## Versions

| Version | Status |
| ---- | ---- |
| 0.1 Play in one click | ✅ 2026-11-09 |
| 0.2 The dressed street | ✅ 2026-11-10 |
| 0.3 Behind the wheel | ✅ 2026-11-10 (latest public release) |
| Next release | ⬜ when milestone B is met |
