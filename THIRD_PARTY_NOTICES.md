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

## Attributions required (CC BY 4.0)

Fast-food reference interior. Licensed under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/), unchanged except for level of detail and texture size:

- "Air Duct Kit" by Gerulf Doesinger (https://www.fab.com/listings/bdd899c9-04ea-428f-847b-d1012854803c), CC BY 4.0
- "Cash register" by Wnoz (https://www.fab.com/listings/6ac6b752-88c5-42e9-88ce-2c0ee9e839e9), CC BY 4.0
- "Compostable Burger Box RAWscan" by Spogna (https://www.fab.com/listings/9d5f4c5a-1877-498b-a4d6-8491f41d94f5), CC BY 4.0
- "Double Metal Shelving" by Jimbogies (https://www.fab.com/listings/0ae68265-4aa1-4538-9213-051e260448da), CC BY 4.0
- "Fridge" by Gerardo Justel (https://www.fab.com/listings/52e9035e-2a20-45ce-8479-6b269fd91aaf), CC BY 4.0
- "Gourmet Cheeseburger and Crispy Fries" by Zarrar Abbas (https://www.fab.com/listings/b8b57837-b085-41b0-a7cd-e1c9b61514ce), CC BY 4.0
- "Heavy Duty Fridge" by Gerardo Justel (https://www.fab.com/listings/d8d2bbd8-56e1-4644-b9ea-aa4dce9737c0), CC BY 4.0
- "Lockers" by Normal Map Enjoyer (https://www.fab.com/listings/a4bb96b2-ff10-4d27-860c-71a1c60f377b), CC BY 4.0
- "Menu Board for Restaurant" by ZendeScan (https://www.fab.com/listings/7b098fbb-22e6-48bb-8945-06a8a7ae459c), CC BY 4.0
- "Metal Food Tray" by Kami Rapacz (https://www.fab.com/listings/fbebe61b-eb0a-4d69-8f5c-6f3394928f69), CC BY 4.0
- "Office Binder" by AshenCat (https://www.fab.com/listings/e6d48397-4207-4d7e-94e5-7acbfd9e1877), CC BY 4.0
- "Office Pack Vol.1" by MobWModels (https://www.fab.com/listings/ce136033-3265-46d3-ac4d-fdbd5c9d0462), CC BY 4.0
- "Paper Bag Scan MEDPOLY" by sixonetwo (https://www.fab.com/listings/5c16c3a9-3f06-454e-9009-1cd788824f2b), CC BY 4.0
- "Realistic Fire Extinguisher" by RED2000 (https://www.fab.com/listings/eb018466-9bb6-46d9-9e43-04f5c9d10b2c), CC BY 4.0
- "Set of 3 Self-Service Kiosks" by ahmed kanana (https://www.fab.com/listings/6f0ebc82-dbda-4c39-a244-c36dedb7f71c), CC BY 4.0

The full list is kept in `assets/SOURCES.md`.

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
