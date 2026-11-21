<p align="center">
  <b>English</b> · <a href="README.fr.md">Français</a>
</p>

<h1 align="center">Gran Theft Praline</h1>

<p align="center">
  <b>An original open-world game in a faithful Lyon, street by street.</b><br>
  Steal a car on rue de la République, then try to lose the police.
</p>

<p align="center">
  <img src="docs/media/readme/hero-cafe-terrace.jpg" alt="In-game: a café terrace and a car driving down rue de la République, version 0.2" width="100%">
  <br><sub><i>In-game, version 0.2, rue de la République. Recorded by the game itself on a MacBook Pro M1 Pro.</i></sub>
</p>

<p align="center">
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml/badge.svg?branch=main"></a>
  <img alt="Status: pre-alpha" src="https://img.shields.io/badge/status-pre--alpha-orange">
  <img alt="Unreal Engine 5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.8-0e1128?logo=unrealengine">
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-game%20systems-00599C?logo=cplusplus">
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.3.0"><img alt="Latest release 0.3.0 for macOS" src="https://img.shields.io/badge/latest%20release-0.3.0%20(macOS)-success?logo=apple"></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-blue.svg"></a>
</p>

<p align="center">
  <a href="#at-a-glance">At a glance</a> ·
  <a href="#where-it-stands">Where it stands</a> ·
  <a href="#lyon-for-real">Lyon, for real</a> ·
  <a href="#in-pictures">In pictures</a> ·
  <a href="#how-it-is-built">How it is built</a> ·
  <a href="#roadmap">Roadmap</a> ·
  <a href="#play-or-build-it">Play or build</a> ·
  <a href="#credits-and-licence">Credits</a>
</p>

---

## At a glance

Gran Theft Praline is a personal, open-source, non-commercial project: a
GTA-like set on **the real map of Lyon**, rebuilt from public geographic
data, made with Unreal Engine 5.8 and Epic's free sample content. The aim is
the feel of today's best open-world games: a car you can steal, traffic and
passers-by who react, a police that hunts you down, and an image that holds
up next to a photo of the real street.

The work is focused on one **300 to 400 m slice of rue de la République,
from Bellecour to the Jacobins**, judged on a Windows PC (RTX 3070 Ti).
There is no story yet, on purpose: the playground comes first.

## Where it stands

*Last checked on 21 November 2026, against the automated tests of the
latest candidate merged on `main`.*

This is a work in progress, and this section says what is **proven** in the
packaged game, not what the code is meant to do.

| | What the player does | State on `main` |
| :--: | ---- | ---- |
| ✅ | Walk, run and look around as **Nicolito**, a fictional character made from City Sample parts | Proven in every automated run |
| ✅ | Walk up to a parked car and steal it | Proven in every automated run |
| ✅ | Hold up a passer-by with a pistol: they raise their hands | Proven by measurement; the close-up view still renders too dark |
| ✅ | A passer-by who saw the theft calls the police: a wanted star | Proven with one car model; being checked with a second one |
| 🟡 | Drive away with the police behind | A 251 m getaway is driven by the automated test; peak at 70 km/h, target 105 to 135 |
| 🟡 | Police units chase the player | 3 of 4 police checks pass; units average 17 km/h instead of 25 |
| ❌ | Drive through living traffic | **Broken on `main`**: most cars near the player end up tipped over. A fix is in testing |
| ⬜ | Lose the police, get arrested, die and respawn | Not measured yet |
| ⬜ | An image that matches the real street | 1 out of 5 on the first photo / capture comparison (12 November) |

Milestone A, *steal a car and flee the police*, is due at the end of
November 2026 and will be shown as an unedited three-minute video recorded
on the PC. It has not been filmed yet.

## Lyon, for real

The city is not drawn by hand: it is **rebuilt from public data**. Building
footprints and heights come from IGN's LiDAR and BD TOPO, trees, kerbs and
street furniture from the Métropole de Lyon, shop names from OpenStreetMap.
Every plane tree, bench and shopfront stands where it stands in the real
street.

<table>
  <tr>
    <th width="50%">The real rue de la République</th>
    <th width="50%">The same street in the game (version 0.3)</th>
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
trees, the real shop signs, believable light in the shade, and a city that
continues beyond the slice. The city is being moved to Epic's City Sample
procedural tools; the pictures here predate that work.

## In pictures

These are frames from videos the game recorded of itself during its
automated tests on a MacBook Pro M1 Pro (versions 0.2 and 0.3, before the
move to PC). Nothing is retouched. They show the street, not the current
characters or traffic: newer captures will be added once milestone A is met.

<table>
  <tr>
    <td width="50%"><img src="docs/media/readme/0.3-driving-rue-republique.jpg" alt="In-game: driving a car down rue de la République in third person" width="100%"><br><sub>Behind the wheel down rue de la République (0.3)</sub></td>
    <td width="50%"><img src="docs/media/readme/0.2-carjack-approach.jpg" alt="In-game: the character walks up to a parked car to steal it" width="100%"><br><sub>Walking up to a parked car to take it (0.2)</sub></td>
  </tr>
  <tr>
    <td><img src="docs/media/readme/0.3-square-benches.jpg" alt="In-game: a small square with cast-iron benches, plane trees and a Lyon lamppost" width="100%"><br><sub>Cast-iron benches and a Lyon lamppost, where the real ones are (0.3)</sub></td>
    <td><img src="docs/media/readme/0.3-plane-trees-lamppost.jpg" alt="In-game: on foot between plane trees and benches, a parked car on the right" width="100%"><br><sub>On foot between the plane trees (0.3)</sub></td>
  </tr>
  <tr>
    <td><img src="docs/media/readme/0.3-on-foot-crossroads.jpg" alt="In-game: a crossroads with a corner building, benches and a car in the distance" width="100%"><br><sub>A crossroads with traffic in the distance (0.3)</sub></td>
    <td><img src="docs/media/readme/real-quai-jean-moulin.jpg" alt="Photo of quai Jean-Moulin along the Rhône, plane trees, traffic and the riverside walk" width="100%"><br><sub>The real quai Jean-Moulin by the Rhône, at the end of the 0.3 drive. Photo: Corentin Eustacchi, <a href="https://commons.wikimedia.org/wiki/File:Quai_Jean-Moulin_(Lyon,_2025).jpg">Wikimedia Commons</a>, CC BY-SA 4.0, cropped.</sub></td>
  </tr>
</table>

## How it is built

The interesting part of the project, beyond the game itself, is how it is
made and checked.

**Reuse before code.** Gameplay stands on Epic's free building blocks
rather than home-made systems: the Game Animation Sample for movement,
City Sample for crowds, cars and the procedural city, the Mass framework
for hundreds of pedestrians and cars, Lyra for combat, Chaos Vehicles,
StateTree, Perception and EQS for the police. Home-made C++ (about 59,000
lines in two modules) glues them to the city of Lyon and to the rules of
the game.

**The city from open data.** Python tools turn IGN, Métropole de Lyon and
OpenStreetMap data into an Unreal level: building shells at their LiDAR
height, façades, pavements, trees and street furniture at their real
coordinates, traffic lanes for the Mass traffic system.
See [tools/city](tools/city/) and [tools/geo-pipeline](tools/geo-pipeline/).

**Every change is played before it lands.** A change reaches the `next`
branch, then `main`, only after a *gate* on the reference PC: the game is
compiled and packaged, then plays itself for several minutes under a dozen
configurations. The gate fails on any crash, on a frame-time regression
(game logic median at most 1.3 times the previous gate, currently about
13 ms), on a broken traffic count, or when the automated run `-GTARun`
fails more than one of its seven steps: steal a car, be seen by a witness,
draw the police, get chased, hold someone up, and more. More than fifty
candidates have gone through this gate since 9 November 2026; the results
are kept in the [dashboard](production/roadmap-status.md).

**Honest measurements.** A test that passes is not taken as proof until we
check what it measures. Two examples from the logs: the traffic count used
to include cars lying on their side, and a witness test passed with a car
model the fix did not concern. Both were caught and the tests rewritten.

**A clean repository.** Continuous integration runs the tool tests and a
provenance check on every push (commit identities, tracked files, branch
names). History is linear and fast-forward only. Brand logos, Epic's
content and the raw city tables stay out of Git for licence reasons.

| Area | Choice |
| ---- | ---- |
| Engine | Unreal Engine 5.8: Lumen, Nanite, Virtual Shadow Maps, TSR |
| Code | C++ for the game systems, Python for the city and build tools, Blueprint for prototypes |
| Reference machine | Windows 11, Ryzen 9 3900, RTX 3070 Ti, DirectX 12 |
| Earlier releases | macOS on Apple Silicon (Metal), versions 0.1 to 0.3 |
| Architecture decisions | [docs/adr](docs/adr/), 13 decision records |

All documentation, in English and French: **[docs/README.md](docs/README.md)**.

## Roadmap

The game comes before the image. The slice is delivered in three
milestones:

| When (order of magnitude) | What |
| ---- | ---- |
| End of November 2026 | **A.** Steal a car and flee the police, on PC, as an unedited video |
| Mid-December 2026 | **B.** Ten minutes of play that feel like GTA: traffic, passers-by who react, combat, wanted stars, a chase |
| End of January 2027 | **C.** Five real photo / game capture pairs that hold up |
| February 2027 | The northern Presqu'île, Bellecour to Terreaux |
| March 2027 | Mac port |
| Then | The story: thefts of Lyon specialities and getaways through the traboules |

Until then missions, menus, interiors, saves and audio are frozen. Assets
stay **free only** (Megascans, MetaHuman, City Sample, free Fab content,
home-made). Details: [roadmap](production/roadmap.md),
[dashboard](production/roadmap-status.md),
[game concept](design/gdd/game-concept.md).

## Play or build it

**The latest public build is [version 0.3.0 for macOS](https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.3.0)**
(Apple Silicon, 10 November 2026): steal a car and drive through traffic
from rue de la République to the Rhône. It is older than the PC work
described above. The next release will come with milestone B.
[Release notes](docs/releases/0.3.md), including how to open an unsigned app.

<details>
<summary><b>Build it yourself</b></summary>

Requirements: Unreal Engine 5.8, Visual Studio with the C++ tools (Windows)
or Xcode (Mac), [Git LFS](https://git-lfs.com/) and Python 3.

Some content is **not stored in Git**, for licence or size reasons:

| Content | How to get it |
| ---- | ---- |
| Epic's Game Animation Sample and City Sample content | Free on Fab, then [`tools/character/install_gasp_content.sh`](tools/character/README.md) |
| The rue de la République level | Generated from public data by [`tools/city`](docs/city/plan.md) and [`tools/level`](tools/level/README.md) |
| City data (OpenStreetMap tables) | Regenerated by [`tools/city`](docs/city/plan.md) (ODbL, no key needed) |
| Real brand logos and textures | Kept outside Git |

```zsh
git lfs install
git clone https://github.com/nic0lasm/Gran-Theft-Praline.git
```

Windows package: `powershell -ExecutionPolicy Bypass -File tools\build\package-win.ps1`.
macOS package and test: `tools/build/package-mac.sh` then
`tools/build/test-build-mac.sh`. Guide: [one-click build](docs/build/one-click-build.md).

</details>

<details>
<summary><b>Controls</b> (keyboard and gamepad)</summary>

| Action | Keyboard (AZERTY) | Keyboard (QWERTY) | Gamepad |
| ---- | ---- | ---- | ---- |
| Move | Z Q S D | W A S D | Left stick |
| Look around | Mouse | Mouse | Right stick |
| Sprint (hold) | Left Shift | Left Shift | A / Cross |
| Jump, vault | Space | Space | X / Square |
| First / third person | V | V | View / Share |
| Get in / out of a car | F | F | Y / Triangle |
| Accelerate / brake, reverse | Z / S | W / S | Right / left trigger |
| Steer | Q / D | A / D | Left stick |
| Handbrake | Space | Space | RB / R1 |

Full details: [player character](docs/gameplay/player-character.md),
[vehicles](docs/gameplay/vehicles.md), [combat](docs/gameplay/combat.md).

</details>

<details>
<summary><b>Contributing</b></summary>

Help is welcome, and you don't need to write code: point out a missing
shop or a wrong building height, or tell us how a build runs on your
machine. Open an [issue](https://github.com/nic0lasm/Gran-Theft-Praline/issues)
to start the conversation, especially before a large piece of work.

- Branches: `feat/<topic>` or `fix/<topic>`; `next` is pre-production,
  `main` is production; history stays linear.
- A change moves to `next`, then `main`, only once CI is green on that
  exact commit and the PC gate has passed.
- Code, commits and identifiers in English; documentation in English and
  French (`file.md` + `file.fr.md`).
- Every external asset gets a row in [assets/SOURCES.md](assets/SOURCES.md);
  large binaries go through Git LFS ([rules](docs/project/git-lfs.md)).

</details>

## Credits and licence

- Geographic data: contains data from **IGN** (BD TOPO, LiDAR HD, RGE ALTI)
  and **Métropole de Lyon** (Licence Ouverte / Etalab Open Licence), and
  **© OpenStreetMap contributors** ([ODbL](https://opendatacommons.org/licenses/odbl/)).
- Photos of the real city in this README: **Corentin Eustacchi**, Wikimedia
  Commons, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/),
  cropped and resized; the two image files keep that licence.
- **Epic Games**: Unreal Engine, the Game Animation Sample, City Sample and
  Lyra are used under the Unreal Engine EULA; they are not in this
  repository and not covered by its licence.
- Shop signs and brands belong to their owners and are not distributed
  here. No real people are depicted in the game, and no assets are taken
  from other games. Details: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md),
  [assets/SOURCES.md](assets/SOURCES.md).

The code and documentation in this repository are released under the
[MIT License](LICENSE). Third-party data and assets keep their own licences.

<p align="center"><i>Made in Lyon, with love for its streets.</i></p>
