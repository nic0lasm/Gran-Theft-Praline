# ADR-0005: Pedestrians

*French version: [adr-0005-pedestrians.fr.md](adr-0005-pedestrians.fr.md)*

## Status

Proposed on 2026-11-10 (the first measurement on the M1 Pro decides).

## Date

2026-11-10

## Decision Makers

nic0lasm (project lead)

## Summary

People walk the sidewalks of rue de la République on a graph of walkways built
from BD TOPO, moved by a small engine-free rule set, drawn by a fixed pool of
cheap skeletal meshes. Mass is kept as the later option if hundreds are needed.

## Context

The street must feel lived in, GTA style: people who walk, stop at a window,
wait at a crossing, step aside, run from a fast car. The budget is already
tight (ADR-0002: p95 <= 42 ms on the M1 profile) and six animated drivers cost
about 10 ms of GPU in version 0.3. Mass (MassCrowd, ZoneGraph, StateTree) is
built for thousands of agents and needs a zone graph authored in the map, a
binary that several threads edit; its representation layer would still need
tuning for Metal.

## Decision

1. **Walkways:** `tools/city/pedestrians.py` builds an undirected graph from the
   same BD TOPO roads as the ground mask: one walkway on each side of a car
   street, three on a pedestrian street, links at junctions marked "crossing"
   or "join". It is data (a CSV staged with the game), not map content.
2. **Behaviour:** `GTPWalkLogic.h`, no Unreal types, checked offline by
   `tools/pedestrians/walk_sim` (same approach as the AI drivers): random
   walk with a preference for sidewalks, pauses, waiting at a crossing when a
   car is near, fleeing along the graph away from a threat.
3. **Runtime:** `UGTPPedestrianSubsystem` (a world subsystem: no change to the
   map or the game mode) keeps a pool sized by the graphics profile (12 on m1),
   moves it in one tick, and spawns or retires people only out of sight.
4. **Cost limits:** one looping animation per person (no animation blueprint),
   at most `MaxVisible` drawn (the closest), no shadow and a lower LOD beyond
   25 m, nothing drawn beyond 90 m, hidden bodies do not pose.
5. **Events for other systems:** `OnPedestrianHit` and `OnPedestrianScared`
   on the subsystem (location, cause) for the police and wanted level.

## Alternatives Considered

- Mass crowd + StateTree now: more scalable, but a zone graph in the map,
  heavier plugins and an unmeasured Metal cost; the walk graph and the events
  here carry over to a Mass crowd if 0.9 needs hundreds.
- Characters with CharacterMovement and motion matching (the player's
  stack): roughly an order of magnitude dearer per person.
- Vertex-animated instanced meshes: cheapest for far crowds, but
  no reaction to a hit and needs an authoring step for each mesh; reconsidered
  for the far crowd.

## Consequences

- People walk through each other (no mutual avoidance, only a sidestep around
  the player); the sideways spread keeps it rare.
- One mesh and animation set at first: variety needs more meshes (Meshes
  config) and, later, tinted materials.
- The cost is unmeasured until a Mac slot: the validation below decides the
  default density.

## Validation Criteria

- `tools/pedestrians/check_walk_logic.py` PASS on the street's real walkways.
- Packaged m1 build, 3 minutes on foot in the street with 12 people:
  p95 <= 42 ms and at most +1.5 ms GPU and +0.5 ms game-thread CPU against the same run with `-Pedestrians=0`.
- A car at 50 km/h at people on the road makes them run; one on a crossing
  makes them wait.

## Related

ADR-0002 (frame budget), ADR-0003 (movement), docs/gameplay/pedestrians.md.
