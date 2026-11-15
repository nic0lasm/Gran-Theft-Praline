
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

## City Sample characters (`-CrowdCitySample=1`, off by default)

The 24 near characters can be assembled from the City Sample Crowds subset under `/Game/Crowd` (outside Git): body, three
garments and a face that follow the body by leader pose. The switch stays off until the crowd holds the game budget and
cooks without a material error.

### Head material

`M_Crowd_Head_v2` (`/Game/Crowd/Character/Shared/Materials/MetaHuman`) fails one permutation in the SM6 cook: the Nanite
base pass (`TBasePassCSFNoLightMapPolicy`, `FNaniteVertexFactory`) needs 65 SRVs and D3D allows 64. One failed
permutation leaves the whole hierarchy without a shader map, and the packaged game logs "Loading a material resource
None with an invalid ShaderMap" and "Tried to access an uncooked shader map ID" as soon as a head loads (the crowd's and
Nicolito's, which use the same hierarchy). The editor never asks for that permutation, so it shows nothing.

Our heads are drawn as plain skinned meshes, so our copy of the material does not need the Nanite permutation.
`tools/pedestrians/fix_crowd_head_material.py` reports the usage flags, the textures sampled, the instances and the
skeletal meshes of the hierarchy and their Nanite setting; with `GTP_APPLY=1` it clears "Used with Nanite" and
"Automatically set usage in editor" on our copy, recompiles and saves it. Only in this project, never in the City Sample
folder; to run again on any fresh copy of the pack. Check: the cook no longer prints "Failed to compile Material" for
the head, and the packaged log has no `LogMaterial: Error`.
