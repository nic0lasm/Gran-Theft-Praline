# Catalog of assets to generate (Tripo)

*French version: [asset-catalog.fr.md](asset-catalog.fr.md). Roadmap: [roadmap.md](roadmap.md), section 5.*
*Updated by the generating thread: status of each model and actual credit cost.*

## Common rules

- **Generation** in Tripo Studio (web interface), in parallel, at
  maximum quality. Reference images (Flux, GPT Image or field
  photos) when text alone is not enough.
- **Master + light variant**: the original GLB is the master (kept out of
  Git, storage to be decided); a light variant (reduced polygons and
  textures) serves the M1 profile. See roadmap, section 1.
- **Faithful to Lyon**: Tripo generates objects without text or logos
  (`no text` in the prompt); real signs, brands and lettering are added
  afterwards from field photos ([art bible](../docs/art/art-bible.md),
  section 3.5). No object copied from an existing game. Prompts, scale,
  pivot and orientation follow the art bible's Tripo charter (section 8).
- **Real scale** in meters noted in the Size column; pivot on the ground.
- **A model's path**: ⬜ to generate → 🎲 generated (pick among the
  variants) → 🧹 cleaned up in Blender (scale, pivot, LODs, collisions) →
  ✅ integrated in Unreal and recorded in `assets/SOURCES.md`.
- **Exports**: `assets/tripo/<wave>/<id>.glb` (masters, out of Git);
  Unreal assets in `Content/Environment/Props/<Family>/<Asset>/`
  (art bible, section 7).

## Waves at a glance

| Wave | For version | Theme | Models | Status |
| ---- | ---- | ---- | ---- | ---- |
| 1 | 0.2 | Street furniture for rue de la République | 60 | ⬜ |
| 2 | 0.3 | Vehicles (drafts) and roadway objects | 30 | ⬜ |
| 3 | 0.4 | Fictional weapons and combat props | 25 | ⬜ |
| 4 | 0.5 | Grocery shop interior | 40 | ⬜ |
| 5 | 0.6 | Fictional police equipment, signage | 20 | ⬜ |
| 6 | 0.7 | Furniture for the squares (Bellecour, Terreaux, Jacobins, République) | 50 | ⬜ |
| 7 | 0.8 | Carried objects and pedestrian props | 30 | ⬜ |
| 8 | 0.9 | Road furniture (traffic lights, generic signs, barriers) | 40 | ⬜ |
| 9 | 0.10 | Weapons and equipment, continued | 30 | ⬜ |
| 10-11 | 0.11 | Interiors per shop type (clothing, gun shop, garage, café, safehouses) | 120 | ⬜ |
| 12 | 0.13 | Two-wheelers and activity furniture | 30 | ⬜ |
| 13 | 0.14 | Façade and monument ornaments | 60 | ⬜ |
| 14 | 0.15 | Potted vegetation, shopfronts, small street details | 80 | ⬜ |
| 15 | 0.16 | Final car bodies and character accessories | 40 | ⬜ |
| 16-17 | 0.17 | Final interior furniture (30 shops, 10 buildings, 3 safehouses) | 150 | ⬜ |
| | | **Total** | **≈ 805** | |

Waves can be generated **ahead** of their version: only the
order matters, not the date. Credit cost to be measured on wave 1, then
pace adjusted to use up the 25,200 Studio credits before they expire.

## Wave 1: street furniture for rue de la République (for 0.2)

Haussmann-style pedestrian street, cobblestones and light paving slabs, sober
furniture in anthracite gray or dark green.

| ID | Object | Generation notes | Size (m) | Status |
| ---- | ---- | ---- | ---- | ---- |
| W1-01 | Wood and cast-iron public bench | wooden slats, anthracite cast-iron legs, armrests | 1.8 × 0.6 × 0.8 | ⬜ |
| W1-02 | Modern metal public bench | anthracite gray perforated sheet metal | 1.8 × 0.6 × 0.8 | ⬜ |
| W1-03 | Concrete bench | light concrete block with wooden seat | 2.0 × 0.5 × 0.45 | ⬜ |
| W1-04 | Old-style street lamp | fluted cast-iron post, four-sided lantern | 4.5 high | ⬜ |
| W1-05 | Contemporary street lamp | cylindrical pole, flat LED head | 6.0 high | ⬜ |
| W1-06 | Wall-mounted street light | cast-iron bracket, lantern | 0.9 | ⬜ |
| W1-07 | Litter bin with transparent bag | metal hoop, rain cover | 0.9 high | ⬜ |
| W1-08 | Cylindrical litter bin | dark green perforated sheet metal | 0.9 high | ⬜ |
| W1-09 | Anti-parking bollard | cast iron, ball top, dark green | 1.0 high | ⬜ |
| W1-10 | Retractable bollard | stainless steel, reflective band | 0.8 high | ⬜ |
| W1-11 | Sidewalk barrier | metal tube, three posts | 1.5 × 1.0 | ⬜ |
| W1-12 | Bike rack hoop | stainless steel U-tube | 0.8 × 0.8 | ⬜ |
| W1-13 | Bike-share station terminal | generic terminal with no logo, gray | 1.3 high | ⬜ |
| W1-14 | Generic city bike | step-through frame, basket, gray | 1.8 long | ⬜ |
| W1-15 | Rectangular planter with shrub | corten steel | 1.5 × 0.6 × 0.7 | ⬜ |
| W1-16 | Large wooden tree planter | wooden crate, small olive tree | 1.2 × 1.2 × 1.0 | ⬜ |
| W1-17 | Tree grate | cast iron, radiating pattern | 1.5 × 1.5 | ⬜ |
| W1-18 | Public drinking fountain | dark green cast iron, old style | 1.2 high | ⬜ |
| W1-19 | Cultural poster column | cylinder, dome, fictional posters with no readable text | 3.5 high | ⬜ |
| W1-20 | Pedestrian information sign (district map) | metal totem, unreadable map | 2.2 high | ⬜ |
| W1-21 | Glass bus shelter | steel and glass structure, bench, no advertising | 4.0 × 1.5 × 2.6 | ⬜ |
| W1-22 | Closed newsstand | metal booth, shutters down | 2.5 × 2.0 × 3.0 | ⬜ |
| W1-23 | Disused phone booth | glass and metal, empty | 1.0 × 1.0 × 2.3 | ⬜ |
| W1-24 | Street mailbox | generic yellow metal, no logo | 1.2 high | ⬜ |
| W1-25 | Parking meter | gray column, screen | 1.6 high | ⬜ |
| W1-26 | EV charging station | white and gray column, two cables | 1.5 high | ⬜ |
| W1-27 | Fire hydrant | red cast iron | 0.8 high | ⬜ |
| W1-28 | Round manhole cover | cast iron, generic pattern | 0.7 diameter | ⬜ |
| W1-29 | Storm drain grate | cast iron, curbside | 0.8 × 0.3 | ⬜ |
| W1-30 | Street electrical cabinet | gray box, unreadable stickers | 1.0 × 0.4 × 1.4 | ⬜ |
| W1-31 | Glass recycling container | green bell | 1.5 × 1.5 × 1.7 | ⬜ |
| W1-32 | 240 l wheelie bin | gray plastic, green lid | 0.6 × 0.7 × 1.1 | ⬜ |
| W1-33 | 660 l wheelie bin | gray plastic, four wheels | 1.3 × 0.8 × 1.2 | ⬜ |
| W1-34 | Café terrace table | round top, cast-iron base | 0.7 diameter | ⬜ |
| W1-35 | Rattan terrace chair | two-tone weave | 0.5 × 0.5 × 0.85 | ⬜ |
| W1-36 | Metal terrace chair | folding metal, dark green | 0.5 × 0.5 × 0.85 | ⬜ |
| W1-37 | Closed terrace parasol | off-white canvas, no logo | 3.0 high | ⬜ |
| W1-38 | Open terrace parasol | off-white canvas, no logo | 3.0 diameter | ⬜ |
| W1-39 | Glass terrace windscreen | metal frame, glass | 1.5 × 1.2 | ⬜ |
| W1-40 | Restaurant chalkboard easel | wood, blank slate | 0.6 × 1.0 | ⬜ |
| W1-41 | Freestanding menu holder | metal, empty frame | 1.4 high | ⬜ |
| W1-42 | Clothing rack in front of a shop | chrome tube, a few garments | 1.5 × 1.5 | ⬜ |
| W1-43 | Florist's stall | wooden trestles, buckets of flowers | 2.0 × 0.8 × 1.0 | ⬜ |
| W1-44 | Closed metal shop shutter | steel slats, for shopfronts | 3.0 × 3.0 | ⬜ |
| W1-45 | Retracted awning | cassette, plain canvas | 4.0 wide | ⬜ |
| W1-46 | Extended awning | plain striped canvas with no text | 4.0 × 1.5 | ⬜ |
| W1-47 | Façade air conditioner | white outdoor unit | 0.8 × 0.3 × 0.6 | ⬜ |
| W1-48 | Wrought-iron balcony (railing) | scroll pattern, for façades | 2.0 × 1.0 | ⬜ |
| W1-49 | Mobile construction fence | wire mesh, concrete base | 3.5 × 2.0 | ⬜ |
| W1-50 | Traffic cone | orange, reflective bands | 0.5 high | ⬜ |
| W1-51 | Crowd control barrier | galvanized metal | 2.0 × 1.1 | ⬜ |
| W1-52 | Delivery pallet with boxes | wood, plain boxes | 1.2 × 0.8 × 1.0 | ⬜ |
| W1-53 | Delivery hand truck | metal, two wheels | 1.2 high | ⬜ |
| W1-54 | Generic parked scooter | gray fairing, no logo | 1.8 long | ⬜ |
| W1-55 | Shared electric kick scooter | generic, no logo | 1.1 long | ⬜ |
| W1-56 | Street name sign | blank blue enamel plate, text added in Unreal | 0.5 × 0.25 | ⬜ |
| W1-57 | Generic "pedestrian zone" sign | pole and sign with no real text | 2.5 high | ⬜ |
| W1-58 | Urban surveillance camera | dome on bracket | 0.4 | ⬜ |
| W1-59 | Wall-mounted ATM | stainless steel front, screen, no logo | 0.8 × 1.0 | ⬜ |
| W1-60 | Pile of garbage bags | three black bags | 0.8 × 0.6 × 0.5 | ⬜ |

## Wave 2: vehicle drafts and roadway objects (for 0.3)

Prompts: object text, then "single object, isolated, game prop, photorealistic, PBR textures, no logo, no readable text". No dimensions in prompts.

| ID | Object | Generation notes | Status |
| ---- | ---- | ---- | ---- |
| W2-01 | City car body draft | reuse interim hatchback | ⬜ |
| W2-02 | Sedan body draft | reuse interim sedan | ⬜ |
| W2-03 | Van body draft | reuse interim delivery van | ⬜ |
| W2-04 | Alloy wheel with tyre | five-spoke alloy, worn tyre | ⬜ |
| W2-05 | Steel wheel with hubcap | plain stamped steel | ⬜ |
| W2-06 | Worn spare tyre | dusty, cracked sidewall | ⬜ |
| W2-07 | Crumpled car door | debris, bent panel | ⬜ |
| W2-08 | Broken bumper | debris, cracked plastic | ⬜ |
| W2-09 | Crash debris pile | glass shards and plastic fragments | ⬜ |
| W2-10 | Dented car hood | debris, scraped paint | ⬜ |
| W2-11 | Concrete roadside bollard | round, chipped edges | ⬜ |
| W2-12 | Flexible lane bollard | white plastic with reflective band | ⬜ |
| W2-13 | Removable steel bollard | socket base, padlock | ⬜ |
| W2-14 | Rubber speed bump segment | black and yellow stripes | ⬜ |
| W2-15 | Speed cushion | asphalt-colored, painted marks | ⬜ |
| W2-16 | Delivery bay sign post | blank pictogram | ⬜ |
| W2-17 | Fold-down parking post | steel, hinged | ⬜ |
| W2-18 | Parking barrier arm | red-white arm, housing | ⬜ |
| W2-19 | Blank front license plate | no characters | ⬜ |
| W2-20 | Blank rear plate with holder | no characters | ⬜ |
| W2-21 | Stack of old tyres | five tyres | ⬜ |
| W2-22 | Steel road plate | trench cover, anti-slip | ⬜ |
| W2-23 | Water-filled road barrier | red plastic | ⬜ |
| W2-24 | Concrete jersey barrier | chipped, grey | ⬜ |
| W2-25 | Delineator post | white with reflector | ⬜ |
| W2-26 | Car battery | generic, no logo | ⬜ |
| W2-27 | Exhaust muffler | debris, rusty | ⬜ |
| W2-28 | Broken side mirror | debris, hanging cable | ⬜ |
| W2-29 | Road grit bin | yellow plastic, hinged lid | ⬜ |
| W2-30 | Portable roadwork sign | blank triangle sign on stand | ⬜ |

## Waves 2 to 5: overview (details added before they start)

- **Wave 2 (0.3)**: 3 fictional car body drafts (city car,
  sedan, van), generic wheels and rims, bodywork debris,
  bollards, speed bump, delivery bay, parking post, blank license
  plates.
- **Wave 3 (0.4)**: fictional pistol, magazines, holster, knife, bat,
  crates and sandbags for cover, training dummies.
- **Wave 4 (0.5)**: counter and cash register, shelving, glass-door fridge,
  generic unbranded products (bottles, boxes, cans, fruit),
  baskets, interior camera, glass door.
- **Wave 5 (0.6)**: emergency lights, police barriers, cones, handcuffs,
  radio, vests, detour signs, barrier tape with no real text.
