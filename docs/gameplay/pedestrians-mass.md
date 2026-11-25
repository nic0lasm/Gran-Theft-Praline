# Pedestrians on Mass (GTPMass module)

## First module: the crowd (GTPMass, `-Pedestrians=mass`)

`GTPCrowd.h/.cpp`: `UGTPCrowdSubsystem` loads the walkway graph, creates `CrowdCount` (300, `-PedMass=N`) Mass entities
(`FGTPCrowdWalkerFragment` = the engine-free `GTPWalk::FWalker` of the legacy system, `FGTPCrowdLookFragment`,
`FGTPCrowdTag`) and one instanced mesh (`CrowdMesh`, a capsule stand-in until the baked person exists).
`UGTPCrowdMoveProcessor` walks them (same `GTPWalk::Step`) and puts those left behind back around the player;
`UGTPCrowdRepresentationProcessor` writes the instance transforms. `-Pedestrians=legacy` or a number keeps the legacy system.
Next: near people swapped for the pooled `AGTPPedestrian` (panic, hands up, ragdoll bridge), far people with animation
(AnimToTexture), crossings with the lights, ZoneGraph lanes instead of the walkway graph.

**Default (feat/mass-default):** with no argument the Mass crowd is the pedestrian system; `-Pedestrians=legacy` (profile density)
or `-Pedestrians=N` brings the legacy system back. The Mass crowd does what the legacy one does for panic spreading, gawking at
an accident, flight from a shot, hands up under a threat and ragdoll under the player's vehicle; its density is 300 people with 24 full
characters near the player (`CrowdCount`, `NearCount` in `[/Script/GTPMass.GTPCrowdSubsystem]`).

## Crossings with MassTraffic cars (`-CrowdCrossingWait=0` to switch off)

Until candidate 57 the crowd crossed one by one whatever the cars did (`GTPWalk::Step` with `bCrossingUnsafe` always false),
and each person on a crossing is an obstacle the MassTraffic cars brake for: with a steady flow of people the car at the head of a
queue never left (queues held up to 250 s at gate 57). Since candidate 58:

- **What a car is to a crossing** (`GTPWalk::LookAtCar`, engine-free): only a car whose line goes over the crossing counts, not one
  that drives along it or passes in the next street. It is *near* when its front is within 12 m of the crossing, stopped or not, and
  *dangerous* when it will be on it within 3 s at its speed, or rolls over it. The subsystem looks four times a second at the driving
  MassTraffic cars around the player (`GTPMassTrafficCars::Find`) and at the car the player drives.
- **In groups, as at a pedestrian light.** With a car near, people step off the kerb only during the first `CrossingWalkSeconds` (8)
  of every `CrossingCycleSeconds` (30); each crossing has its own clock. The car gets through between two groups. With a
  dangerous car they wait whatever the clock says. With no car in sight they cross at once, as before.
- **Nobody is stuck, on either side.** Who waits at the kerb is not an obstacle for the cars (no `FMassTrafficObstacleTag` until they
  step off), so a car never waits for somebody who waits for it. Nobody waits longer than `CrossingWaitMaxSeconds` (45): after
  that they cross and the cars brake. Those who wait stand side by side along the kerb.
- **The log line, every 10 s:** `crowd: crossings: N waiting at the kerb (longest wait now N s), N on a crossing; since the start:
  most waiting at once N, longest wait N s, N stepped off after the longest wait allowed (45 s); cars: N seen, N of the N crossings
  around have one near (N dangerous)`. A good run has people waiting whenever a crossing has a car near, a longest wait under
  about 25 s, and nobody (or almost) stepping off after the longest wait allowed. `cars: 0 seen (no MassTraffic cars to ask for)` means
  the map has no MassTraffic cars: nobody waits.
- **Checked offline** (`python3 tools/pedestrians/check_walk_logic.py`): one person every 2 s for 300 s at a crossing where a car
  waits. One by one, the crossing is never free. In groups it is free 62 % of the time, never busy more than 11 s in a row, the
  longest wait is 21 s and as many people get across. Twelve cars are read (stopped before it, coming fast, past it, in the next
  street, along it). **Not proven in the game**: that the queues of gate 57 drain.

## Hair of the assembled characters (`-CrowdHair=0` for none, `-CrowdGrooms=0` for no groom)

Until candidate 63 every passer-by was bald (video of gate 59). Not a cook problem: City Sample's crowd heads have no
hair of their own, its character code adds it (a groom bound to the face, and a baked texture painted on the scalp), and
our assembly (`UGTPCrowdSubsystem::DressFromCitySample`) gave none.

Since candidate 64 the crowd wears City Sample's own hair, the way City Sample puts it on (`FindHair`, `GiveHair`,
`AGTPPedestrian::SetGroom`, `SetHair`, `SetHairNear`):

- **What the project holds is asked of the asset registry** under `CrowdCharacterRoot`: the grooms
  (`Hair_<size>_<style>`), their hair cards (`…_CardsMesh_Group0_LOD0` to `LOD4`), the bindings Epic made of every groom
  for every face (`<sex>/GroomBindings/GB_<face mesh>_<groom>`), the hair meshes shown from a distance
  (`…_Helmet_LOD5`, `…_Mesh_LOD5`), the baked scalp textures (`…_baked`) and the baked eyebrows (`Eyebrows_*_baked`).
  The style named in `CrowdHairKeptForPlayer` (`PulledBack`) is left to the player.
- **Near the player, the groom**: a groom component on the face, with the binding of that groom for that face and the
  groom the binding names (two heads of the pack can have a groom of the same name; a binding holds only on its own).
  As hair cards (`bCrowdGroomCards`, the component's `bUseCards`), and only the grooms that have cards: strands cost
  several times more. The component's class and properties are reached by name, as for the player's grooms: the
  HairStrands plugin is not a dependency of the modules. It shows and ticks within `CrowdGroomDistance` (20 m) of the
  player only: a groom's tick is its game-thread cost (measured on the player's), so that distance is the lever if the
  frame time rises.
- **Further away, the far hair mesh** of the style on the head: a static mesh is fixed to the `head` bone, put back by
  the inverse of that bone's reference pose; a skeletal one follows the body's pose like a garment.
- **Under both, painted on the face**: on every material of the face that has the head material's `HairAttributeMap`, a
  dynamic instance takes the baked texture of the style, baked eyebrows (`EyebrowsAttributeMap`) and
  `ShowBakedGroomTextures` (only a texture of the same kind as the one it replaces, virtual or not).
- **Colours**: one melanin and one redness per person, dark more often than fair, on the head material and on the
  scalars of the hair materials whose name contains `Melanin` or `Redness`. `CrowdBaldShare` (0.1) of the men keep a
  bare scalp.
- **Log**: at the first person dressed, `crowd hair: men: N styles in the project under …: AfroFade (groom yes, cards
  yes, bindings 3, far mesh yes, painted yes), …`, the same for women (`NONE, they stay bald` when the project has
  nothing) and `crowd hair: N groom bindings and N baked eyebrows in the project; …`; once per face and style,
  `crowd hair: face m_001_nrw_FaceMesh, style AfroFade: groom …, binding …, cards asked for (bUseCards)` or `…: NO GROOM
  on this head`; once per material, `crowd hair: face slot N <material>: HairAttributeMap was …, set to …; eyebrows …`
  and `crowd hair: groom … slot N material …, its scalars: …`; `crowd hair: NO material of the face … has a
  HairAttributeMap texture: …`; every 10 s, `crowd: dressed N times: groom on the head N (not made N), far hair mesh N,
  hair painted on the scalp N, eyebrows painted N, left bald N, no head material that takes hair N; grooms shown now N
  (most at once N); without a top N, without trousers or a skirt N, without shoes N (women N), without a face N`.
- **What to copy** (assets outside Git, editor closed, City Sample read only), as for Nicolito's hair: by the inventory of 14 November the
  project has one groom (the player's) and two baked scalp textures, so without the copy the crowd only gets a painted
  scalp. `GTP_CROWD_HAIR=1` with `tools/character/crowd_hair_deps.py` lists what is missing: the 27 bindings of the
  nine grooms with cards for the faces 001 to 003 of each sex (the grooms and their cards follow at the next run, as
  what the bindings depend on), the nine far meshes, five baked textures and every garment mesh the code asks for;
  `tools/vehicles/copy_citysample_content.py --list Saved/CitySampleCopy/missing.txt` copies them (`--dry-run` first,
  for the size); run both again until only garments City Sample does not have remain ("nothing matches").
- **Not proven**: anything in the game. Not known from here: whether a groom binds on a face held at its level 4 and
  following the body's pose, what 24 grooms as cards cost, whether the far meshes are static or skeletal and how they
  sit on each head, whether the face at level 4 has a material with `HairAttributeMap`. The lines above answer each.

**A groom made in game asks for its pipeline states before it registers (candidate 67).** With the hair copied to
the PC (gate 65: 6 styles for the men, 5 for the women, 31 bindings found), every run that put a groom on a near
character raised one handled ensure, about 36 `Error` lines, and failed the gate's "0 error" rule: `PSOPrecache.cpp`,
`!IsComponentPSOPrecachingEnabled() || bPSOPrecacheCalled`, under `AGTPPedestrian::SetGroom`. The engine's groom
component only asks for its pipeline states when it is loaded from disk (its `PostLoad`); one made with `NewObject`
reached `CreateSceneProxy` without having asked. `SetGroom` now calls `PrecachePSOs` on the component once the groom,
the binding and the cards are set and before `RegisterComponent`, as the engine does itself for a skinned mesh. The
line `crowd hair: face …` ends with "pipeline states asked for before it registered". **Not proven**: nothing compiled;
expected at the next gate, no `PSOPrecache` ensure in any run.

**Shoes.** A top, trousers or a skirt and shoes are loaded for everybody (`LoadAny`, the first mesh of the list the
project has). At gate 55 the man held up at 3 m wears tan leather shoes (`gtarun-aim.jpg`); from 10 m in the video of
gate 59 the same shoes read as bare feet. For the women nothing shows it: the materials listed at gate 36 had none for
her shoes. The log now says it: `crowd: garments of the first man|woman: top …, bottom …, shoes …` (`NONE: … barefoot`
when the project has none of the meshes), and the script above lists the shoe meshes present and brings the missing.

The far crowd (beige figures beyond the 24 near characters) is another matter: one stand-in mesh, `CrowdMesh`.

## The ground under a person, and no stand-in near the player (candidate 66)

Video of gate 62, 96 to 140 s: people run in the air at the height of the balls of the signal poles, one stands on a
parasol, others float beside the trees; and among the fleeing crowd some are still white mannequins, one of them 3 m
from the camera.

- **In the air.** The ground under a person was the first surface met from 4 m above the player's ground: the top of a
  tree, a parasol or a signal pole when he passed under one, kept until the next look (about every 2 s). Now
  (`UGTPCrowdSubsystem::GroundUnder`, `GTPWalk::PickGround`, engine-free) the town's surfaces are met one component
  after the other, from head height (1.5 m) above his own ground, and his ground is the highest one he can step onto
  (`GroundStepUp`, 35 cm): what hangs over him is never met, a bench or a bin is left for the ground beneath it. When
  every surface is higher than a step (a ramp run up fast), the least high. Somebody just placed takes the surface
  nearest the player's ground, looked for from 4 m above it. Checked offline (`check_walk_logic.py`): 12 cases of 12
  (tree, signal pole, bench, kerb, ramp, cellar, parasol over a table).
- **White mannequins.** They are not passers-by left undressed: they are the stand-in of the far crowd
  (`CrowdMesh`, one still mesh baked from the mannequin). The full characters go to the nearest people, 24 of them
  until candidate 65; when more people flee round the player, the others were drawn as that stand-in, however near.
  Now `NearCount` is 36 (`-CrowdNear=N` to measure another number without a build) and no stand-in is drawn within
  `StandInMinDistance` (25 m) of the player: somebody left without a full character there is not drawn until one is
  free. The stand-in itself is unchanged beyond that distance: giving it clothes is asset work (City Sample's own far
  crowd meshes, `Crowd/VAT`), not done.
- **Log**, every 10 s: `crowd: ground: N people standing more than 1.5 m above the player's ground within 60 m of him
  now (highest N m); since the start: most at once N, highest N m, a surface above the ground left aside N times` (a
  good run keeps the first two at 0 on flat ground) and `crowd: bodies: N full characters; people within 40 m of the
  player without one N now (most N), of whom not drawn because nearer than 25 m N now (most N)` (the last "most" tells
  whether 36 is enough).
- **Not proven in the game.** Not known: what 12 more full characters cost (measured 13.4 ms of game p50 with 24
  against a limit of 17.38), and whether the people seen black in that video are only against the light (those nearer
  than 20 m, sun behind the camera, are lit as usual).

## A body on the ground leaves the cars' way (candidate 66)

Gate of candidate 62, `-GTARun` pass: the traffic's lines show five cars going `cruise -> waiting (ahead
GTPPedestrian_2147479682 …)` from 22:23:05 to 22:24:10, and the crowd's own line counts `fallen 1` from 22:23:12 to
22:24:02, then 0. It is not somebody standing on the lane: a walking person of the crowd has no collision at all, so the
only pedestrian the traffic's probe ahead (a sweep on pawns, vehicles and physics bodies) can meet is a body knocked
down, which has the `Ragdoll` profile. The person run over stays where his body lies until the body is given to
somebody else, and every car on that lane waited behind it all that time.

Since candidate 66 a body that has been down for 3 s leaves the cars' way (`AGTPPedestrian::SettleRagdoll`, a timer
set by `StartRagdoll`): its collision becomes physics only (no query sees it: the probe, a bullet) and ignores vehicles
and pawns, so it still lies on the ground and the cars pass over it without touching it. Log:
`pedestrian <name>: down for 3 s, the body leaves the cars' way …`. At the gate: no car waits behind the same
`GTPPedestrian` for more than a few seconds. **Not proven in the game.** Not done: the person getting up and running
off (no animation for it), and the traffic's cars stopping for people who stand or walk (they do not see them).

## A body thrown by the player's car stays within bounds and slides off the car

Long video of gate 74 (`-GTARun`, candidate 74): at 75 s the stolen car, at about 30 km/h, hits a man who goes 4 to 5 m
up, comes down on the roof and slides off its left side; at 143 s and 148 s, at 38 to 40 km/h through a crowd, bodies
fly as high and 15 m ahead; at 160 s one lands on the van's roof. (At 80 s the body lies on the road against the car's
left side; the light patch on the roof is its glass reflecting the sky.) Two causes in the code:

- **The fall began inside the car.** The rule was "within 170 cm of the car's centre": a car is 4 m long, so the
  bumper was already 30 cm into the person when his body got its collision, and the physics threw it out of the car's
  volume. Now the car's own box is read once a frame (`UGTPCrowdSubsystem::ReadPlayerCar`) and
  `GTPImpact::CarReaches` says when its leading end is about to touch the person (his radius, plus what the car covers
  in 0.05 s) or when a flank brushes him. Somebody 20 cm from the flank is no longer knocked down.
- **Nothing bounded what the car's collision did to the body afterwards.** `AGTPPedestrian` now ticks while its body
  falls and holds every part to `GTPImpact::FBodyBounds`: never faster along the ground than the car that hit
  (times 1.05), never up faster than 3 m/s, nothing going up once the pelvis is 1.4 m above the ground the person
  stood on (under a car's roof), spin at most 10 rad/s, and a part that starts inside the car is pushed out at 3 m/s
  at most. The launch itself is the rule the player run over already has (`GTPImpact::ClassifyCarHit`: 5.7 m/s ahead
  and 2 m/s up at 40 km/h), given to the whole body at once: on the pelvis alone it made the body somersault.

**A body that lies on a car.** Pelvis more than 60 cm above the ground and neither falling nor thrown
(`GTPImpact::StepOnCar`): after 0.4 s the body is pushed across the car, off its nearer side, at 2.8 m/s; still up
there after 1.5 s (another car, or the push did not do), the cars stop holding it and it drops to the ground. The
count only starts 0.8 s after the hit, the time a person takes to go down.

Checked without the engine (`python3 tools/ragdoll/check_impact_logic.py`): a pelvis kicked upward at 20 m/s at every
frame never passes 1.4 m by more than one frame's travel, and tops at 143 cm after one kick; from 18 to 130 km/h, at
30 and 60 frames a second, the person falls before the bumper is inside him; a free throw is never taken for a body
lying on a car. Log, every 10 s: `crowd: run over: N people hit by the player's car, N bodies fell; highest a pelvis
went above its ground … cm (bound 140), fastest part found … cm/s, … frames brought back within bounds, … bodies slid
off a car, … dropped through one`; per hit `crowd: <name> hit by the player's car at … km/h …` and
`pedestrian <name>: falls, leaves at …`; the line `down for 3 s …` now ends with the highest the pelvis went.
**Not proven**: nothing compiled, nothing seen in the game. Expected in the next long video: nobody above a car's
roof, no body left on a car for more than 2 to 3 s, "highest" at 140 cm or just above.

## A body that fell stays with its person until it lies still

The crowd has 36 bodies for the people within 40 m of the player: a body goes from whoever left the closest set to
somebody nearer. A person run over at 73 km/h is 40 m behind the car two seconds later. His body, still falling, was
taken from him: its physics and its collision went in one frame, in the middle of its contacts, and it stood elsewhere
the next. Gate 92's long take: a person falls at 150.2 s, the line `down for 3 s` of his body never comes, and at
153.9 s the physics engine reports a disabled body in a group that goes to sleep (`Ensure condition failed:
!PBDRigid->Disabled()`), 5.3 s to write its stack: the picture froze for 6 s. The link between the two is read in the
code and in the order of the lines, not proven.

A body that fell now stays until it has been down for its 3 s and none of its parts moves, 8 s at most
(`AGTPPedestrian::CanBeTaken`, `GTPImpact::BodyCanBeTaken`, checked by `tools/ragdoll/check_impact_logic.py`). The
crowd asks before it takes a body, and so does the older pedestrian subsystem before it moves a far person. A body
run over therefore lies in the street for 3 s and more instead of 2. Log: the line `crowd: run over` ends with
`N kept with their person while they fell, N taken still moving`, and the first ten bodies taken while they moved are
named (`pedestrian <name>: its body is taken … s after it fell`). At the gate: every `falls` has its `down for 3 s`,
no `Handled ensure`. Not compiled nor played when written.

## Cars standing on a walkway (`-CrowdAvoidCars=0` to walk through them as before)

Video of gate 59, 130 to 180 s: the player's car stands in a pedestrian street and people walk through it, one of them
up onto the bonnet. Two causes, two answers:

- **The ground under a person** was a visibility trace, which stops on a car: the person stood on it. It is now a trace on
  the static objects only (`ECC_WorldStatic`, as the cars' own ground traces), and something more than 40 cm higher than
  the ground 60 cm behind the person is not taken for ground (a bench, a planter). Since candidate 66 the ground is told
  from what stands on it or hangs over it by the section above.
- **A car in the way is walked round** (`GTPWalk::AvoidBox`, engine-free): four times a second the subsystem lists the
  vehicles of the game standing or slower than 5 km/h within `CarAvoidDistance` (70 m) of the player, as the box of
  their body seen from above, and the slow MassTraffic cars that have no such actor. A person whose next 3.5 m go
  through a box (45 cm of margin) takes a line beside it on his walkway; right in front of it he stands and steps
  sideways; with no room on either side he waits 4 s and turns back. A fast car is not walked round: people run from it
  or are knocked down.
- **Log**, every 10 s: `crowd: cars: N standing or slow within 70 m of the player; people going round one N, standing in
  front of one N, within the body of one N; since the start: most within a body at once N, turned back at one N,
  something high not taken for the ground N times`. A good run keeps "within the body of one" at 0 or 1 (a car that
  stops on somebody).
- **Checked offline** (`python3 tools/pedestrians/check_walk_logic.py`): a car on a walkway, 20 people for 180 s. Not
  avoided: 5 114 frames inside its body. Avoided on a 7 m walkway: 0 frames inside and as many passages (173 for 174);
  on a 2 m walkway: 0 frames inside, nobody passes, everybody turns back after 4 s. **Not proven in the game.**

## City Sample characters (default; `-CrowdCitySample=0` for the mannequins)

The 24 near characters are assembled from the City Sample Crowds subset under `/Game/Crowd` (outside Git): body, three
garments and a face that follow the body by leader pose. On by default since candidate 43: at the gate of candidate 42
the packaged game held a game p50 of 11.49 ms with them (budget 13) and logged no material error.
`-CrowdCitySample=0` brings back the mannequins of before (the player's own body mesh); a project without the pack
falls back to them by itself and says so ("crowd: assembled City Sample characters off, mannequins"). With the
mannequins the log names their body and materials ("crowd: mannequin material slot …").

### Head material

`M_Crowd_Head_v2` (`/Game/Crowd/Character/Shared/Materials/MetaHuman`) fails one permutation in the SM6 cook: the Nanite
base pass (`TBasePassCSFNoLightMapPolicy`, `FNaniteVertexFactory`) needs 65 SRVs and D3D allows 64. One failed
permutation leaves the whole hierarchy without a shader map, and the packaged game logs "Loading a material resource
None with an invalid ShaderMap" and "Tried to access an uncooked shader map ID" as soon as a head loads (the crowd's and
Nicolito's, which use the same hierarchy). The editor never asks for that permutation, so it shows nothing.

City Sample meets the same failure and answers it in its own `DefaultEngine.ini`: `r.Nanite.AllowSkinnedMeshes=0`, with a
comment that names `M_Crowd_Head_v2` and its more than 64 SRVs. Our `[SystemSettings]` has the same line since
candidate 41: the Nanite permutation of skinned meshes is no longer compiled, Epic's material stays untouched, and the
packaged game has no `LogMaterial: Error` with the crowd or with Nicolito (gate of candidate 41). It also means no
skeletal mesh of the game can be drawn through Nanite.

### Cost of the near characters

Each near character follows the economy of the legacy pedestrians and of the AI cars: beyond `NearDetailDistance` (20 m)
no shadow and the body's level of detail `NearFarLOD` (2), the garments at the body's level (`bSyncAttachParentLOD`);
the pose is refreshed 20 times a second beyond 15 m and 12 times beyond 25 m (`bNearReduceAnimRate`), every frame for
a body that falls. Settings in `[/Script/GTPMass.GTPCrowdSubsystem]`.

### Animations of the assembled characters

The City Sample bodies have the MetaHuman skeleton, not the mannequin's: walk, idle, run and the hands-up loop are baked
for it under `/Game/MetaHumans/GTP/Anims` (`tools/character/retarget_player_animations.py`, prefix `MH_`). The hands-up
loop is `MH_A_HandsUp` (`CrowdHandsUpAnimation`); while it is not baked the mannequin's own is played and the log says
"ANOTHER SKELETON". `-GTARun` saves what the player sees when the hold-up succeeds
(`Saved/Screenshots/gtarun-aim.png`, `gtarun-aimclose.png`): the states it counts say nothing of the pose shown.
