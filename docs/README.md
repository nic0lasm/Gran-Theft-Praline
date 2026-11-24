# Documentation

*French version: [README.fr.md](README.fr.md). Back to the [project README](../README.md).*

Every document exists in English (`name.md`) and French (`name.fr.md`).
Documents marked *history* describe earlier plans (versions 0.4 to 0.9,
the Mac releases, the Tripo pipeline); they are kept for the record and
no longer drive the work.

## Start here

| Document | What it answers |
| ---- | ---- |
| [Game concept](../design/gdd/game-concept.md) | What the game is, for whom, and what it is not |
| [Roadmap](../production/roadmap.md) | The slice, its three milestones and the dates |
| [Dashboard](../production/roadmap-status.md) | What is proven today, gate by gate |
| [Architecture decisions](adr/) | Why each big technical choice was made |

## Architecture decisions (ADR)

| ADR | Subject |
| ---- | ---- |
| [0001](adr/adr-0001-rendering-pipeline.md) | Photorealistic rendering pipeline |
| [0002](adr/adr-0002-performance-budget.md) | Performance target and frame budget |
| [0003](adr/adr-0003-movement.md) | Movement: on foot, driving, non-player drivers |
| [0004](adr/adr-0004-wanted-police.md) | Wanted level and police pursuit |
| [0005](adr/adr-0005-pedestrians.md) | Pedestrians |
| [0006](adr/adr-0006-ragdoll-and-impacts.md) | Ragdoll and physical reactions |
| [0007](adr/adr-0007-interface-settings-minimap.md) | Interface texts, settings and minimap |
| [0008](adr/adr-0008-day-night-weather.md) | Code-driven day-night cycle and weather |
| [0009](adr/adr-0009-audio-mix.md) | Game audio |
| [0010](adr/adr-0010-city-cells-streaming.md) | Loading the quarter by cells |
| [0011](adr/adr-0011-gameplay-framework.md) | Gameplay framework for combat |
| [0012](adr/adr-0012-interiors.md) | Interiors without loading |
| [0014](adr/adr-0014-missions-framework.md) | Missions framework, data in tables |
| [0015](adr/adr-0015-mass-crowd-and-traffic.md) | Mass crowd and traffic; legacy traffic by default |

## The city

| Document | Subject |
| ---- | ---- |
| [Reproducing Lyon faithfully](city/plan.md) | Data sources and pipeline |
| [Generating the Presqu'île in 3D](city/generation.md) | LiDAR, Grand Lyon data, procedural façades |
| [Art bible](art/art-bible.md) | The look the game aims for |
| [Rendering strategy](rendering/rendering-strategy.md) | Photorealism: what is used and why |
| [Day-night cycle and weather](rendering/day-night-weather.md) | Sun, sky, rain |
| [Free assets survey](art/free-assets.md), [sourcing survey](sourcing/asset-sourcing.md) | What exists for free before building anything |

## Gameplay

| Document | Subject |
| ---- | ---- |
| [Playable character](gameplay/player-character.md) | Nicolito and the Game Animation Sample |
| [On-foot feel](gameplay/on-foot-feel.md) | Camera, prompts, ambience |
| [Drivable vehicles](gameplay/vehicles.md) | Stealing and driving |
| [Traffic on MassTraffic](gameplay/traffic-mass.md), [traffic reactions](gameplay/traffic-reactions.md) | City Sample traffic in Lyon |
| [Pedestrians](gameplay/pedestrians.md), [pedestrians on Mass](gameplay/pedestrians-mass.md) | The crowd and its reactions |
| [Wanted level and police](gameplay/wanted.md) | Witnesses, stars, pursuit |
| [The circuit of the filmed flight](gameplay/flight-circuit.md) | The lap the automatic run flees on, its file, its driving line |
| [Combat and weapons](gameplay/combat.md), [hit reactions](gameplay/hit-reactions.md), [threat response](gameplay/threat-response.md) | Hold-ups and shooting |
| [Ragdoll](gameplay/ragdoll.md), [breakable street furniture](gameplay/breakable-street-furniture.md) | Physical reactions |
| [Animation sources](animation/sources.md), [combat animations](animation/combat-sources.md), [car poses](animation/car-poses.md) | Where the animations come from |
| [Test drives](testing/test-drives.md) | Scripted drives used by the tests |

## Performance and testing

| Document | Subject |
| ---- | ---- |
| [Vehicle cost](performance/vehicle-cost.md), [pedestrian cost](gameplay/pedestrians-cost.md), [ragdoll cost](gameplay/ragdoll-cost.md) | What each system costs per frame |
| [QA and release](qa/README.md) | Gates, checklists, bug template |
| [Git LFS](project/git-lfs.md) | Which files go through LFS |

## History

| Document | Subject |
| ---- | ---- |
| [Version 0.3.0 release notes](releases/0.3.md) | The latest public build, macOS |
| [One-click macOS build](build/one-click-build.md) | How the Mac releases were packaged |
| [Systems inventory](../production/systems-inventory.md) | The review of 10 November, against roadmap v3 |
| [Plan to version 0.9](../production/test-phase-plan.md), [0.9 test protocol](qa/test-protocol.md) | A test build postponed by the refocus of 12 November |
| [Missions](gameplay/missions.md), [money](gameplay/money.md), [reference interior](gameplay/reference-interior.md), [audio](audio/audio-design.md), [interface](ui/) | Systems frozen until the slice is done |
| [Tripo import pipeline](assets/import-pipeline.md), [asset catalog](../production/asset-catalog.md) | Generated assets, replaced by Epic's free content |
| [Rendering test results](rendering/test-results/) | Mac captures from 9 and 10 November |
