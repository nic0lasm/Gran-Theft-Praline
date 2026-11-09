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

## Waves V: real-world vehicles of the French fleet (for 0.3 onward)

Source: docs/gameplay/vehicles.md, section 10 (proposed by the vehicles thread; Nico's decision: cars are those found in France). Whole car at real scale, with reference images of the real model, no logos or plates (applied as decals outside Git); wheel and window cutting and rig done in Blender afterwards. No dimensions in prompts. Priority 1 = drivable in 0.3; 2 = traffic 0.7; 3 = services and two-wheelers.

| ID | Vehicle | Priority | Status |
| ---- | ---- | ---- | ---- |
| V-01 | Renault Clio V | 1 | ⬜ |
| V-02 | Peugeot 208 II | 1 | ⬜ |
| V-03 | Renault Kangoo II (white) | 1 | ⬜ |
| V-04 | Dacia Sandero III | 2 | ⬜ |
| V-05 | Citroën C3 III | 2 | ⬜ |
| V-06 | Peugeot 308 II | 2 | ⬜ |
| V-07 | Renault Captur II | 2 | ⬜ |
| V-08 | Peugeot 3008 II | 2 | ⬜ |
| V-09 | VW Golf VII | 2 | ⬜ |
| V-10 | Toyota Yaris IV | 2 | ⬜ |
| V-11 | VW Polo VI | 2 | ⬜ |
| V-12 | Renault Twingo III | 2 | ⬜ |
| V-13 | Fiat 500 | 2 | ⬜ |
| V-14 | Renault Mégane IV | 2 | ⬜ |
| V-15 | Dacia Duster II | 2 | ⬜ |
| V-16 | Mercedes A-Class W177 | 2 | ⬜ |
| V-17 | BMW 3 Series G20 | 2 | ⬜ |
| V-18 | Tesla Model Y | 2 | ⬜ |
| V-19 | Citroën Berlingo III | 2 | ⬜ |
| V-20 | Renault Trafic III | 2 | ⬜ |
| V-21 | Peugeot Boxer / Jumper | 2 | ⬜ |
| V-22 | Taxi Toyota Corolla estate | 2 | ⬜ |
| V-23 | TCL articulated bus (Heuliez GX 437 type) | 3 | ⬜ |
| V-24 | TCL trolleybus (Hess lighTram type) | 3 | ⬜ |
| V-25 | Peugeot 5008 Police nationale | 3 | ⬜ |
| V-26 | Lyon municipal police car | 3 | ⬜ |
| V-27 | SDMIS ambulance (VSAV) | 3 | ⬜ |
| V-28 | La Poste Kangoo | 3 | ⬜ |
| V-29 | Métropole waste truck | 3 | ⬜ |
| V-30 | Vélo'v bike | 3 | ⬜ |
| V-31 | Shared kick scooter | 3 | ⬜ |
| V-32 | Piaggio Liberty / Vespa scooter | 3 | ⬜ |
| V-33 | Yamaha TMAX | 3 | ⬜ |
| V-34 | Cargo bike | 3 | ⬜ |

## Priority generation order (rue de la République visible first)

Nico's decision: generate first what is visible in the 0.2 street. Order: wave 14, then 13, then 6, then 8 (≈ 230 models, Studio budget ≈ 12,650 credits). Waves 3, 4, 5, 7, 9 onward, and vehicles beyond the V-01 to V-34 already done, wait. No dimensions in prompts; shopfronts and signs carry no readable text, real brand signage is applied afterwards as decals outside Git.

## Wave 14: Potted vegetation, shopfronts, small street details

| ID | Object | Status |
| ---- | ---- | ---- |
| W14-01 | Terracotta pot with boxwood ball | ⬜ |
| W14-02 | Zinc planter with olive tree | ⬜ |
| W14-03 | Window box red geraniums | ⬜ |
| W14-04 | Balcony planter lavender | ⬜ |
| W14-05 | Potted bay laurel | ⬜ |
| W14-06 | Potted palm | ⬜ |
| W14-07 | Hanging flower basket | ⬜ |
| W14-08 | Climbing ivy panel | ⬜ |
| W14-09 | Potted bamboo | ⬜ |
| W14-10 | Topiary cone | ⬜ |
| W14-11 | Cafe shopfront | ⬜ |
| W14-12 | Bakery shopfront | ⬜ |
| W14-13 | Pharmacy shopfront | ⬜ |
| W14-14 | Clothing boutique shopfront | ⬜ |
| W14-15 | Tobacconist shopfront | ⬜ |
| W14-16 | Bank branch shopfront | ⬜ |
| W14-17 | Shuttered shop | ⬜ |
| W14-18 | Phone shop shopfront | ⬜ |
| W14-19 | Bookshop shopfront | ⬜ |
| W14-20 | Restaurant shopfront | ⬜ |
| W14-21 | Butcher shopfront | ⬜ |
| W14-22 | Jeweler shopfront | ⬜ |
| W14-23 | Optician shopfront | ⬜ |
| W14-24 | Supermarket entrance | ⬜ |
| W14-25 | Hair salon shopfront | ⬜ |
| W14-26 | Fast food shopfront | ⬜ |
| W14-27 | Blade projecting sign (blank) | ⬜ |
| W14-28 | Pharmacy cross sign | ⬜ |
| W14-29 | Blank shop sign board | ⬜ |
| W14-30 | Shop door | ⬜ |
| W14-31 | Display mannequin | ⬜ |
| W14-32 | Window display shelf | ⬜ |
| W14-33 | Awning crank | ⬜ |
| W14-34 | Doormat | ⬜ |
| W14-35 | Intercom panel | ⬜ |
| W14-36 | Mailboxes wall unit | ⬜ |
| W14-37 | Building entrance door | ⬜ |
| W14-38 | Carriage gate | ⬜ |
| W14-39 | Doorstep stone | ⬜ |
| W14-40 | Boot scraper | ⬜ |
| W14-41 | House number plaque (blank) | ⬜ |
| W14-42 | Gutter downpipe | ⬜ |
| W14-43 | Wall lamp sconce | ⬜ |
| W14-44 | Air vent grille | ⬜ |
| W14-45 | Cellar window grille | ⬜ |
| W14-46 | Meter cabinet | ⬜ |
| W14-47 | Parcel locker | ⬜ |
| W14-48 | Satellite dish | ⬜ |
| W14-49 | Window shutters | ⬜ |
| W14-50 | French window with railing | ⬜ |
| W14-51 | Window with lace curtain | ⬜ |
| W14-52 | Flower pot on sill | ⬜ |
| W14-53 | Bicycle against wall | ⬜ |
| W14-54 | Blank A-board | ⬜ |
| W14-55 | Wall ashtray bin | ⬜ |
| W14-56 | Water valve cover | ⬜ |
| W14-57 | Cobblestone patch | ⬜ |
| W14-58 | Limestone paving slabs | ⬜ |
| W14-59 | Curb stone | ⬜ |
| W14-60 | Gutter channel with grate | ⬜ |
| W14-61 | Stained paving slab | ⬜ |
| W14-62 | Fallen leaves pile | ⬜ |
| W14-63 | Plane tree trunk | ⬜ |
| W14-64 | Young plane tree with guard | ⬜ |
| W14-65 | Street litter | ⬜ |
| W14-66 | Newspaper on ground | ⬜ |
| W14-67 | Rain puddle | ⬜ |
| W14-68 | Pigeon | ⬜ |
| W14-69 | Pigeons group | ⬜ |
| W14-70 | Street cat | ⬜ |
| W14-71 | Dog water bowl | ⬜ |
| W14-72 | Doorstep parcel | ⬜ |
| W14-73 | Stacked fruit crates | ⬜ |
| W14-74 | Milk crate | ⬜ |
| W14-75 | Beer kegs | ⬜ |
| W14-76 | Gas bottle | ⬜ |
| W14-77 | Flower bucket | ⬜ |
| W14-78 | Terrace heater | ⬜ |
| W14-79 | Terrace planter divider | ⬜ |
| W14-80 | Umbrella stand | ⬜ |

## Wave 13: Facade and monument ornaments

| ID | Object | Status |
| ---- | ---- | ---- |
| W13-01 | Stone balcony with iron railing | ⬜ |
| W13-02 | Continuous iron balcony | ⬜ |
| W13-03 | Carved console bracket | ⬜ |
| W13-04 | Triangular pediment | ⬜ |
| W13-05 | Curved pediment with keystone | ⬜ |
| W13-06 | Molded window surround | ⬜ |
| W13-07 | Keystone mascaron | ⬜ |
| W13-08 | Cornice with dentils | ⬜ |
| W13-09 | Corbel row | ⬜ |
| W13-10 | Corinthian pilaster | ⬜ |
| W13-11 | Ionic column | ⬜ |
| W13-12 | Corinthian column | ⬜ |
| W13-13 | Balustrade | ⬜ |
| W13-14 | Balustrade end post | ⬜ |
| W13-15 | Rusticated block | ⬜ |
| W13-16 | String course | ⬜ |
| W13-17 | Zinc mansard dormer | ⬜ |
| W13-18 | Oeil-de-boeuf | ⬜ |
| W13-19 | Chimney stack | ⬜ |
| W13-20 | Roof finial | ⬜ |
| W13-21 | Zinc ridge and gutter | ⬜ |
| W13-22 | Cartouche (blank shield) | ⬜ |
| W13-23 | Garland swag | ⬜ |
| W13-24 | Lion head relief | ⬜ |
| W13-25 | Rosette medallion | ⬜ |
| W13-26 | Caryatid | ⬜ |
| W13-27 | Atlas figure | ⬜ |
| W13-28 | Allegorical statue | ⬜ |
| W13-29 | Bust on pedestal | ⬜ |
| W13-30 | Equestrian bronze (generic) | ⬜ |
| W13-31 | Standing bronze (generic) | ⬜ |
| W13-32 | Blank bronze plaque | ⬜ |
| W13-33 | Fountain basin | ⬜ |
| W13-34 | Wall fountain | ⬜ |
| W13-35 | Obelisk | ⬜ |
| W13-36 | Stone urn | ⬜ |
| W13-37 | Pedestal | ⬜ |
| W13-38 | Ornate lamp bracket | ⬜ |
| W13-39 | Door knocker | ⬜ |
| W13-40 | Carved door panel | ⬜ |
| W13-41 | Transom grille | ⬜ |
| W13-42 | Window guard railing | ⬜ |
| W13-43 | Oculus vent | ⬜ |
| W13-44 | Facade clock | ⬜ |
| W13-45 | Sundial | ⬜ |
| W13-46 | Gothic portal arch | ⬜ |
| W13-47 | Gargoyle | ⬜ |
| W13-48 | Gothic pinnacle | ⬜ |
| W13-49 | Rose window | ⬜ |
| W13-50 | Statue niche | ⬜ |
| W13-51 | Bell turret | ⬜ |
| W13-52 | Ornate iron gate | ⬜ |
| W13-53 | Spiked iron fence | ⬜ |
| W13-54 | Ornate stone bollard | ⬜ |
| W13-55 | Bronze candelabra lamp | ⬜ |
| W13-56 | Theater arch bay | ⬜ |
| W13-57 | Small triumphal arch | ⬜ |
| W13-58 | Monument steps | ⬜ |
| W13-59 | Granite monument base | ⬜ |
| W13-60 | Ornate rainwater head | ⬜ |

## Wave 6: Furniture for the squares

| ID | Object | Status |
| ---- | ---- | ---- |
| W6-01 | Classic square bench | ⬜ |
| W6-02 | Backless stone and wood bench | ⬜ |
| W6-03 | Circular tree bench | ⬜ |
| W6-04 | Large square fountain | ⬜ |
| W6-05 | Equestrian monument pedestal | ⬜ |
| W6-06 | Triple-lantern lamp post | ⬜ |
| W6-07 | Single-lantern lamp post | ⬜ |
| W6-08 | Gravel path edging | ⬜ |
| W6-09 | Low hoop fence | ⬜ |
| W6-10 | Lawn patch | ⬜ |
| W6-11 | Raised flower bed | ⬜ |
| W6-12 | Mature plane tree | ⬜ |
| W6-13 | Clipped lime tree | ⬜ |
| W6-14 | Horse chestnut | ⬜ |
| W6-15 | Tree guard grille | ⬜ |
| W6-16 | Bandstand | ⬜ |
| W6-17 | Morris-style kiosk (blank) | ⬜ |
| W6-18 | Refreshment kiosk | ⬜ |
| W6-19 | Carousel | ⬜ |
| W6-20 | Playground slide | ⬜ |
| W6-21 | Swing set | ⬜ |
| W6-22 | Spring rider | ⬜ |
| W6-23 | Sandbox | ⬜ |
| W6-24 | Concrete ping pong table | ⬜ |
| W6-25 | Petanque court border | ⬜ |
| W6-26 | Petanque boules | ⬜ |
| W6-27 | Steel drinking fountain | ⬜ |
| W6-28 | Square trash bin | ⬜ |
| W6-29 | Recycling column | ⬜ |
| W6-30 | Info board (blank map) | ⬜ |
| W6-31 | Bollard with chain | ⬜ |
| W6-32 | Grass hoop | ⬜ |
| W6-33 | Granite paving patch | ⬜ |
| W6-34 | Ornamental pool | ⬜ |
| W6-35 | Floor water jets | ⬜ |
| W6-36 | Seated bronze statue (generic) | ⬜ |
| W6-37 | Commemorative column | ⬜ |
| W6-38 | Steps with handrail | ⬜ |
| W6-39 | Low stone wall | ⬜ |
| W6-40 | Automatic public toilet | ⬜ |
| W6-41 | Market stall frame | ⬜ |
| W6-42 | Market stall with vegetables | ⬜ |
| W6-43 | Wooden market chalet | ⬜ |
| W6-44 | Generic food truck | ⬜ |
| W6-45 | Square cafe umbrella | ⬜ |
| W6-46 | Terrace table with two chairs | ⬜ |
| W6-47 | Bike hoop row | ⬜ |
| W6-48 | Path light bollard | ⬜ |
| W6-49 | Boxwood hedge | ⬜ |
| W6-50 | Pigeon flock | ⬜ |

## Wave 8: Road furniture

| ID | Object | Status |
| ---- | ---- | ---- |
| W8-01 | Vehicle traffic light | ⬜ |
| W8-02 | Pedestrian light | ⬜ |
| W8-03 | Light on arm pole | ⬜ |
| W8-04 | Push-button box | ⬜ |
| W8-05 | Stop sign (no letters) | ⬜ |
| W8-06 | Give way sign | ⬜ |
| W8-07 | No entry sign | ⬜ |
| W8-08 | Speed limit sign (blank) | ⬜ |
| W8-09 | No parking sign | ⬜ |
| W8-10 | One way sign | ⬜ |
| W8-11 | Pedestrian crossing sign | ⬜ |
| W8-12 | Bus stop pole (blank) | ⬜ |
| W8-13 | Direction finger signs (blank) | ⬜ |
| W8-14 | Bike lane sign | ⬜ |
| W8-15 | Roundabout sign | ⬜ |
| W8-16 | Warning triangle | ⬜ |
| W8-17 | Parking sign | ⬜ |
| W8-18 | Pole with two signs | ⬜ |
| W8-19 | Sign gantry (blank) | ⬜ |
| W8-20 | Guardrail | ⬜ |
| W8-21 | Guardrail end | ⬜ |
| W8-22 | Pedestrian railing | ⬜ |
| W8-23 | Concrete barrier | ⬜ |
| W8-24 | Plastic barrier | ⬜ |
| W8-25 | Barrier gate arm | ⬜ |
| W8-26 | Island bollard | ⬜ |
| W8-27 | Road stud | ⬜ |
| W8-28 | Lane separator | ⬜ |
| W8-29 | Tall cone | ⬜ |
| W8-30 | Traffic drum | ⬜ |
| W8-31 | Road works light | ⬜ |
| W8-32 | VMS trailer (blank) | ⬜ |
| W8-33 | Traffic camera pole | ⬜ |
| W8-34 | Signal controller cabinet | ⬜ |
| W8-35 | Zebra crossing patch | ⬜ |
| W8-36 | Painted speed bump | ⬜ |
| W8-37 | Manhole in asphalt | ⬜ |
| W8-38 | Road arrow marking | ⬜ |
| W8-39 | Curb ramp tactile | ⬜ |
| W8-40 | Tactile paving tile | ⬜ |

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
