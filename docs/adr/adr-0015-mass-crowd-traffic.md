# ADR-0015: Mass crowd and traffic

*French version: [adr-0015-mass-crowd-traffic.fr.md](adr-0015-mass-crowd-traffic.fr.md)*

## Status

Proposed on 2026-11-12. Decided by the project lead the same evening (switch to Mass now); this ADR
records it and is completed together by the pedestrians and traffic threads. Supersedes the "Mass later"
options of ADR-0003 and ADR-0005.

## Date

2026-11-12

## Decision Makers

nic0lasm (project lead)

## Summary

Pedestrians and traffic move to Unreal's Mass framework: agents on a ZoneGraph generated from our own
data, a level of detail that turns the agent near the player into a real actor (our reactions, witnesses,
ragdoll, car theft) and the far ones into animated instances. The current systems stay behind
`-Pedestrians=legacy` and `-Traffic=legacy` until Mass measures as well.

## Context

The target is a living street of 150+ pedestrians and 40+ cars in the slice, which the fixed pool of
skeletal meshes of ADR-0005 cannot reach at the M1 and PC budgets. The reasons ADR-0005 postponed Mass
(a zone graph authored by hand in the map, one binary several threads edit) are removed by generating
the ZoneGraph from our data in the city import.

## Decision

1. **Plugins** in the .uproject: MassGameplay, MassAI (MassCrowd, MassNavigation, MassLOD,
   MassRepresentation, MassAvoidance), ZoneGraph, ZoneGraphAnnotations, StateTree, GameplayStateTree,
   AnimToTexture. Smart Objects is already on.
2. **ZoneGraph generated, never drawn.** An editor step of `run_city_import.sh` (Python calling a C++
   importer, see Traffic details) creates the ZoneShapes from BD TOPO: sidewalks and crossings from the walkway graph of `tools/city/pedestrians.py`,
   lanes from `lanes.csv`; lane tags Pedestrian, Crossing, Car. The map stays out of Git and is rebuilt by the
   import, so nobody edits it by hand.
3. **Pedestrians.** MassCrowd agents on the pedestrian lanes with the Mass LOD:
   - within about 40 m of the player: a real actor with a City Sample Crowds character, our reactions
     (panic spreading, bystanders, hands up: `GTPWalkLogic.h`, `GTPThreat`), witnesses (`GTPWitnessLogic`)
     and ragdoll, called from the actor or from Mass processors;
   - beyond: instances animated by texture (AnimToTexture);
   - fleeing goes through ZoneGraph disturbance annotations, stops at windows and benches through Smart Objects.
4. **Traffic** (traffic thread): MassTraffic copied from City Sample 5.8 into `Plugins/MassTraffic`,
   git-ignored (Epic code is not allowed in the public repo; the README says how to install it); near cars are
   Chaos actors (theft, collisions, chases), far ones instances; our code that depends on it lives in its own
   module (GTPMass) so the rest compiles without it.
5. **Gate for every step**: CI green, PC compile, PC package that holds 120 s without crash. Parity is
   measured before any legacy code is deleted.

## Consequences

- The reaction rules (engine-free, tested in CI) are kept: they become the logic of the near actor and of
  the Mass processors, so the work done on them is not lost.
- City Sample Crowds assets and MassTraffic are Epic content: out of Git, installed by script.
- The slice may slip by one to two weeks (risk accepted by the project lead).
- First measurable goal: 150+ pedestrians in the slice, GPU cost measured on the PC (week 1).

## Traffic details (traffic thread)

- **ZoneGraph from our data** (branch `feat/mass-zonegraph`): `tools/city/zonegraph.py` turns `lanes.csv`
  (one-way lanes, junction ids, next lanes) and `walkways.csv` into a table of shapes: one-way "Car" splines
  cut 6 m before each junction; one polygon per junction (tags Car and Intersection) whose lane-profile
  points are those cut ends, so ZoneGraph routes the turns inside and MassTraffic finds its intersections;
  two-way "Pedestrian" splines, tag Crossing on crossings. The editor step `place_zonegraph.py` hands the
  table to a C++ importer (`UGTPZoneGraphImport`, module GTPMass), which places the ZoneShape actors and adds
  missing tags and lane profiles: C++ rather than Python because the ZoneShape point API is not exposed to
  Python. The slice tables of `npc_data.py` (lanes with direction and signals) plug in through `--lanes` and
  `--walkways`.
- **Traffic lights**: MassTraffic intersections run their own light cycles; ours (`GTPTrafficLights.h`, tested)
  feed the periods and phases so the lights drawn in the street (`-SignalHeads`) stay in step.
- **Near car = our Chaos actor.** MassTraffic hands a car near the player to a Chaos actor class; ours is
  `AGTPVehicle` (doors, carjack, damage, bail out, police crimes). Its driver logic is the existing one:
  `GTPDriveLogic.h` for the pedals and `GTPTrafficReactLogic.h` for the horn, going round, panic, fleeing and
  shock (branch `feat/traffic-reactions`, tested in CI), driven by a GTPMass bridge instead of the lane
  follower when Mass is on.
- **Far cars**: MassTraffic instances replace `AGTPFarTraffic`; the pool and ghost mode stay only under
  `-Traffic=legacy`.
- **Vehicle models**: MassTraffic needs several; City Sample Vehicles (free, Epic) and the free drivable pack
  listed by the reuse survey go out of Git through the Fab manifest. Until then the Clio is the only model.
- **Police**: Chaos actors that enter the Mass traffic; cars give way to sirens through a ZoneGraph
  annotation (step 4).

## Open questions (traffic)

- Lane-profile direction on junction polygon points: the importer faces "in" points along the lane and
  reverses the profile on "out" points; the first debug view on the PC confirms or flips it.
- How the ZoneGraph data is rebuilt after the import (automatic in the editor, or an explicit build call):
  checked on the first Mac run.
- Whether MassTraffic still needs City Sample's CityTrafficBuilder fixes on 5.8 (list in the README of
  Myxcil/MassTraffic-Test, read only, nothing copied).

## Open questions (pedestrians)

- Mass LOD thresholds between actor, ISM and AnimToTexture instances: to measure.
- How the lent-pedestrian API (`AcquireFromPool`, shops) maps onto Mass entities: the near actor pool stays.
