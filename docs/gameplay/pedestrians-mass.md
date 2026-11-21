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
