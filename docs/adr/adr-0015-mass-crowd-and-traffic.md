# ADR-0015: Mass crowd and traffic

*French version: [adr-0015-mass-crowd-and-traffic.fr.md](adr-0015-mass-crowd-and-traffic.fr.md)*

## Status

Accepted for the pedestrians (2026-11-12). Corrected for the traffic on 2026-11-22: the default
traffic stays the legacy one, and MassTraffic is measured at every gate without deciding it.

## Date

2026-11-12, corrected 2026-11-22

## Context

The slice needs a busy street: more than 150 people and 40 cars around the player, on the
reference PC, inside the frame budget of ADR-0002. Our first systems (ADR-0005 for the
pedestrians, the actor traffic of `AGTPTrafficNetwork`) spend one actor per person and per car.
Unreal's Mass framework and City Sample's MassTraffic plugin (free, Epic code, never in Git)
were built for that density. On 2026-11-12 Nico chose to move both pedestrians and traffic to
Mass at once, replacing the "Mass later" option of ADR-0003 and ADR-0005.

## Decision

- **Pedestrians on Mass by default.** MassCrowd agents on the ZoneGraph generated from our data,
  a real character near the player (reactions, witnesses, ragdoll), animated instances further
  away. `-Pedestrians=legacy` keeps the old system.
- **Traffic: legacy by default, MassTraffic behind `-Traffic=mass`.** The ZoneGraph lanes,
  spawners and the `GTPMass` module stay; MassTraffic runs only when asked for.
- **Our code on top of Mass lives in `GTPMass`**, so the game builds and runs without City
  Sample's plugins.

## Correction of 2026-11-22: what the gate judges

Gates 52 to 57 judged MassTraffic, while the game, its `-GTARun` pass and its videos drive the
legacy traffic. MassTraffic did not pass a whole gate in that time (cars laid on their side,
then queues standing still behind people on crossings, then near cars without a body that the
player drives through). So, from candidate 60:

1. The gate judges what the player sees. The default pass measures the legacy traffic (cars
   moving, none lying, the player does not drive through them: the crash step of `-GTARun` runs on
   legacy cars too). This is blocking for `next`, together with the gate video.
2. MassTraffic leaves the critical path of milestone A (steal a car and escape the police). Its
   passes (`mass`, `gtarun -Traffic=mass`) are still measured at every gate: cars upright and
   moving from 120 to 300 s at 75.6 or more, at most 5 % lying within 100 m, near cars with a body,
   crash step. These criteria are informative. They become blocking again only for switching the
   default traffic to mass, once a whole gate passes them.
3. The threshold for the legacy traffic is set from the measure of gate 59, not before.

The criteria of each candidate are in `tools/perf/gate-spec.json`.

## Consequences

- The vehicle theft, the police chase and their videos are judged on the traffic the player
  actually meets.
- MassTraffic work continues, but no new MassTraffic lot is planned until the legacy measure of
  gate 59 has been read.
- Two traffic systems are kept side by side until mass passes a whole gate; the legacy code is not
  removed before that.

## Related

ADR-0002 (performance budget), ADR-0003 (movement), ADR-0005 (pedestrians),
[docs/gameplay/traffic-mass.md](../gameplay/traffic-mass.md),
[docs/gameplay/pedestrians-mass.md](../gameplay/pedestrians-mass.md).
