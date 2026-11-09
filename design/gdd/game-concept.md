# Game Concept: Gran Theft Praline

*Created: 2026-11-09*
*Status: Draft, scope set by the project lead: functional and visual parity
with the current generation of open-world crime games, on a faithful Lyon.
Story, characters and tone come later.*
*French version: [game-concept.fr.md](game-concept.fr.md)*

---

## Elevator Pitch

> A photorealistic open-world crime game in present-day Lyon, with the
> handling, weapons, enterable shops and buildings and depth of interaction
> players expect from the best of the genre (Grand Theft Auto VI is the
> benchmark), on a map rebuilt as faithfully as possible from the real city.

The story, the protagonist and the tone are **not defined yet**. They will be
written once the playable base exists. This document defines that base: what
the player can do, how it should feel and look, and which part of Lyon comes
first.

---

## Core Identity

| Aspect | Detail |
| ---- | ---- |
| **Genre** | Open-world crime action-adventure, third person (first person optional later) |
| **Platform** | macOS Apple Silicon (development target), Windows PC next |
| **Player Count** | Single-player |
| **Session Length** | 30 to 120 minutes |
| **Monetization** | Premium, none yet |
| **Estimated Scope** | Large (vertical slice 9 to 12 months, full vision several years, solo developer with generative tools) |
| **Benchmark** | Grand Theft Auto VI (functional and visual reference), Grand Theft Auto V (systems baseline), Red Dead Redemption 2 (interaction depth, animation) |

---

## Core Fantasy

Lyon as a living, playable city: you can walk, drive, fight and enter places
in streets you recognise, and the city reacts like a real one.

---

## Unique Hook

The genre's reference experience, **and also** a real European city rebuilt
street by street: the Presqu'île, the rivers and bridges, the hills, the
shops and the light of Lyon, instead of a fictional American city.

---

## Benchmark Method

The benchmark is **Grand Theft Auto VI**: what it shows publicly (trailers,
official material) and, once it is released, the game itself. Where a
feature is not documented yet, **Grand Theft Auto V** and **Red Dead
Redemption 2** set the minimum. Each feature below has a parity target; a
parity review is run after release and at each milestone.

We reproduce **functions and quality levels, never content**: no names,
characters, logos, map layouts, missions, music, UI art or assets from those
games.

---

## Feature Parity Targets

Priority: **P0** = vertical slice, **P1** = alpha, **P2** = full vision.

### 1. On-foot handling and character
| Feature | Target | Priority |
| ---- | ---- | ---- |
| Walk, jog, sprint, crouch, jump, vault, climb low obstacles | Motion-matched, weighty, responsive | P0 |
| Camera | Third person with shoulder swap; first person | P0 / P1 |
| Physical reactions | Stumble, fall, ragdoll blend, push through crowds | P0 |
| Swimming in the Saône and Rhône | Swim, dive, climb out on quays | P1 |
| Stamina, health, armour | Readable without heavy HUD | P0 |
| Contextual interactions | Doors, sit, lean, pick up, open containers | P0 |
| Appearance | Clothes, hair, accessories bought in shops | P1 |

### 2. Driving and vehicles
| Feature | Target | Priority |
| ---- | ---- | ---- |
| Cars with physics-based handling | Weight transfer, grip, braking, per-class feel (city car, saloon, van, sports) | P0 |
| Two-wheelers | Scooter, motorbike, bicycle | P0 / P1 |
| Damage | Deformation, broken glass, lights, punctures, smoke | P0 |
| Enter and exit | Animated, steal parked cars, carjack | P0 |
| In-car | Radio, indicators, horn, camera modes | P1 |
| Public transport and boats | Buses, trams, river boats as vehicles or scenery | P2 |
| Garages, customisation | Store and modify vehicles | P1 |

### 3. Weapons and combat
| Feature | Target | Priority |
| ---- | ---- | ---- |
| Melee | Punches, kicks, takedowns | P0 |
| Firearms | Pistol, SMG, shotgun, rifle; aim, recoil, reload | P0 |
| Cover system | Snap to cover, blind fire, cover-to-cover | P0 |
| Weapon wheel and inventory | Quick selection, limited carry | P0 |
| Hit reactions and damage model | Location-based, readable | P0 |
| Throwables | Grenades, molotov | P1 |
| Weapon shop | Buy weapons, ammo, upgrades (fictional brands) | P1 |

### 4. Police and wanted system
| Feature | Target | Priority |
| ---- | ---- | ---- |
| Witnesses and reporting | Crimes are reported, not detected by magic | P0 |
| Heat levels 1 to 5 | Patrols, roadblocks, spike strips, helicopter, special units | P0 (1 to 3) / P1 |
| Search and escape | Line of sight, search zone, hiding, changing vehicle | P0 |
| Arrest and death | Penalty in money and equipment, quick restart | P0 |
| Fictional French police force | Vehicles, uniforms and procedures inspired by French policing, fictional insignia | P0 |

### 5. Interiors, shops and buildings
| Feature | Target | Priority |
| ---- | ---- | ---- |
| Enterable shops without loading screens | Bakery, café, newsagent, clothes, convenience store, weapon shop, garage | P0 (5 types) |
| Buying | Food, clothes, weapons, vehicles | P0 / P1 |
| Store robbery | Threaten the clerk, till, alarm, heat | P0 |
| Enterable buildings | Lobbies, stairwells, courtyards, some flats, car parks, rooftops | P0 (selection) / P1 |
| Safehouses | Save, change clothes, store weapons and vehicles | P0 |
| Bars, restaurants, cinema, gym | Ambient and mini-activities | P1 / P2 |

### 6. Living city
| Feature | Target | Priority |
| ---- | ---- | ---- |
| Crowds | Dense pedestrians with schedules, reactions to violence, conversations | P0 |
| Traffic | Lane-aware traffic, lights, pedestrians crossing, accidents, reactions to the player | P0 |
| Time and weather | Full day/night cycle, rain, fog, wet surfaces | P0 |
| Ambient events | Street events, arguments, petty crime, deliveries | P1 |
| Wildlife | Pigeons, dogs, river birds | P2 |

### 7. Systems and interface
| Feature | Target | Priority |
| ---- | ---- | ---- |
| Phone | Contacts, map, messages, in-game social network | P1 |
| Map and GPS | Real street names, route guidance | P0 |
| Money and economy | Earn, spend, steal | P0 |
| Minimal HUD | Mini-map, health, heat; hideable | P0 |
| Save and load | Safehouses, autosave | P0 |
| Accessibility | Subtitles, remapping, aim assist, colour options | P0 |

### 8. Graphics
| Area | Target | Priority |
| ---- | ---- | ---- |
| Lighting | Lumen global illumination and reflections, physically based sky and sun position for Lyon's latitude | P0 |
| Geometry | Nanite for buildings, streets and props; photogrammetry-grade materials | P0 |
| Characters | MetaHuman-level faces and skin, cloth simulation | P0 |
| Effects | Wet surfaces, puddles, volumetric fog, rain, smoke, fire, glass | P0 |
| Vehicles | Paint, dirt, damage, interior detail | P0 |
| Density | Signs, street furniture, litter, cafe terraces, parked scooters | P0 |
| Post-process | Filmic, restrained, natural lenses | P0 |

The **frame-rate and resolution target** on the development Mac is fixed by
an ADR before production, then never relaxed without the project lead's
decision.

---

## Lyon Fidelity

- **Layout**: street network, building footprints and heights, terrain,
  rivers and quays from IGN (LiDAR HD, BD TOPO), Grand Lyon open data (3D
  buildings) and OpenStreetMap. Credits shown in game.
- **Landmarks**: rebuilt at real scale from public references (Place
  Bellecour, Hôtel de Ville, Opéra, bridges, Fourvière on the skyline).
- **Façades and street level**: per-street materials and colours, real shops
  in the right places with their **real signs and brands**, reproduced from
  field photos (Nico's decision, 2026-11-09, non-commercial use; see the
  [art bible](../../docs/art/art-bible.md), section 3.5); **faces stay
  fictional**.
- **Interiors**: not in open data; rebuilt as plausible fiction.
- **Visual reference only**: Google Photorealistic 3D Tiles and field photos
  guide modelling but are never shipped.
- **Tools to reproduce the city** (geodata pipeline, Cesium for Unreal,
  procedural façades, photogrammetry, generative tools) are evaluated in a
  separate study.

---

## Core Loop (base, without story)

### Moment-to-Moment (30 seconds)
Moving through Lyon on foot or by vehicle, fighting, fleeing or entering a
place.

### Short-Term (5 to 15 minutes)
A self-contained activity: rob a shop, steal a car to order, escape a heat
level, buy and equip, deliver something across the city.

### Session-Level (30 to 120 minutes)
A chain of activities and free roam that ends at a safehouse with more money,
better gear and new places unlocked. Story missions will slot in here later.

### Long-Term Progression
Money, weapons, vehicles, clothing, safehouses and, later, the story and new
districts.

---

## Game Pillars

### Pillar 1: Genre-reference handling
Moving, driving and shooting must feel as good as the benchmark; feel is
tuned before content is added.

*Design test*: debating a new feature or polishing an existing verb, we polish
the verb until it reaches parity.

### Pillar 2: Lyon as it is
The map is the real city; we do not move, merge or invent streets in the
playable area.

*Design test*: debating a convenient fictional layout or the real one, we keep
the real one and adapt the gameplay.

### Pillar 3: Photorealism you can touch
Every surface, light and sound should pass for a photo or a recording; what
the player can see in a shop window, they should mostly be able to enter or
interact with.

*Design test*: debating a larger area or a denser, more interactive one, we
choose density.

### Pillar 4: Systems before scripts
Police, crowds, traffic and shops work as systems that react to the player
everywhere, so that the story added later sits on a living city.

*Design test*: debating a scripted set piece or a system that produces the
same moment, we build the system.

### Anti-Pillars (What This Game Is NOT)

- **NOT a copy of another game's content**: we match functions and quality,
  never names, characters, map, missions or assets.
- **NOT real people**: places and shop signs are faithful (real brands
  included, non-commercial use), faces are fictional.
- **NOT the whole city at once**: one district at full quality first, then
  district by district.
- **NOT multiplayer** for now: the budget goes to the city and the systems.
- **NOT story-first yet**: the story is written with the project lead once
  the base is playable.

---

## Vertical Slice: City Scope

**District: northern Presqu'île, from Place Bellecour to Place
des Terreaux, between the Saône and Rhône quays** (about 1.5 km²).
*Decided by the project lead on 2026-11-09.*

- Why: the densest shopping streets (enterable shop types for P0), wide
  streets and quays for driving and chases, two rivers with bridges, iconic
  landmarks (Bellecour, Hôtel de Ville, Opéra), excellent open-data
  coverage, and flat terrain that keeps the first technical risks low.
- Alternatives: the Croix-Rousse slopes (character, stairs, but steep and
  narrow for driving), Vieux-Lyon and Fourvière (heritage but largely
  pedestrian), Part-Dieu (modern towers and boulevards, less iconic).
- Content target: the full district at final quality, about 30 enterable
  shops across 5 types, 3 safehouses, 10 enterable buildings, 6 vehicle
  types, 6 weapons, heat levels 1 to 3, 6 sandbox activities.

---

## Technical Considerations

| Consideration | Assessment |
| ---- | ---- |
| **Engine** | Unreal Engine 5.8 (decided), C++ with Blueprint prototyping, Lumen, Nanite, World Partition, Mass for crowds and traffic, Chaos Vehicles |
| **Key Technical Challenges** | City reconstruction from geodata, seamless interiors and streaming, crowd and traffic density, police AI, Apple Silicon performance |
| **Art Style** | 3D photorealistic |
| **Art Pipeline Complexity** | Very high |
| **Audio Needs** | High (city ambience, vehicles, weapons, radio) |
| **Networking** | None |
| **Procedural Systems** | Building shells from geodata, façade variation, crowd and traffic population |

---

## Risks and Open Questions

- **Scope**: parity with a studio of thousands is out of reach in full; the
  answer is one district at full quality, P0 only, before anything else.
- **Performance**: photorealism plus crowds and traffic on Apple Silicon; the
  performance budget ADR comes before production.
- **Benchmark availability**: part of the benchmark is known only from public
  material; parity targets are reviewed once the game is out.
- **Interiors**: every enterable place is hand-built content; the shop types
  share kits to keep the cost down.
- **Open question**: which tools reproduce Lyon best for our budget? Answered
  by the separate city-reproduction study.

---

## MVP Definition

**Core hypothesis**: our handling (on foot, driving, shooting) reaches the
benchmark's feel on a real Lyon street block.

**Required for MVP**:
1. One greybox block of the chosen district built from real geodata.
2. On-foot movement and camera at target feel.
3. One car with physics-based handling and damage.
4. One pistol with cover and melee.
5. One enterable shop that can be robbed, and heat levels 1 and 2.

**Explicitly NOT in MVP**: final art, crowds and traffic at density, story,
phone, customisation.

### Scope Tiers

| Tier | Content | Features | Timeline |
| ---- | ---- | ---- | ---- |
| **MVP** | One greybox block | Handling, one car, one gun, one shop, heat 1-2 | 6 to 8 weeks |
| **Vertical Slice** | Chosen district at final quality | All P0 features | 9 to 12 months |
| **Alpha** | Central Lyon, both banks | P0 and P1 features, story begins | Several years |
| **Full Vision** | Greater central Lyon | All features, full story | To be planned |

---

## Next Steps

- [x] Decision: vertical slice district (northern Presqu'île, 2026-11-09)
- [ ] Study: tools to reproduce Lyon (geodata, Cesium for Unreal, procedural façades, photogrammetry)
- [ ] `/art-bible`: photorealistic visual identity for Lyon
- [ ] Performance budget ADR
- [ ] `/map-systems`: turn the parity tables into systems and priorities
- [ ] `/prototype handling`: MVP above
- [ ] Story, characters and tone with the project lead, once the base is playable
