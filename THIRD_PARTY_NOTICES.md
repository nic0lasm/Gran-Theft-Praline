# Third-party notices

*French version: [THIRD_PARTY_NOTICES.fr.md](THIRD_PARTY_NOTICES.fr.md).*

Gran Theft Praline is a non-commercial fan project released under the
[MIT License](LICENSE) (code and documentation). Everything below keeps its
own licence. The same text is shown on the in-game credits screen. Every
asset's exact origin is in [assets/SOURCES.md](assets/SOURCES.md).

## Geographic data

| Data | Source | Licence | Credit line |
| ---- | ---- | ---- | ---- |
| BD TOPO (buildings, roads), LiDAR HD, RGE ALTI | IGN, via data.geopf.fr | Licence Ouverte Etalab 2.0 | Contains data from IGN |
| Street trees, kerbs, street furniture, administrative data | Métropole de Lyon (data.grandlyon.com) | Licence Ouverte / Etalab Open Licence | Contains data from Métropole de Lyon |
| Shops, benches, fountains, brand identifiers | OpenStreetMap | ODbL 1.0 | © OpenStreetMap contributors (https://www.openstreetmap.org/copyright) |
| Brand names and colours | Wikidata | CC0 | — |

Derived tables (buildings, lanes, shops) are produced from these sources by
`tools/city` and `tools/geo-pipeline`. Under the ODbL, the derived database is
available by running those tools (no key needed); the packaged game contains
only the generated geometry.

## Engine, animation and art

| Item | Source | Licence |
| ---- | ---- | ---- |
| Unreal Engine 5.8 | Epic Games | Unreal Engine EULA (not covered by the MIT licence) |
| Game Animation Sample (character animation) | Epic Games | Unreal Engine EULA |
| Quixel Megascans (surfaces, vegetation, props) | Fab (Epic Games) | Fab Standard licence, free assets only; the scans are not redistributable outside the game |
| Poly Haven, ambientCG (textures, HDRIs) | polyhaven.com, ambientcg.com | CC0 |
| Photos of the real city (README only, not in the game) | Corentin Eustacchi, Wikimedia Commons | CC BY-SA 4.0 |

Each row used by the build is detailed in `assets/SOURCES.md`; a row is added
there **before** any asset enters the project.

## Sound and fonts

| Item | Source | Licence |
| ---- | ---- | ---- |
| Sound effects, ambiences, voices of passers-by, radio | Generated procedurally by `tools/audio` (no recording, no real voice) | MIT (original) |
| Street ambience recording (`SW_Amb_Street_Day`), if kept | See `assets/SOURCES.md` | As listed there |
| Fonts | Engine default fonts (Roboto) | Apache 2.0 |

## Brands, people and other games

- Shop signs show the real shop names as lettering. Trademark artwork
  (logos) is **not** distributed in this repository and is only present in a
  privately shared build if the owner of the project decided so; see
  [docs/qa/brands-rule.md](docs/qa/brands-rule.md). All brands belong to their
  owners; this project is not affiliated with or endorsed by any of them.
- No real person is depicted. No asset is taken from another game, and no
  game files are used.

## Reporting a missing credit

Open an issue on the project repository. A missing credit is a defect and is
fixed before the next test build.
