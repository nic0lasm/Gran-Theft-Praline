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

## Hair of the assembled characters (`-CrowdHair=0` for none)

Until candidate 63 every passer-by was bald (video of gate 59). Not a cook problem: City Sample's crowd heads have no
hair of their own, its character code adds it (a groom, and a baked texture painted on the scalp at a distance), and our
assembly (`UGTPCrowdSubsystem::DressFromCitySample`) gave none. Shoes were never missing: a top, trousers or a skirt and
shoes are loaded for everybody, and the log now counts whoever lacks one.

Since candidate 64 (`UGTPCrowdSubsystem::FindHair`, `GiveHair`):

- **What the project holds is asked of the asset registry** under `CrowdCharacterRoot`: the baked scalp textures
  (`Hair_<size>_<style>_baked`) and the hair meshes City Sample shows from a distance (`Hair_<size>_<style>_Helmet_LOD5`,
  `_Mesh_LOD5`). The style named in `CrowdHairKeptForPlayer` (`PulledBack`) is left to the player.
- **Painted on the scalp**: on every material of the face that has the head material's `HairAttributeMap`, a dynamic
  instance takes the baked texture of the person's style (only one of the same kind as the one it replaces, virtual or
  not) and `ShowBakedGroomTextures`.
- **With a volume**: the style's mesh on the head. A static mesh is fixed to the `head` bone, put back by the inverse of
  that bone's reference pose (it was modelled around the head in that pose); a skeletal one follows the body's pose like a
  garment (`AGTPPedestrian::SetHair`).
- **Colours**: one melanin and one redness per person, dark more often than fair, on the head material (hair and
  eyebrows) and on the scalars of the hair mesh's material whose name contains `Melanin` or `Redness`.
  `CrowdBaldShare` (0.1) of the men keep a bare scalp.
- **Log**: at the first person dressed, `crowd hair: men: N styles in the project under …: AfroFade (painted + mesh), …`
  and the same for women (`NONE, they stay bald` when the project has nothing); once per material,
  `crowd hair: face slot N <material>: HairAttributeMap was …, set to …; ShowBakedGroomTextures …` and
  `crowd hair: mesh … slot N material …, its scalars: …`; `crowd hair: NO material of the face … has a HairAttributeMap
  texture: …` when the face shown has no such material; every 10 s, `crowd: dressed N times: hair painted on the scalp N,
  hair mesh on the head N, left bald N, no head material that takes hair N; without a top N, without trousers or a skirt
  N, without shoes N, without a face N`.
- **What to copy for the volume** (assets outside Git, editor closed, City Sample read only), as for Nicolito's hair:
  `GTP_CROWD_HAIR=1` with `tools/character/crowd_hair_deps.py` lists the nine hair meshes and five baked textures of the
  crowd and what they depend on, missing in the project; `tools/vehicles/copy_citysample_content.py --list
  Saved/CitySampleCopy/missing.txt` copies them (`--dry-run` first); run both again until `0 missing`. The script also
  gives the class of each mesh (static or skeletal).
- **Not proven**: anything in the game. Not known from here: whether the face shown at its level 4 has a material with
  `HairAttributeMap`, whether the hair meshes are static or skeletal, how their material draws on a static mesh, how a
  mesh modelled on City Sample's reference head sits on each of the six heads. The lines above answer each.

The far crowd (beige figures beyond the 24 near characters) is another matter: one stand-in mesh, `CrowdMesh`.

## Cars standing on a walkway (`-CrowdAvoidCars=0` to walk through them as before)

Video of gate 59, 130 to 180 s: the player's car stands in a pedestrian street and people walk through it, one of them
up onto the bonnet. Two causes, two answers:

- **The ground under a person** was a visibility trace, which stops on a car: the person stood on it. It is now a trace on
  the static objects only (`ECC_WorldStatic`, as the cars' own ground traces), and something more than 40 cm higher than
  the ground 60 cm behind the person is not taken for ground (a bench, a planter).
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
