# Pedestrians on Mass: mapping of the current behaviours

*French version: [pedestrians-mass.fr.md](pedestrians-mass.fr.md). Decision: [ADR-0015](../adr/adr-0015-mass-crowd-traffic.md).*

What exists today (`GTPWalkLogic.h`, engine-free, tested by `tools/pedestrians/check_walk_logic.py`) and where
each part lives once pedestrians are Mass agents. Nothing here is built yet; this is the plan the first
Mass slice follows.

| Today | On Mass | Notes |
| --- | --- | --- |
| Walkway graph (CSV from BD TOPO) | ZoneGraph lanes generated from the same data (tags Pedestrian, Crossing) | one source, two outputs: the CSV stays for the legacy system and the tests |
| `GTPWalk::Step` (walk, pause, cross) | MassCrowd navigation along lanes; pauses through Smart Objects (window, bench, phone) | crossing wait keeps `GTPCrossingBlockedQuery` as a ZoneGraph annotation fed by the lights |
| `Flee`, `CatchPanic` (herd) | ZoneGraph disturbance annotation: a danger event writes a disturbance, agents in range switch to the flee state and pass it on once (same radius and generations) | the rule (7 m, 65 %, 2 steps) stays in one engine-free function used by both |
| `Gawk`, `Freeze` (hands up) | state-tree tasks "Look at" and "Hands up" on the near actor; far agents only flee or stand | `GTPThreat` and `GTPWitnessLogic` are called from the actor, never reimplemented |
| 20 drawn skeletal meshes | near actor (about 40 m, a City Sample Crowds character) drawn from a pool; beyond, AnimToTexture instances | the actor pool keeps `AcquireFromPool` for shops and scenes |
| Ragdoll cap (3 on m1) | near actor only, same shared ceiling | far agents never ragdoll |
| `-PedDemo=panic\|gawk\|shot` | kept: events trigger the same disturbance and loud-sound paths | the PC films the same three scenarios on both systems |

First slice (week 1): MassCrowd agents on the generated lanes, near actor swap with our reactions, 150+
pedestrians in the slice, GPU cost on the PC, `-Pedestrians=legacy` to compare.

What this needs from others: the generated ZoneGraph (traffic thread, with Pedestrian and Crossing lanes),
the City Sample Crowds assets (outside Git), and the PC for compile and measures.
