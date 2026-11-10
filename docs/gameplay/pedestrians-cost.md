# Pedestrian cost and how to scale the crowd

*French version: [pedestrians-cost.fr.md](pedestrians-cost.fr.md)*

Status: **no measurement exists yet for the pedestrians** (nothing compiled in
Unreal when this was written). The only reference is version 0.3: 6 AI drivers,
each a full character with its animation blueprint, cost about 10 ms of GPU on the
M1 profile. A pedestrian here is much lighter (one looping animation, no animation
blueprint, no movement component, no physics), but by how much is the first thing
to measure. Gains below are hypotheses ranked by expected size, to be confirmed or
discarded by the bench.

## Where the cost of one drawn person is

| Part | What drives it | Lever |
|---|---|---|
| GPU, geometry and shading | triangles of the LOD drawn, number of material slots, resolution (m1 = 50 %) | forced LOD with distance, fewer slots, fewer people drawn |
| GPU, shadows | a moving skinned caster redraws virtual shadow map pages every frame | no shadow beyond 25 m (done), no contact shadow (switch) |
| GPU, lighting | dynamic GI and distance field contribution of a skinned mesh | switched off for people (switch) |
| CPU, animation | pose evaluation of the skeleton and skinning update, every frame, per person | animation rate by distance (switch), skeleton LOD, not posing hidden bodies (done) |
| CPU, game thread | movement along the walkway, ground trace, separation | 5 Hz updates when not drawn (switch), traces round robin (done) |

## Levers, in the order to try them

| # | Lever | State | Expected gain (hypothesis) | Switch |
|---|---|---|---|---|
| 1 | Fewer drawn (`MaxVisible` 10, 6, 20) | done | linear in people drawn: the first and surest | `-PedMaxVisible=N` |
| 2 | Lower mesh LOD and no shadow with distance | done | large per far person, none for near ones | `FarLOD`, `DetailDistance` |
| 3 | Animation rate by distance (1/20 s, 1/12 s, 1/8 s) | done, on | CPU: halves the pose cost of far people | `-PedAnimRate=0/1` |
| 4 | Cheap render flags (no contact shadow, decals, occlusion, GI, per-bone blur; fixed bounds) | done, on | GPU: small per person, adds up | `-PedCheap=0/1` |
| 5 | Far updates at 5 Hz | done, on | game thread: hidden people almost free | `-PedSpread=0/1` |
| 6 | Appear out of sight, not drawn beyond 90 m | done | avoids pop-in; keeps the pool small | `DrawDistance` |
| 7 | Skeleton LOD: drop fingers, twist and IK bones at LOD 1+ | to do (editor) | CPU: skeleton cost drops about with the bone count | asset setting |
| 8 | Far crowd as vertex animation texture instances (one draw call for many) | to do, asset step per mesh | what makes 100+ possible; no reaction to hits, so only beyond 25 m | new |
| 9 | Impostor sprites beyond 60 m | to do | cheapest far crowd; needs a capture step per mesh | new |
| 10 | Mass crowd representation | alternative | designed for thousands; heavy plugins, zone graph in the map | see ADR-0005 |

Levers 1 to 6 are in the code behind switches; 7 to 9 need editor work and a
measurement showing they are needed. From 12 to a believable crowd of 30 to 60 the
likely path is: levers 1 to 5 to find how many drawn people the budget carries
(1.5 ms GPU, 0.5 ms CPU), then lever 8 for everyone beyond that, so the street
feels populated while only the closest 10 are real characters.

## Measuring

```
caffeinate -dimsu tools/pedestrians/bench-crowd-mac.sh <version> [duration_s]
```

Runs the packaged m1 build with 0, 12, 30 and 60 people, then at 30 each lever off
in turn and `MaxVisible` 6 and 20, 180 s each by default, and writes
`Builds/<version>/evidence/crowd-<stamp>/SUMMARY.md`. Cost of a person = frame
time with N minus with 0, divided by the people drawn (`LogGTPPedestrians: active=
drawn=` lines). Needs the packaged build only, no editor.

New behaviours are frozen until these numbers are in.

## Budget (performance thread, 2026-11-10)

Target: 20 drawn people at most 2 ms GPU and 1 ms CPU; shadows only inside 20 m,
reduced animation rate beyond 25 m, impostors or instanced crowd beyond 40 m.
The case `n40-vis20` of the bench is that target. The judge is `tools/perf/bench.sh`;
the crowd bench is the quick per-lever comparison.
