
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
