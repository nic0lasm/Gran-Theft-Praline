<p align="center">
  <b>English</b> · <a href="README.fr.md">Français</a>
</p>

<h1 align="center">Gran Theft Praline</h1>

<p align="center">
  <b>An original open-world game in a faithful Lyon, street by street.</b><br>
  Walk it, drive it, and soon get chased through it.
</p>

<p align="center">
  <img src="docs/media/readme/hero-cafe-terrace.jpg" alt="In-game: a café terrace and a car driving down rue de la République, version 0.2" width="100%">
  <br><sub><i>In-game, version 0.2, rue de la République. Filmed by the game itself on a MacBook Pro M1 Pro.</i></sub>
</p>

<p align="center">
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.3.0"><img alt="Download 0.3.0" src="https://img.shields.io/badge/download-0.3.0%20for%20macOS-success?logo=apple"></a>
  <img alt="0.4 in progress" src="https://img.shields.io/badge/next-0.4%20The%20living%20street-orange">
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml/badge.svg?branch=main"></a>
  <img alt="Unreal Engine 5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.8-0e1128?logo=unrealengine">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-blue.svg"></a>
</p>

<p align="center">
  <a href="#lyon-for-real">Lyon, for real</a> ·
  <a href="#in-pictures">In pictures</a> ·
  <a href="#play-in-one-click">Play</a> ·
  <a href="#where-we-are">Where we are</a> ·
  <a href="#roadmap">Roadmap</a> ·
  <a href="#build-it-yourself">Build</a> ·
  <a href="#contributing">Contribute</a> ·
  <a href="#credits-and-licence">Credits</a>
</p>

---

Gran Theft Praline is a fan-made, open-source, non-commercial project with a
big goal: the handling, interactions and image quality of today's best
open-world games, **on the real map of Lyon**. No story yet, on purpose: the
playground comes first. Everyone is welcome to follow along, try the builds,
share what they know about the city or lend a hand.

## Lyon, for real

The city is not drawn by hand: it is **rebuilt from public data**. Building
footprints and heights come from IGN's LiDAR and BD TOPO, trees, kerbs and
street furniture from the Métropole de Lyon, shop names from OpenStreetMap.
Every plane tree, bench and shopfront stands where it stands in the real
street.

<table>
  <tr>
    <th width="50%">The real rue de la République</th>
    <th width="50%">The same street in the game (0.3 in progress)</th>
  </tr>
  <tr>
    <td><img src="docs/media/readme/real-rue-republique.jpg" alt="Photo of the pedestrian rue de la République in Lyon, stone façades and plane trees" width="100%"></td>
    <td><img src="docs/media/readme/0.3-rue-republique-wide.jpg" alt="In-game: running down rue de la République between stone façades, shopfronts and plane trees" width="100%"></td>
  </tr>
  <tr>
    <td><sub>Photo: Corentin Eustacchi, <a href="https://commons.wikimedia.org/wiki/File:Rue_de_la_R%C3%A9publique_Nord_pi%C3%A9tonne_(Lyon,_2025).jpg">Wikimedia Commons</a>, CC BY-SA 4.0, cropped.</sub></td>
    <td><sub>Real building heights from IGN LiDAR (median roof error 0.39 m), 175 buildings, 13,000 façade modules, 112 plane trees.</sub></td>
  </tr>
</table>

What the game still lacks next to the photo, honestly: leaves on the plane
trees, the real shop logos, people in the street, and a real character model
(the player is still Epic's white mannequin). They are all on the roadmap.

## In pictures

All images below are frames from videos the game records of itself during
its automated tests, on a MacBook Pro M1 Pro with the everyday **M1**
quality profile. Nothing is retouched.

<table>
  <tr>
    <td width="50%"><img src="docs/media/readme/0.3-driving-rue-republique.jpg" alt="In-game: driving a car down rue de la République in third person" width="100%"><br><sub>Behind the wheel down rue de la République (0.3)</sub></td>
    <td width="50%"><img src="docs/media/readme/0.2-carjack-approach.jpg" alt="In-game: the character walks up to a parked car to steal it" width="100%"><br><sub>Walking up to a parked car: press F to take it (0.2)</sub></td>
  </tr>
  <tr>
    <td><img src="docs/media/readme/0.3-square-benches.jpg" alt="In-game: a small square with cast-iron benches, plane trees and a Lyon lamppost" width="100%"><br><sub>Cast-iron benches and a Lyon lamppost, where the real ones are (0.3)</sub></td>
    <td><img src="docs/media/readme/0.3-plane-trees-lamppost.jpg" alt="In-game: on foot between plane trees and benches, a parked car on the right" width="100%"><br><sub>On foot between the plane trees (0.3)</sub></td>
  </tr>
  <tr>
    <td><img src="docs/media/readme/0.3-on-foot-crossroads.jpg" alt="In-game: a crossroads with a corner building, benches and a car in the distance" width="100%"><br><sub>A crossroads with traffic in the distance (0.3)</sub></td>
    <td><img src="docs/media/readme/real-quai-jean-moulin.jpg" alt="Photo of quai Jean-Moulin along the Rhône, plane trees, traffic and the riverside walk" width="100%"><br><sub><b>Next stop:</b> the real quai Jean-Moulin by the Rhône, coming in 0.3 with Pont Lafayette. Photo: Corentin Eustacchi, <a href="https://commons.wikimedia.org/wiki/File:Quai_Jean-Moulin_(Lyon,_2025).jpg">Wikimedia Commons</a>, CC BY-SA 4.0, cropped.</sub></td>
  </tr>
</table>

🎥 Videos and measurements: [version 0.1 test](production/evidence/0.1/),
[rendering tests in four lighting setups](docs/rendering/test-results/2026-11-09/)
(noon, golden hour, rain, night).

## Play in one click

**[⬇️ Download version 0.3.0 "Behind the wheel" for macOS](https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.3.0)**
(Mac with Apple Silicon, released November 10, 2026).

Steal a car and drive through traffic from rue de la République to the Rhône
quay and Pont Lafayette. This version also previews the systems of 0.4 to
0.6: traffic lights, pedestrians, ragdolls, police with wanted stars, a
minimap and full map, saved settings, day and night with weather, and the
sound of the street. Measured over 330 s of driving on an M1 Pro with
everything on: median frame 33.9 ms (about 30 fps), 95 % of frames under
44.5 ms. [Release notes](docs/releases/0.3.md).

The app is not signed by Apple yet: the release notes explain how to open it
the first time. You can also [build it yourself](#build-it-yourself) with a
single script.

<details>
<summary><b>Controls</b> (keyboard and gamepad)</summary>

| Action | Keyboard (AZERTY) | Keyboard (QWERTY) | Gamepad |
| ---- | ---- | ---- | ---- |
| Move | Z Q S D | W A S D | Left stick |
| Look around | Mouse | Mouse | Right stick |
| Sprint (hold) | Left Shift | Left Shift | A / Cross |
| Walk (toggle) | Left Alt | Left Alt | Left stick click |
| Jump, vault | Space | Space | X / Square |
| Crouch | C | C | B / Circle |
| First / third person | V | V | View / Share |
| Pause menu | Escape | Escape | Menu / Options |
| Get in / out of a car | F | F | Y / Triangle |
| Accelerate / brake, reverse | Z / S | W / S | Right / left trigger |
| Steer | Q / D | A / D | Left stick |
| Handbrake | Space | Space | RB / R1 |

Full details: [player character](docs/gameplay/player-character.md),
[vehicles](docs/gameplay/vehicles.md).

</details>

## Where we are

| | Version | What you can do | Status |
| :--: | ---- | ---- | ---- |
| ✅ | **0.1** Play in one click | launch the game and walk down rue de la République | released Nov 9, 2026 |
| ✅ | **0.2** The dressed street | stroll down a faithful, dressed street on an M1 laptop | released Nov 10, 2026 |
| ✅ | **0.3** Behind the wheel | steal a car and drive through traffic to the Rhône quay and Pont Lafayette | released Nov 10, 2026 |

Version 0.3.0 is out, with the systems of the next versions as previews.
Next up is 0.4 "The living street": finishing pedestrians, traffic lights,
ragdolls and breakable street furniture, and winning back the frame time
needed to keep 95 % of frames under 42 ms on the M1.

## Roadmap

The game moves forward in **playable versions**: each one launches with a
double-click and adds something you can see or do.

| Version | You can… | Target |
| ---- | ---- | ---- |
| 0.4 The living street | meet pedestrians who react, stop at traffic lights, knock street furniture over | Dec 2026 |
| 0.5 The police arrive | get wanted (1-2 stars), be chased by car, get arrested or lose the police | Dec 2026 |
| 0.6 Day and night | drive by day, at dusk, at night and in the rain | Jan 2027 |
| 0.7 Armed | fight bare-handed and with a pistol, take cover, die and respawn | Feb 2027 |
| 0.8 City life | enter a shop without loading, buy, rob it, earn money, save | Mar 2027 |
| 0.9 The whole district | roam from Bellecour to the Terreaux without loading | Apr 2027 |
| 0.10 to 0.12 | city map and GPS, six weapons, missions, scooter and Vélo'v | summer 2027 |
| 0.13 to 0.17 | final façades and landmarks, interiors, people and cars, the sound of Lyon, polish | early 2028 |
| **1.0** Vertical slice | play the whole northern Presqu'île, Bellecour → Terreaux | early 2028 |

Dates are low-confidence orders of magnitude, reviewed after every version.
Details: [full roadmap](production/roadmap.md),
[systems inventory](production/systems-inventory.md),
[game concept](design/gdd/game-concept.md).

### Two quality profiles

| Profile | What it is | Target |
| ---- | ---- | ---- |
| **Ultra** | The real visual target: full-resolution assets, Nanite, high-quality Lumen | A powerful machine (recent dedicated GPU or high-end Apple Silicon) |
| **M1** | The everyday playable profile on an Apple M1 Pro laptop | about 30 fps at 1080p |

The M1 profile only lowers graphics settings; models and materials are never
simplified. See [ADR-0001](docs/adr/adr-0001-rendering-pipeline.md) and
[ADR-0002](docs/adr/adr-0002-performance-budget.md).

## Build it yourself

### Requirements

- A Mac with Apple Silicon
- [Unreal Engine 5.8](https://www.unrealengine.com/) (Epic Games Launcher)
- Xcode with the Metal toolchain
- [Git LFS](https://git-lfs.com/)
- Python 3 (city and asset tools)

Some content is **not stored in Git**, for licence or size reasons, and is
installed or regenerated locally:

| Content | How to get it |
| ---- | ---- |
| Epic's Game Animation Sample (character, animations, camera) | Free on Fab, then [`tools/character/install_gasp_content.sh`](tools/character/README.md) |
| The rue de la République level | Generated from public data by [`tools/city`](docs/city/plan.md) and [`tools/level`](tools/level/README.md) |
| City data (OpenStreetMap tables) | Regenerated by [`tools/city`](docs/city/plan.md) (ODbL, no key needed) |
| High-quality 3D models | Kept outside Git; prepared for Unreal by [`tools/assets`](docs/assets/import-pipeline.md) |
| Real brand logos and textures | Kept outside Git in `Plugins/CityBrands/` |

### Steps

```zsh
git lfs install
git clone https://github.com/nic0lasm/Gran-Theft-Praline.git
open Gran-Theft-Praline/GranTheftPraline/GranTheftPraline.uproject
```

Then, once the content above is installed:

```zsh
tools/build/package-mac.sh        # → Builds/<version>/GranTheftPraline.app
tools/build/test-build-mac.sh     # 10-minute test, video and frame times
```

Step-by-step guide: [one-click macOS build](docs/build/one-click-build.md).

### Technology

| Area | Choice |
| ---- | ---- |
| Engine | Unreal Engine 5.8: Lumen, Virtual Shadow Maps, Substrate, TSR |
| Code | C++ for the game systems, Blueprint for prototyping |
| Platform | macOS on Apple Silicon (Metal) first, Windows later |
| Gameplay foundations | Epic's free building blocks: Game Animation Sample, Gameplay Ability System, Chaos Vehicles, StateTree |
| City | IGN (BD TOPO, LiDAR HD, RGE ALTI), data.grandlyon.com and OpenStreetMap, processed by [tools/city](tools/city/) and [tools/geo-pipeline](tools/geo-pipeline/) |
| Art | Quixel Megascans, Poly Haven and ambientCG (CC0), Blender; see the [art bible](docs/art/art-bible.md) |

## Contributing

The foundations are still moving, but your help is very welcome, and you
don't need to write code:

- **Know Lyon?** Point out a missing shop, a wrong building height, a
  detail that makes a street feel right.
- **Tried a build?** Tell us how it runs on your Mac (model, frame rate,
  problems).
- **Unreal, 3D, sound, design?** Pick a version on the
  [roadmap](production/roadmap.md) and suggest what you would like to take on.

Open an [issue](https://github.com/nic0lasm/Gran-Theft-Praline/issues) to
start the conversation, especially before a large piece of work.

<details>
<summary><b>How the repository works</b></summary>

- **Branches**: work on `feat/<topic>` or `fix/<topic>`. `next` is the
  pre-release branch, `main` the release branch. History stays linear
  (rebase, fast-forward only).
- **CI**: every push runs the *Provenance check* (commit identities, tracked
  files, branch names) and the tool tests. A change moves to `next`, then
  `main`, only once CI is green on that exact commit.
- **Evidence**: a visual or performance change comes with a screenshot or a
  measurement.
- **Languages**: code, commit messages and identifiers in English;
  documentation in English and French (`file.md` + `file.fr.md`).
- **Assets**: every external or generated asset gets a row in
  [assets/SOURCES.md](assets/SOURCES.md) with its origin and licence. Large
  binaries go through Git LFS ([rules](docs/project/git-lfs.md)).

</details>

## Credits and licence

- Geographic data: contains data from **IGN** (BD TOPO, LiDAR HD, RGE ALTI)
  and **Métropole de Lyon** (Licence Ouverte / Etalab Open Licence), and
  **© OpenStreetMap contributors** ([ODbL](https://opendatacommons.org/licenses/odbl/)).
- Photos of the real city in this README: **Corentin Eustacchi**, Wikimedia
  Commons, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/),
  cropped and resized; the two image files keep that licence.
- **Epic Games**: Unreal Engine and the Game Animation Sample are used under
  the Unreal Engine EULA and are not covered by this repository's licence.
- Shop signs and brands belong to their owners; they are reproduced for a
  non-commercial project and are not distributed in this repository. No
  real people are depicted in the game, and no assets are taken from other
  games.
- Every asset's source and licence: [assets/SOURCES.md](assets/SOURCES.md).

The code and documentation in this repository are released under the
[MIT License](LICENSE). Third-party data and assets keep their own licences.

<p align="center"><i>Made in Lyon, with love for its streets. 🦁</i></p>
