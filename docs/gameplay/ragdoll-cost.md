# Ragdoll and breakables: cost and how to go past 3 ragdolls

*French version: [ragdoll-cost.fr.md](ragdoll-cost.fr.md)* · ADR-0006

**All figures below are estimates from how the engine works, not measurements.** The
first Mac run (`tools/ragdoll/measure-ragdoll-mac.sh`) replaces them.

## What costs, per simulating ragdoll

| Cost | Thread | Order of magnitude (estimate) | Lever |
| --- | --- | --- | --- |
| Rigid bodies and constraints of the physics asset (the mannequin has about 20 to 30 bodies, many of them fingers) | physics / game | 0.1 to 0.3 ms | simulate fewer bodies (hands kept kinematic), fewer solver iterations, damping so bodies sleep |
| Writing the simulated pose back into the skinned mesh | game | 0.02 to 0.05 ms | frozen pose after rest |
| Capsule follow, state machine | game | negligible | none needed |
| Skinning and shadow of the mesh | GPU | same as an animated one at the same LOD | LOD, no shadow far away, hide after freezing |
| Contacts with cars and the street collision | physics | grows with the contact count: the main risk in a pile-up | cap, collision profile |

Rest costs nothing: no tick, simulation off after the freeze. The risk is a crash
with many bodies at once, hence the cap.

## What costs, per breakable object

Removing an instance from a hierarchical instanced mesh rebuilds the tree of that
component: a few hundred instances should be sub-millisecond, but this is the likeliest
hitch (to check with the largest group of props). The thrown body is one rigid body
(about 0.02 ms) with a lifetime of 25 s and a cap of 6 on m1. The 20 Hz check against
the instances near a car is a sphere query per group: expected under 0.1 ms.

## Going past 3 ragdolls (switch `-RagdollLite=1`)

| Lever | In lite mode | Expected gain (estimate) |
| --- | --- | --- |
| Simplified physics | hands and fingers kinematic, 3 solver iterations instead of the default, damping 1.5 | 30 to 40 % of a ragdoll's physics cost |
| Quick sleep | sensitive sleep family, rest at 50 cm/s for 0.4 s, decided after 5 s at most, pauses 0.5 / 1.5 s | a body stops costing about 40 % sooner |
| Freeze then disappear | pose kept after rest, a non-player body hidden 4 s later | no skinning cost for old bodies |
| Distance cap | no ragdoll beyond 35 m from the viewer (the body stays animated); the cap still freezes the farthest | removes the invisible cases |
| Higher cap | 6 on m1, 12 elsewhere instead of 3, 6, 10 | to confirm: only if the lite cost per body is at most half the full one |

Decision rule: lite becomes the default if three lite ragdolls cost at most 0.5 ms
GPU and 1 ms CPU over the baseline (`-Ragdoll=0`) and six cost no more than three full
ones. Otherwise the cap stays 3.
