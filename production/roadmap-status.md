# Dashboard: where the game stands

*French version: [roadmap-status.fr.md](roadmap-status.fr.md). Roadmap: [roadmap.md](roadmap.md). Systems: [systems-inventory.md](systems-inventory.md).*
*Updated at every milestone and every review. Last update: 2026-11-21 (`next` = `main` = candidate 52, gate of candidate 54 running).*

Legend: ✅ proven in the packaged game · 🟡 partly · ⬜ not done · ❓ not measured, unknown · ❄️ frozen

## Current target: the rue de la République slice, in three milestones

300 to 400 m from Bellecour to the Jacobins, judged on PC. Full bars:
[roadmap §3](roadmap.md#3-the-target-the-slice-in-three-milestones).

| Milestone | Target | Status | Latest proof |
| ---- | ---- | ---- | ---- |
| A. Steal a car and flee the police | End of November 2026 | 🟡 theft and witness proven; 251 m getaway with a 70 km/h peak (target 105 to 135); police units at 17 km/h for 25; traffic broken (cars tipped over); video not shot | candidate 52, 20 Nov |
| B. The 10 minutes | Mid-December 2026 | ⬜ video not shot; pedestrian reactions, lights, arrest, death and respawn ❓ | |
| C. The 5 photo / capture pairs | End of January 2027 | 🟡 a single board: 1/5 on two views, 0 on three; nothing measured since | candidate 17b, 12 Nov |
| PC package without crashes (every gate) | Permanent | ✅ twelve gates in a row without a crash | candidates 40 to 52, 15 to 20 Nov |

## Gates since version 4

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

Gate thresholds: game logic median at most 1.3 times that of the previous
gate; automated run at least 6 out of 7; from candidate 53, at least 75.6
upright cars moving on average from 120 to 300 s and at most 5 % of the
cars within 100 m tipped over; no crash.

## Priority 1 systems

| System | Measured on the PC | What is missing |
| ---- | ---- | ---- |
| Walking, running, camera | ✅ Nicolito player, armed walk tested | Hands-on session |
| Stealing a car | ✅ moving and parked, alarm | Hands-on session |
| Driving | 🟡 0 to 50 km/h in 4.4 s, 71 km/h peak (launch trial, candidate 46); wheel fix in 47 | 0 to 100, a 105 to 135 peak, a body that no longer touches the road |
| Traffic | ❌ about fifty upright cars moving, most nearby cars tipped over (candidate 52) | Upright traffic, then lights and giving way ❓ |
| Pedestrians | 🟡 City Sample crowd, 24 near characters; hands-up pose measured; a witness calls the police (one car model) | Fear animations, witness with every car ❓ |
| Police, wanted stars | 🟡 3 stars, 5 units; 3 of 4 police checks | Pursuit speed (17 km/h for 25), losing the police ❓, arrest ❓ |
| Basic combat | 🟡 hold-up and pistol shooting tested | Bare hands ❓ |
| Death and respawn | ❓ | Packaged test |

## Image

| Gap on the first board (12 Nov) | Since |
| ---- | ---- |
| Black shadows | Ultra profile on PC (13 Nov); no new board ❓ |
| Empty map beyond 45 m | Far backdrop ready, never opened in the editor ⬜ |
| Grey tiled ground | ❓ |
| Black shop windows and night | Shop windows and night lighting validated on PC (13 Nov) |
| Mannequin player | ✅ Nicolito (candidate 42) |
| No pedestrians or cars in the views | City Sample crowd and cars in game; no new board ❓ |

No capture has left the PC since 13 November.

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
