# Breakable street furniture (0.4): list and behaviours

Proposal of 2026-11-10, to be agreed with the ragdoll and physical reactions thread before
any code. It covers the props already placed on rue de la République by
`tools/city/props.py` and `tools/city/details.py`, plus the catalogue items (waves 1, 8 and
14) that will be. Each catalogue item gets one of the three behaviours below, as in
open-world games: the city reacts to a speeding car, but buildings and heavy structures hold.

## Three behaviours

| Class | What happens on impact | Proposed technique |
| ---- | ---- | ---- |
| **R: toppleable** | The prop becomes a rigid body, rolls, tips over, can be pushed on foot | On contact the HISM instance is swapped for a physics actor (same mesh, simple collision, mass below) that goes back to sleep once still |
| **C: breakable** | Above an impulse threshold it bends or snaps at the base and falls; below it, it stops or deflects the car | Swap for a physics actor with a base joint that breaks (tunable threshold), or a Chaos Geometry Collection for glazing only |
| **F: fixed** | It stops the car and damages it, it does not move | Nothing to do: current blocking collision |

Proposed M1 budget (to confirm with the optimisation thread): at most 24 awake physics bodies
at once; a fallen prop stays 60 s, then vanishes out of view, and the original instance
comes back when the player is more than 80 m away. Breakables under 0.5 m cast no shadow
once on the ground.

## List per item

| ID | Item | Class | Mass (kg) | Note |
| ---- | ---- | ---- | ---- | ---- |
| W1-07, W1-08 | Litter bins | R | 15 | The W1-07 transparent bag can spill W14-65 litter |
| W1-32, W1-33 | Wheelie bins | R | 25 / 45 | They roll |
| W1-34 | Café table | R | 12 | |
| W1-35, W1-36 | Terrace chairs | R | 4 | Fly far: good effect, cheap |
| W1-38 | Open parasol | R | 20 | Tips over with its base |
| W1-40, W1-41 | Chalkboard, menu holder | R | 6 | |
| W1-50, W8-29, W8-30 | Cones, drums | R | 3 / 5 / 10 | |
| W1-51, W8-24 | Crowd and plastic barriers | R | 15 / 8 | |
| W1-14, W1-55, W1-54 | Bicycle, kick scooter, parked scooter | R | 15 / 20 / 110 | The scooter falls on its side |
| W14-05, W14-79 | Potted bay laurel, terrace planter | R | 40 / 60 | Heavy: they slide more than fly |
| W14-78 | Terrace heater | R | 25 | |
| W14-73, W14-75, W1-60 | Crates, kegs, rubbish bags | R | 5 / 30 / 3 | |
| W1-04 | Heritage street lamp | C | 250 | Snaps at the base above about 35 km/h; its light goes out (day/night thread) |
| W8-01, W8-02 | Vehicle and pedestrian lights | C | 120 / 80 | Same; a fallen light stays dark |
| W8-05 to W8-17, W1-57 | Signs on poles | C | 25 | Bend from 15 km/h |
| W1-12 | Bike hoop | C | 30 | Bends, does not come off |
| W1-01 | Wood and cast-iron bench | C | 90 | Torn off above about 40 km/h |
| W1-13 | Bike-share terminal | C | 150 | |
| W1-24, W1-25 | Post box, parking meter | C | 60 / 80 | |
| W1-27 | Fire hydrant | C | 80 | Torn off: water jet (particle effect to plan) |
| W1-21, W1-39 | Bus shelter, glass windscreen | C (glass only) | | The glass shatters (Geometry Collection), the frame stays |
| W1-09 | Anti-parking bollard | F | | Lyon bollards really stop cars |
| W1-17, W14-63, W14-64 | Tree grate, plane trunks | F | | Guarded young tree W14-64: C to discuss |
| W1-19, W1-22, W1-23 | Poster column, kiosk, phone booth | F | | |
| W1-30, W8-34 | Street cabinets | F | | |
| W1-18, W1-31 | Drinking fountain, glass bell | F | | |
| Monumental W13-xx, W6-xx | Statues, fountains, plinths | F | | |
| Wall and roof details | Downpipes, plaques, awnings, chimneys | F | | On walls, out of reach |

## How it fits without editing the map

`props_placement.csv` and `details_placement.csv` will gain a `breakable` column (R, C or F)
and a `mass_kg` column, taken from this list by catalogue id. `place_props.py` will put R and
C props in HISM components with a tag (`GTP_Breakable_R`, `GTP_Breakable_C`). A C++ world
subsystem, on the ragdoll thread's side, will swap the hit instance for a physics actor on
impact. The map stays replayable by script, as today.
