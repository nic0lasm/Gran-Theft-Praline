# Dashboard: where the game stands

*French version: [roadmap-status.fr.md](roadmap-status.fr.md). Detailed roadmap: [roadmap.md](roadmap.md).*
*Updated by the threads whenever a task is finished. Last update: 2026-11-09.*

Legend: ✅ done · 🟡 in progress · ⬜ to do · ⛔ blocked

## Current version: 0.1 "Play in one click"

Target: 2026-11-27. Progress: 3 / 8 tasks.

| # | Task | Thread | Status | Evidence |
| ---- | ---- | ---- | ---- | ---- |
| 1 | Project switched to C++ (GameMode, PlayerController, GameInstance) | Build and one-click launch | ✅ | compiles with no error or warning on the M1 Pro, Shipping build launches |
| 2 | Git LFS rules set up, quota checked | Build and one-click launch | ✅ | [rules](../docs/project/git-lfs.md); ~30 MB in LFS out of 10 GiB |
| 3 | Game Animation Sample integrated (character, motion matching, camera) | Playable character | 🟡 plan ready, editor pending | [plan](../docs/gameplay/player-character.md) |
| 4 | Enhanced Input for keyboard-mouse and gamepad | Playable character | 🟡 mappings defined | [plan §5](../docs/gameplay/player-character.md) |
| 5 | Playable `L_RueRepublique` level (collisions, PlayerStart, boundaries) | Playable rue de la République | 🟡 | Reference scene merged; `tools/level/` scripts written, to run in the editor |
| 6 | Title screen and pause menu | Build and one-click launch | 🟡 | code compiled; title screen still at next test |
| 7 | macOS packaging script → `GTAL/Builds/0.1/` | Build and one-click launch | ✅ | [package-mac.sh](../tools/build/package-mac.sh): 154 s, 944 MB (Metal SM5 + SM6 cooked for the M1) |
| 8 | Build tested: 2 min video, frame-time CSV, 10 min without a crash | Build and one-click launch | ⬜ | |

In parallel (for 0.2): Tripo wave 1, street furniture, 0 / 60 models
(see [catalog](asset-catalog.md)).

## All versions

| Version | Name | Target | Status |
| ---- | ---- | ---- | ---- |
| 0.1 | Play in one click | 2026-11-27 | 🟡 |
| 0.2 | The dressed street | 2026-12-18 | ⬜ (performance measurements already under way) |
| 0.3 | Behind the wheel | 2027-01-15 | ⬜ |
| 0.4 | Armed | 2027-02-05 | ⬜ |
| 0.5 | First shop | 2027-02-19 | ⬜ |
| 0.6 | The police arrive | 2027-03-05 | ⬜ |
| 0.7 | The whole district | 2027-04-09 | ⬜ |
| 0.8 | The crowd | 2027-05-14 | ⬜ |
| 0.9 | Traffic | 2027-06-25 | ⬜ |
| 0.10 | The arsenal | 2027-07-30 | ⬜ |
| 0.11 | City life | 2027-09-03 | ⬜ |
| 0.12 | The map | 2027-09-24 | ⬜ |
| 0.13 | Time passing | 2027-10-29 | ⬜ |
| 0.14 | Façades and monuments | 2027-12-17 | ⬜ |
| 0.15 | The lived-in district | 2028-01-28 | ⬜ |
| 0.16 | People and cars | 2028-02-25 | ⬜ |
| 0.17 | Interiors | 2028-03-31 | ⬜ |
| 0.18 | The sound of Lyon | 2028-04-28 | ⬜ |
| 0.19 | Polish | 2028-05-26 | ⬜ |
| 1.0 | Vertical slice | 2028-06-16 | ⬜ |

## Indicators

| Indicator | Value | Target | Source |
| ---- | ---- | ---- | ---- |
| GPU frame time, 95th percentile (reference scene, editor, before profiles) | 43.0 to 50.7 ms | ≤ 33.3 ms in the M1 profile | Measurement of 2026-11-09, 4 lighting setups |
| Tripo models generated / integrated | 0 / 0 | ~800 by 0.17 | [catalog](asset-catalog.md) |
| Tripo Studio credits remaining | 25,200 | to be used before expiry | Studio interface |
| Most recent playable build | none | one per version | `GTAL/Builds/` |

## Decisions awaiting Nico

- M1 profile levers to hold 30 fps (during 0.2).
- Where to store the high-quality Tripo masters, out of Git (before the end of wave 1).
