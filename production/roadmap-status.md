# Dashboard: where the game stands

*French version: [roadmap-status.fr.md](roadmap-status.fr.md). Roadmap: [roadmap.md](roadmap.md). Systems: [systems-inventory.md](systems-inventory.md).*
*Updated by the threads whenever a task is done. Last update: 2026-11-10.*

Legend: ✅ done · 🟡 in progress · ⬜ to do · ⛔ blocked

## Current version: 0.3 "Behind the wheel"

Ships as soon as the 95th percentile is ≤ 42 ms on the M1 profile with
traffic, without waiting for the other workstreams.

| # | Task | Workstream | Status | Evidence |
| ---- | ---- | ---- | ---- | ---- |
| 1 | Drivable Clio V, 208 II, Kangoo II with LODs | Vehicles | ✅ | `assets/staging/vehicles/evidence-lods/` on the Mac |
| 2 | Get in, get out, break the window, pull out an NPC driver | 0.3 | ✅ | videos `GTAL/Builds/0.3-dev/evidence` |
| 3 | Traffic on 112 lanes, NPC drivers | 0.3 | 🟡 | branch `feat/traffic-drivers` |
| 4 | Engine sound and driving panel | 0.3 | 🟡 | branch `feat/vehicle-hud-sound` |
| 5 | Extended map: Rhône quay, Pont Lafayette, 6 m cross streets | City v2 | ✅ | `0.3-city` build, 60 s test |
| 6 | Falling into the Rhône: respawn on the quay, water surface | City v2 | ⬜ | |
| 7 | NPC GPU cost reduced (shadows, LODs, drivers without animation) | 0.3 | 🟡 | Mac slot of 2026-11-10 |
| 8 | Extended map budget (culling, shadows, Lumen) | Optimisation | 🟡 | at the Mac slot |
| 9 | Per-system GPU envelopes measured a first time | Optimisation | ⬜ | |
| 10 | 600 s measurement: p95 ≤ 42 ms with traffic | 0.3 | ⬜ | |
| 11 | Release archive: split if > 2 GiB | 0.3 | ⬜ | |

## Open workstreams

| Workstream | Targets | State |
| ---- | ---- | ---- |
| Pedestrians (Mass + StateTree) | 0.4 | cloud preparation |
| Police and stars | 0.5 | cloud preparation |
| Day/night + weather | 0.6 | cloud preparation |
| M1 optimisation | ongoing | Mac slot queued |
| Street realism | ongoing | audit against photos |
| City v2 | ongoing | quay and bridge done |
| Import, free packs, Tripo | ongoing | 84 models imported; Megaplants maple to measure |

## All versions

| Version | Name | Status |
| ---- | ---- | ---- |
| 0.1 | Play in one click | ✅ 2026-11-09 |
| 0.2 | The dressed street | ✅ 2026-11-10 |
| 0.3 | Behind the wheel | 🟡 |
| 0.4 | The living street | ⬜ (pedestrians in preparation) |
| 0.5 | The police arrive | ⬜ (police in preparation) |
| 0.6 | Day and night | ⬜ (day/night in preparation) |
| 0.7 | Armed | ⬜ |
| 0.8 | City life | ⬜ |
| 0.9 | The whole district | ⬜ |
| 0.10 | Map and comfort | ⬜ |
| 0.11 | The arsenal | ⬜ |
| 0.12 | Your own city | ⬜ |
| 0.13 | Facades and landmarks | ⬜ |
| 0.14 | Interiors | ⬜ |
| 0.15 | People and cars | ⬜ |
| 0.16 | The sound of Lyon | ⬜ |
| 0.17 | Polish | ⬜ |
| 1.0 | Vertical slice | ⬜ |

## Indicators

| Indicator | Value | Target | Source |
| ---- | ---- | ---- | ---- |
| Frame time, 0.2 build, 600 s, M1 profile | p50 29.9 ms, p95 40.3 ms, 84.9 % ≤ 33 ms | p95 ≤ 42 ms (provisional), 95 % ≤ 33.3 ms (aim) | 0.2 evidence on the Mac |
| Extended 0.3 map, 60 s | 65 % of frames < 33 ms, GPU p50 32 ms | same | `0.3-city` build |
| Cost of 6 NPC drivers | about +10 ms GPU | 2 ms for all traffic (roadmap §3) | slot of 2026-11-10 |
| Release archive size | 1.36 GB (0.2) | < 2 GiB per file | release v0.2.0 |
| Models imported into Unreal | 84 (284 meshes) | | Import thread |

## Decisions awaiting Nico

- None. Order of versions 0.4 to 0.7 decided by Nico on 2026-11-10: the living street first.
