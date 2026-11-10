# ADR-0003: Movement (on foot, driving, non-player drivers)

*French version: [adr-0003-movement.fr.md](adr-0003-movement.fr.md)*

## Status

Accepted on 2026-11-10 for version 0.3 (applies the stack chosen by the
project lead on 2026-11-09: free Epic building blocks, police and shops in
house).

## Date

2026-11-10

## Decision Makers

nic0lasm (project lead)

## Summary

How the player and the cars move: the character keeps `CharacterMovement`
with the Game Animation Sample (motion matching); cars use Chaos Vehicles
with tuning in C++; non-player drivers follow lanes built from BD TOPO with
an engine-free decision header, simulated outside Unreal.

## Context

Version 0.3 must let the player steal a parked car or pull a driver out
and drive from rue de la République to the Rhône. Unreal 5.8 offers several
movement systems: `CharacterMovement` (mature), `Mover` (experimental),
Chaos Vehicles (marked experimental in 5.8.2 but the only maintained vehicle
physics), and for traffic MassTraffic / ZoneGraph (City Sample, heavy,
ruled out as a base in the asset survey). The target machine is an M1 Pro,
30 fps (ADR-0002).

## Decision

1. **On foot:** `CharacterMovement` + GASP motion matching, as shipped in
   0.1. `Mover` is reconsidered once it leaves experimental status.
2. **Cars:** Chaos Vehicles (`AWheeledVehiclePawn`), base tuning in
   `AGTPVehicle` (city car, front-wheel drive, automatic), each model a
   Blueprint child (docs/gameplay/vehicles.md).
3. **Non-player drivers:** lanes from BD TOPO (`tools/city/traffic.py`),
   pure pursuit steering and speed rules in `GTPDriveLogic.h`, which has no
   engine types so `tools/vehicles/traffic_sim` checks it on the real lanes.
   Cars spawn and disappear only out of sight. No navigation mesh needed.
4. **Engine sound:** synthesised from the Chaos rpm (`GTPEngineSynth.h`),
   checked offline, until recorded loops exist.

## Alternatives Considered

- `Mover` for the character: experimental, GASP is built on
  `CharacterMovement`.
- MassTraffic / ZoneGraph for traffic: built for thousands of cars and a
  City Sample road network; too heavy to adapt for 6 cars in 0.3. Kept as an
  option for 0.9 (hundreds of cars) if our lanes do not scale.
- Behaviour trees and a navigation mesh for drivers: harder to test without
  the editor, and a navmesh over the whole city costs memory on the M1.

## Consequences

- Driving AI and engine sound can be developed and tested in the cloud;
  only tuning and measurements need the Mac.
- Junction priorities, lights and lane changes are missing until 0.9; a
  brief touch at a junction can happen.
- Chaos Vehicles may change between engine versions: re-check the tuning
  after each engine update.

## Validation Criteria

- `traffic_sim` PASS on the lanes of the current map (lane offset p95
  < 1.5 m, p99 < 3 m, mean speed > 12 km/h).
- In the packaged game: carjack a driven car, drive 3 minutes, 95 % of
  frames ≤ 33.3 ms on the M1 profile while driving with 6 AI cars.

## Related

ADR-0002 (frame budget), docs/gameplay/vehicles.md, production/roadmap.md (0.3).
