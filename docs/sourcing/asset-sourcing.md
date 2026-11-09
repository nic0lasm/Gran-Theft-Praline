# Asset and tool sourcing survey

*French version: [asset-sourcing.fr.md](asset-sourcing.fr.md)*
*Surveyed: 2026-11-09. Prices and licences change often: re-read the licence
on the listing at download time and log it in [`assets/SOURCES.md`](../../assets/SOURCES.md).*
*Used by: [roadmap](../../production/roadmap.md), [rendering strategy](../rendering/rendering-strategy.md).*

Goal: list what already exists so that we do not rebuild it, and say for each
item whether we use it.

**Verdicts**

- **Recommended**: we build on it.
- **To test**: promising; a short spike decides, with measurements on the M1 Pro.
- **Rejected**: not used, with the reason.

**Licence rules that apply to every item**: original game, no content from
existing games, no real people (rules 18 to 21 of the working rules). An
asset that shows a real vehicle make or a real face is reworked or rejected.
Real shop signs and brands are allowed since 2026-11-09 (non-commercial use),
reproduced from field photos and kept out of the repo
([art bible](../art/art-bible.md), section 3.5).

---

## 1. Headline findings

1. **The whole Quixel Megascans library became free on Fab on 2026-11-05**
   (listings updated until 2026-11-12), with new terms that also allow AI
   training. This removes our biggest content cost for ground, surfaces,
   props and vegetation. Claim the assets on Fab now and log the licence.
2. **Epic's free samples cover most of the gameplay foundations**: the Game
   Animation Sample (motion matching, updated for 5.8 on 2026-08-12), Lyra
   (Gameplay Ability System and weapons patterns), Chaos Vehicles, StateTree,
   Smart Objects, Mass, PCG. No all-in-one "crime open world" framework is
   worth buying.
3. **No paid kit reproduces Lyon.** Paris Haussmann kits exist (KitBash3D
   Paris, PolySphere City Core Paris) and are close to the Presqu'île's
   19th-century façades, but the Lyon façade kit from our own photos stays
   the plan. A Paris kit can speed up blockout and secondary streets.
4. **France has no PLATEAU equivalent**, but IGN BD TOPO + LiDAR HD + Grand
   Lyon open data + OpenStreetMap together cover footprints, heights,
   terrain, roads and shop locations. Our `extract_bbox.py` already turns
   BD TOPO into a greybox.
5. **The main photogrammetry tool (RealityScan) is Windows-only.** On the Mac:
   Apple Object Capture (free) or Metashape Standard (paid).
6. **City Sample is a study reference, not a base**: American city, systems
   written for one demo (MassTraffic is not an official engine plugin and is
   no longer developed), heavy, historically Windows-first.

---

## 2. Gameplay frameworks and plugins

### 2.1 Epic (free, part of the engine or official samples)

| Item | What it gives | Licence, price | UE 5.8 / macOS | Verdict | Why |
| ---- | ---- | ---- | ---- | ---- | ---- |
| [Game Animation Sample](https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8) | Motion matching locomotion, traversal (vault, climb), 500+ animations, updated for 5.8 | Free, UE EULA | 5.8 (2026-08-12); runs on Mac | **Recommended** | Our on-foot base: best-in-class locomotion for free |
| CharacterMovementComponent | Stable character movement | Engine | Yes | **Recommended** | Stable path for the MVP |
| [Mover](https://dev.epicgames.com/documentation/en-us/unreal-engine/mover-in-unreal-engine) | Next-generation movement (modes, modifiers) | Engine plugin | Experimental in 5.8 (beta missed) | **To test** after M1 | Meant to replace the CharacterMovementComponent, but APIs still change |
| Gameplay Ability System | Abilities, attributes (health, armour), effects, cues | Engine | Yes | **Recommended** | Standard for weapons, damage, status effects |
| [Lyra Starter Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) | Shooter on GAS, weapons, inventory, modular game features | Free, updated each release | 5.8 (a GAS warning reported in 5.8.1) | **To test** as reading reference | Copy patterns (weapons, inventory, input), not the project: multiplayer arena structure |
| Enhanced Input, Common UI, UMG | Input remapping, menus, HUD | Engine | Yes | **Recommended** | Accessibility requirements (remapping) |
| StateTree, AI Perception, Smart Objects | NPC and police behaviours, sight and hearing, usable objects | Engine | Yes | **Recommended** | Basis of police, shopkeepers, pedestrians |
| [Chaos Vehicles](https://dev.epicgames.com/documentation/en-us/unreal-engine/vehicles-in-unreal-engine) | Wheeled vehicle physics | Engine plugin | Flagged experimental in 5.8.2; AI vehicles broken with Physics Prediction (fix announced) | **Recommended** | Only maintained option; keep Physics Prediction off (single-player) |
| Chaos Destruction, Geometry Collections | Breakable glass, props | Engine | Yes | **To test** | Cost on the M1 Pro to measure |
| Mass Entity, MassCrowd | Thousands of lightweight agents | Engine; overhauled in 5.8 | Yes, little documentation | **To test** (Phase 2A) | Needed for density; isolate behind our interfaces |
| ZoneGraph | Lane graph for crowds and traffic | Engine plugin | In maintenance mode | **To test** | Usable but not developed further; we may generate lanes from geodata ourselves |
| [City Sample](https://dev.epicgames.com/documentation/unreal-engine/city-sample-project-unreal-engine-demonstration) and MassTraffic | Full city demo: Mass crowds, traffic, parking | Free, "UE-only" content | Heavy (traffic level alone about 18 GB), hard to port, City Sample Crowds historically Windows-only | **Rejected** as base, kept as study | American look; MassTraffic is "not an official engine plugin" and no longer developed |
| PCG | Rule-based placement; 5.8 adds manual edits on generated output and a building and street sample graph | Engine | Yes | **Recommended** | Dressing, façade module placement, vegetation |
| World Partition, HLOD, Level Instances, data layers | Streaming the district, seamless interiors | Engine | Yes | **Recommended** | Base of the open world and of enterable shops |
| Mesh Terrain | New terrain type tied to PCG | Engine | Experimental | **Rejected** for now | Flat Presqu'île; revisit for Fourvière and Croix-Rousse |
| Water plugin | Rivers, buoyancy | Engine | Yes | **To test** | Saône and Rhône surfaces, swimming (P1) |
| MetaSounds, Audio Modulation, Convolution reverb | Procedural audio, mix | Engine | Yes | **Recommended** | Vehicles, weapons, street reverb |
| SunPosition, Sky Atmosphere, Volumetric Clouds | Real sun for Lyon, sky, weather | Engine | Yes | **Recommended** | Already in ADR-0001 |

### 2.2 Third-party (Fab and others)

| Item | What it gives | Licence, price | UE 5.8 / macOS | Verdict | Why |
| ---- | ---- | ---- | ---- | ---- | ---- |
| [City Traffic Pro](https://www.fab.com/listings/b4aedd9f-2ca9-499d-b283-0d9864520a0d) (lyoshko) | Spline traffic AI: lanes, lane changes, intersections, roundabouts | Fab, paid (price on listing) | UE5, version and Mac binaries to check | **To test** in Phase 2A | Could save weeks on traffic; decided against our own lanes from geodata |
| [Foothold](https://forums.unrealengine.com/t/gregorigin-foothold-mover-gasp-traversal-moves/2841979) | Climb, ledge, ladders, mantle on Mover | Fab, paid | 5.8 Mover; API changes between versions | **To test** after Mover | Only if we move to Mover |
| [Pedestrian AI System](https://elsewaregames.itch.io/pedestrian-ai-system) | Spline crowds, traffic lights | Paid | Demos on 5.3 | **Rejected** | Old version, Mass gives more density |
| Drivable Cars: Basic Vehicle System | Blueprint car framework | Fab, damage sold separately | Blueprint only | **Rejected** | Weaker than Chaos plus our code; "GTA-style" marketing |
| Mega Open World pack | All-in-one Blueprint open world, wanted system | Fab, paid | To check | **Rejected** | Lock-in, Blueprint-only, quality unknown; we need our own police system |
| Police AI (legacy marketplace) | Police by wanted level | Old marketplace | UE4 era | **Rejected** | Unmaintained |
| Inventory and weapon-wheel plugins | Inventory UI and data | Various | Various | **Rejected** | Our needs are small (limited carry, weapon wheel): Lyra patterns + GAS suffice |

---

## 3. Content: environments, props, vehicles, characters

| Item | What it gives | Licence, price | UE 5.8 / macOS | Verdict | Why |
| ---- | ---- | ---- | ---- | ---- | ---- |
| [Quixel Megascans on Fab](https://quixel.com/) | Scanned surfaces, props, plants (Megaplants) | **Free since 2026-11-05** (Fab listings updated until 2026-11-12); new terms allow AI training; older downloads keep their old licence | Yes | **Recommended** | Ground, stone, asphalt, cobbles, vegetation, generic props at scan quality |
| Fab Standard Licence assets | Everything else on Fab | Free to paid; usable in any engine | Depends | **To test** per asset | Read the licence of each listing |
| [KitBash3D Paris](https://digitalproduction.com/2026/09/11/kitbash3d-paris-city-break-no-airport/) | Haussmann blocks, landmarks, street props; Nanite, collisions, Packed Level Instances | Paid (KitBash3D licence) | Unreal path; Nanite meshes need LOD check on the M1 Pro | **To test** (purchase = decision) | Closest existing style to rue de la République; useful for blockout and secondary streets, never as Lyon landmarks |
| [PolySphere City Core Paris](https://forums.unrealengine.com/t/polysphere-studio-city-core-paris/2675791) | Parisian district pack | Fab, paid | To check | **To test** | Alternative to KitBash3D |
| [Modular Paris Haussmann](https://cgtrader.com/3d-models/exterior/house/modular-paris-haussmann) (CGTrader) | Haussmann façade kit | CGTrader royalty-free, paid | FBX, not UE-ready | **To test** | Reference for module cuts |
| Timeless French villas | Rural French houses | Marketplace | UE5 | **Rejected** | Wrong typology |
| [Police Generic Vehicles 3D Pack](https://www.fab.com/listings/a9e12e7c-fb9f-4799-b172-9b47c0939ec3) | Unbranded police sedan, pickup, truck, motorbike | Fab, paid | To check | **To test** | Generic designs fit the "no real makes" rule; need French-style fictional livery |
| Other Fab car packs (Fleetway, 1980s Cars, Sports and Police Cars) | Drivable cars | Fab | Stylised, PhysX-era or 1980s | **Rejected** | Wrong style or outdated physics |
| French traffic signs (Fab, European collections) | Road signs, markings | Fab | To check | **To test** | Signage is public-domain-style but check the pack licence |
| [MetaHuman](https://www.metahuman.com/license) | Photoreal characters, in-editor creator (web app wound down), crowd LODs | Free below USD 1 M revenue under UE terms | 5.8 editor creator on macOS | **Recommended** | Population, police, shopkeepers; fictional faces only |
| City Sample vehicles and buildings | US cars and towers | UE-only content | Heavy | **Rejected** | American look; real-make-like designs |
| [Poly Haven](https://polyhaven.com), [ambientCG](https://ambientcg.com) | HDRIs, textures, some models | CC0 | Any | **Recommended** | Look-dev HDRIs, extra materials |
| Sketchfab downloadable models | Varied | CC BY or paid, case by case | Any | **To test** case by case | Quality and licence vary |

---

## 4. Geodata and city reconstruction tools for Lyon

| Item | What it gives | Licence, price | UE 5.8 / macOS | Verdict | Why |
| ---- | ---- | ---- | ---- | ---- | ---- |
| IGN BD TOPO (WFS Géoplateforme) | Building footprints, heights, ground altitude, roads, hydrography | Licence Ouverte 2.0, free | Our Python script, any OS | **Recommended** (in use) | Backbone of the greybox; `tools/geo-pipeline/extract_bbox.py` |
| [IGN LiDAR HD](https://www.data.gouv.fr/datasets/mns-lidar-hd/) | Classified point clouds (ground, buildings, vegetation, bridges), DTM and DSM at 0.5 m | Licence Ouverte, free | PDAL / Python, any OS | **Recommended** | Terrain, quays, embankments, roof heights, tree positions; check Lyon tile availability on the IGN map |
| IGN RGE ALTI, ortho-photos (BD ORTHO) | Elevation, aerial images | Licence Ouverte | Any | **Recommended** | Ground texture reference, minimap base |
| [Grand Lyon textured 3D models (CityGML)](https://www.data.gouv.fr/datasets/611621d83eed0bb631f343d0) | LOD2 buildings with roof shapes and textures (2012, 2015, 2018 editions) | Licence Ouverte, free | CityGML; conversion needed | **To test** | Roof shapes better than extruded footprints; old textures not usable in game |
| [Grand Lyon photomesh 2023](https://www.data.gouv.fr/datasets/6752312729bbdda4ba8f637f) | 3D Tiles mesh from summer 2023 aerial photos, 602 km² | Open data, licence to confirm | 3D Tiles (Cesium) | **Recommended** as reference only | Baked light, low street-level detail (ADR-0001); great for checking proportions |
| [LIRIS py3dtilers / cityGMLto3DTiles](https://datasets.liris.cnrs.fr/node/59) | Open tools by a Lyon lab to convert Grand Lyon CityGML | Open source | Python | **To test** | Lyon-specific conversion already solved |
| OpenStreetMap (Overpass API) | Street names, shop types, street furniture, trees, traffic lights, tram lines | ODbL, credit and share-alike on the database | Any | **Recommended** | Shop placement, map and GPS names, traffic light positions |
| [Blosm](https://github.com/vvoovv/blosm) (Blender) | Imports OSM, terrain, Google 3D Tiles into Blender | Free GPL base; premium edition about USD 18 (older price) | Blender on macOS | **Recommended** (free base) | Fast OSM blockouts in Blender |
| [Cesium for Unreal](https://cesium.com/learn/cesium-unreal/) | Streams 3D Tiles (photomesh, Google 3D Tiles) in the editor | Free plugin; Cesium ion tiers | 5.8 macOS CI work visible; Mac issues reported on 5.6 | **To test**, editor only | Visual reference inside Unreal; not a runtime dependency |
| Google Photorealistic 3D Tiles | Photoreal city mesh | Google Maps Platform terms, billing account | Via Cesium or Blosm | **Rejected** for game, reference only | Terms forbid shipping; already rule 21 |
| [Panoramax](https://panoramax.fr), Mapillary | Street-level photos | Open licences (check per instance) | Web | **Recommended** as reference | Façade and shopfront reference without a field trip |
| Houdini (City Sample workflow) | Procedural city generation | Paid (Indie licence) | macOS | **Rejected** | Cost and learning curve; PCG and Blender cover our needs |

---

## 5. Photogrammetry and Gaussian splatting

| Item | What it gives | Licence, price | macOS | Verdict | Why |
| ---- | ---- | ---- | ---- | ---- | ---- |
| [RealityScan 2.x](https://www.realityscan.com/) (ex-RealityCapture) | Desktop photogrammetry | Free below USD 1 M revenue | **Windows only** | **Rejected** on our Mac | Would need a PC |
| RealityScan mobile app | Phone scans of small objects | Free | iOS | **To test** | Quick prop scans (bollards, bins) |
| Apple Object Capture (macOS API, apps built on it) | Photogrammetry on Apple Silicon | Free API; apps vary | Yes | **To test** | Native on our Mac; quality to compare |
| Agisoft Metashape Standard | Desktop photogrammetry | About USD 179 perpetual | macOS build exists; Apple Silicon performance to check | **To test** (purchase = decision) | If Object Capture is not enough |
| Gaussian splatting plugins (Luma UE plugin, XScene, NanoGS, SplatRenderer…) | Splat rendering in Unreal | Free to paid | Luma lists Mac (experimental); several need DX12 or 5.6-5.7 only | **Rejected** for game | Baked light, no collision (ADR-0001); maybe distant skyline later |

---

## 6. Audio

| Item | What it gives | Licence, price | Verdict | Why |
| ---- | ---- | ---- | ---- | ---- |
| MetaSounds | Procedural engine, weapon and ambience sounds | Engine | **Recommended** | Built in |
| [Sonniss GDC Game Audio Bundles](https://sonniss.com/gameaudiogdc) (2015-2026) | Hundreds of GB of professional sound effects | Free, royalty-free commercial use; 2024 bundle forbids AI training | **Recommended** | Vehicles, weapons, impacts, ambiences |
| Freesound | Community sounds | CC0, CC BY, CC BY-NC per sound | **To test** | Filter CC0 and CC BY only |
| Field recordings in Lyon | Real ambiences: quays, trams, markets, church bells | Ours | **Recommended** | Identity of the city; needs a recorder |
| Radio music | Licensed or commissioned tracks | To define | Later (P1) | Not in P0 |

---

## 7. Generative and modelling tools

| Tool | Place in the pipeline | Licence point | Verdict |
| ---- | ---- | ---- | ---- |
| Flux (Black Forest Labs) | Signage, posters, menus, graffiti, decals, mood boards | API outputs reported commercially usable; read the current BFL API terms; open "dev" weights are non-commercial | **Recommended** upstream only (ADR-0001) |
| GPT Image | Same as Flux, text-heavy signage | Output rights assigned to the user under the provider terms | **Recommended** upstream only |
| [Tripo](https://www.tripo3d.ai/help/privacy-policy/how-to-use-tripo-models-commercially) | Prop and vehicle blockouts, background clutter after cleanup | **Free plan: Tripo keeps the rights** (ToS 5.2.1); commercial use needs a paid plan (about USD 20 a month) at generation time | **Recommended** on a paid plan only |
| Blender | Façade kit, landmarks, vehicles, cleanup of every generated mesh | Free, GPL (outputs are ours) | **Recommended** |
| Image editor | Cleanup of generated signage | Any | **Recommended** |

Every generated asset gets a row in `assets/SOURCES.md` with tool, plan,
prompt and date.

---

## 8. Recommended stack in one table

| Need | Use |
| ---- | ---- |
| On foot | Game Animation Sample + CharacterMovementComponent (Mover later) |
| Abilities, weapons, damage | Gameplay Ability System, Lyra patterns |
| Vehicles | Chaos Vehicles, our tuning |
| NPC, police | StateTree, AI Perception, Smart Objects, our wanted system |
| Crowds, traffic | Mass (to test), lanes from our geodata; City Traffic Pro as a candidate |
| World | World Partition, HLOD, data layers, PCG |
| City layout | BD TOPO + LiDAR HD + OSM through our pipeline; Grand Lyon CityGML for roofs |
| Surfaces, props, plants | Megascans (free) + Poly Haven / ambientCG |
| Façades | Our Lyon kit from photos; a Paris kit possibly for blockout |
| Characters | MetaHuman |
| Audio | MetaSounds + Sonniss + Lyon field recordings |
| Signage and blockouts | Flux, GPT Image, Tripo (paid) then Blender |

## 9. Purchases to decide

**Decided by the project lead on 2026-11-09: small budget until the MVP.**
A paid Tripo plan (about USD 20 a month) for any Tripo output meant to ship;
Metashape only if Apple Object Capture falls short; no large pack before a
measured spike in Phase 2. Gameplay foundation, also decided on 2026-11-09:
**free Epic building blocks** (section 8), police and shops written in-house.

Candidates:

| Item | Approximate cost | When |
| ---- | ---- | ---- |
| Tripo paid plan | about USD 20 a month | As soon as a Tripo output is meant to ship |
| Metashape Standard | about USD 179 once | If Apple Object Capture falls short |
| Paris Haussmann kit (KitBash3D or PolySphere) | price on listing | Phase 2B, if it saves façade time |
| City Traffic Pro | price on listing | Phase 2A, after a spike |
| Audio recorder for field recordings | 100 to 250 € | Phase 2 |

## 10. Sources

- Megascans free on Fab: [Quixel](https://quixel.com/), [Creative Bloq](https://www.creativebloq.com/news/the-worlds-biggest-library-of-scanned-assets-for-3d-artists-is-now-free-and-it-seems-that-this-time-its-for-good), [GameFromScratch](https://gamefromscratch.com/entire-quixel-megascans-library-goes-free-again/)
- Game Animation Sample 5.8: [Epic tech blog](https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8)
- Mover status: [Epic docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/mover-in-unreal-engine), [forum](https://forums.unrealengine.com/t/any-roadmap-updates-on-mover/2833898)
- Chaos Vehicles in 5.8: [Fab listing noting experimental flag](https://www.fab.com/listings/bd39c4cf-beda-4c08-8f62-87baec9216b7), [AI vehicles and Physics Prediction](https://forums.unrealengine.com/t/ai-controlled-vehicles-stop-working-when-physics-prediction-is-enabled/2835577)
- UE 5.8 release (PCG, Mass, last UE5 major): [release notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes), [Mass 5.8 thread](https://forums.unrealengine.com/t/mass-entity-5-8-preview/2733180)
- City Sample and MassTraffic: [Epic docs](https://dev.epicgames.com/documentation/unreal-engine/city-sample-project-unreal-engine-demonstration), [porting thread](https://forums.unrealengine.com/t/how-to-port-matrix-ue5-crowds-traffic-parking-to-my-project/522935/2), [Mac crowds thread](https://forums.unrealengine.com/t/mac-support-of-ue5-assets-like-city-sample-crowds/681072)
- Lyra: [Epic docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
- Traffic and crowds plugins: [City Traffic Pro](https://forums.unrealengine.com/t/lyoshko-city-traffic-pro/2533887)
- Paris kits: [KitBash3D Paris](https://digitalproduction.com/2026/09/11/kitbash3d-paris-city-break-no-airport/), [PolySphere City Core Paris](https://forums.unrealengine.com/t/polysphere-studio-city-core-paris/2675791)
- Grand Lyon: [photomesh 2023](https://www.data.gouv.fr/datasets/6752312729bbdda4ba8f637f), [textured 3D models](https://www.data.gouv.fr/datasets/611621d83eed0bb631f343d0), [LIRIS datasets](https://datasets.liris.cnrs.fr/node/59)
- IGN LiDAR HD: [MNS LiDAR HD](https://www.data.gouv.fr/datasets/mns-lidar-hd/)
- Blosm: [GitHub](https://github.com/vvoovv/blosm)
- Cesium for Unreal on Mac: [M1 Pro thread](https://community.cesium.com/t/cesium-for-unreal-plugin-issues-on-mac-m1-pro-with-ue-5-6/41378)
- RealityScan: [official site](https://www.realityscan.com/)
- Gaussian splatting plugins: [Luma UE plugin](https://lumalabs.ai/ue), [radiancefields.com Unreal tag](https://radiancefields.com/tags/unreal-engine)
- Sonniss GDC 2026: [rekkerd.org](https://rekkerd.org/sonniss-releases-gdc-2026-game-audio-bundle/)
- MetaHuman licence: [metahuman.com](https://www.metahuman.com/license)
- Tripo commercial use: [Tripo help](https://www.tripo3d.ai/help/privacy-policy/how-to-use-tripo-models-commercially)
- Flux licences: [BFL legal](https://bfl.ai/legal/developer-terms-of-service)
