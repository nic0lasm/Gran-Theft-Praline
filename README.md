<p align="center">
  <b>English</b> · <a href="README.fr.md">Français</a>
</p>

<p align="center">
  <img src="docs/media/banner.svg" alt="Gran Theft Praline: an open world in Lyon" width="100%">
</p>

<p align="center">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-blue.svg"></a>
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml/badge.svg?branch=main"></a>
  <img alt="Unreal Engine 5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.8-0e1128?logo=unrealengine">
  <img alt="Platform: macOS" src="https://img.shields.io/badge/platform-macOS%20(Apple%20Silicon)-lightgrey?logo=apple">
  <img alt="Status: pre-production" src="https://img.shields.io/badge/status-pre--production-orange">
</p>

# Gran Theft Praline

**An original open-world game set in a faithful, photorealistic Lyon.**

Walk the streets of the Presqu'île, take the wheel, step into the shops and
buildings, and deal with the police, in a city rebuilt from real geographic
data. The aim is ambitious and assumed: the handling, density of interaction
and image quality players expect from the very best modern open-world games,
on the map of a real French city.

The first playable area is a **vertical slice of the northern Presqu'île**,
from Place Bellecour to Place des Terreaux, between the Saône and Rhône
embankments (about 1.5 km²).

> **There is no story yet, on purpose.** We are building the playable base
> first: movement, driving, weapons, enterable interiors, crowds and traffic.
> The plot, characters and tone will be written once that base exists.

Everything in the game is original. No real brands, no real people, no
assets taken from other games.

## Screenshots

Nothing to show yet: the first in-game captures will appear here with
version 0.2 (*The dressed street*). In the meantime, the rendering targets
and measurements are documented in
[docs/rendering/rendering-strategy.md](docs/rendering/rendering-strategy.md).

## Project status

The project is in **pre-production**. The Unreal project, the geodata
pipeline, the rendering strategy and the roadmap are in place; a greybox of
rue de la République (334 buildings from IGN data) is used to measure
performance. Nothing is playable yet.

The game moves forward in **playable versions**, each one launching with a
double-click and adding something you can see or do:

| Version | Name | You can… | Target |
| ---- | ---- | ---- | ---- |
| **0.1** | Play in one click | launch the game and walk, run, jump in rue de la République | Nov 27, 2026 |
| 0.2 | The dressed street | stroll down a photorealistic street at 30 fps | Dec 2026 |
| 0.3 | Behind the wheel | steal a parked car and drive to the Rhône | Jan 2027 |
| 0.4 to 0.6 | Armed, first shop, police | shoot, rob a shop, lose the police | Mar 2027 |
| 0.7 to 0.13 | The whole district | Bellecour → Terreaux with crowds, traffic, map, economy, day and night, weather | Oct 2027 |
| 0.14 to 0.18 | Final quality | façades, monuments, interiors, population, vehicles, sound | Apr 2028 |
| **1.0** | Vertical slice | one hour of play across the district, no blocker | **Jun 2028** |

- Full roadmap: [production/roadmap.md](production/roadmap.md)
- Live dashboard (task by task): [production/roadmap-status.md](production/roadmap-status.md)
- Game concept and feature parity tables: [design/gdd/game-concept.md](design/gdd/game-concept.md)

## Two quality profiles

Photorealism comes first, even if it makes the game heavy. Every version
ships with two profiles, picked in the options menu:

| Profile | What it is | Target |
| ---- | ---- | ---- |
| **Ultra** | The real visual target: full-resolution assets, Nanite, high-quality Lumen, uncompromised lighting and materials | A powerful machine (recent dedicated GPU or high-end Apple Silicon); validated by captures compared with real photos |
| **M1** | The everyday playable profile on an Apple M1 Pro laptop | 30 fps at 1080p, 95% of frames ≤ 33.3 ms |

The M1 profile only lowers **graphics settings**; models and materials are
never simplified to make it keep up. Details in
[ADR-0001](docs/adr/adr-0001-rendering-pipeline.md) and
[ADR-0002](docs/adr/adr-0002-performance-budget.md).

## Technology

| Area | Choice |
| ---- | ---- |
| Engine | Unreal Engine 5.8: Lumen, Virtual Shadow Maps, Substrate, TSR |
| Code | C++ for the game systems, Blueprint for prototyping |
| Platform | macOS on Apple Silicon (Metal) first, Windows later |
| Gameplay foundations | Epic's free building blocks: Game Animation Sample, Gameplay Ability System, Chaos Vehicles, StateTree, Mass (crowds and traffic) |
| City | IGN (BD TOPO, LiDAR HD, RGE ALTI), data.grandlyon.com, OpenStreetMap, processed by [tools/geo-pipeline](tools/geo-pipeline/) |
| Art | Quixel Megascans, Tripo for 3D models, Blender for clean-up and layout |

## Build and play

> A one-click build is the goal of version **0.1** (target Nov 27, 2026).
> Until then, the project can only be opened in the editor.

Requirements: a Mac with Apple Silicon, Unreal Engine 5.8 (Epic Games
Launcher), Xcode with the Metal toolchain, and [Git LFS](https://git-lfs.com/).

```zsh
git lfs install
git clone https://github.com/nic0lasm/Gran-Theft-Praline.git
open Gran-Theft-Praline/GranTheftPraline/GranTheftPraline.uproject
```

The packaging script and step-by-step build instructions will be added
under `tools/build/` with version 0.1.

## Contributing

The project is young and its foundations are still moving, but ideas,
feedback, bug reports and knowledge of Lyon are very welcome: open an
[issue](https://github.com/nic0lasm/Gran-Theft-Praline/issues) to start the
conversation before any large piece of work.

How the repository works:

- **Branches**: work on `feat/<topic>` or `fix/<topic>`. `next` is the
  pre-release branch, `main` is the release branch. History is kept linear
  (rebase, fast-forward only).
- **CI**: every push runs the *Provenance check*, which verifies commit
  identities, tracked files and branch names. A change only moves to `next`,
  then `main`, once CI is green on that exact commit.
- **Evidence**: a visual or performance change comes with a screenshot or a
  measurement.
- **Languages**: code, commit messages and identifiers in English;
  documentation in both English and French (`file.md` + `file.fr.md`).
- **Assets**: every external or generated asset gets a row in
  [assets/SOURCES.md](assets/SOURCES.md) with its origin and licence. Large
  binaries go through Git LFS. No real brands or real people.

## Credits and data

- Geographic data: contains data from **IGN** and **Métropole de Lyon**
  (Licence Ouverte / Etalab Open Licence) and **© OpenStreetMap
  contributors** (ODbL).
- Asset sources and licences are tracked one by one in
  [assets/SOURCES.md](assets/SOURCES.md).
- Unreal Engine and its sample content are used under the Unreal Engine EULA;
  they are not covered by this repository's licence.

## License

The code and documentation in this repository are released under the
[MIT License](LICENSE). Third-party data and assets keep their own licences,
listed in [assets/SOURCES.md](assets/SOURCES.md).
