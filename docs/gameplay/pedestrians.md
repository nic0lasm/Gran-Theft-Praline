# Pedestrians

*French version: [pedestrians.fr.md](pedestrians.fr.md)*

Decision and alternatives: [ADR-0005](../adr/adr-0005-pedestrians.md).

## What the player sees

People walk the sidewalks and the pedestrian street, stop now and then to look
around, wait at the kerb when a car comes at a crossing, step aside when the
player walks into them, run from a car coming fast at them, and are knocked
down (ragdoll, 8 s) if it hits them.

## Data

```
python3 tools/city/pedestrians.py --map L_City_RueDeLaRepublique
```

writes `gen/<focus>/pedestrians/walkways.csv` (+ `.json`, `.png` check view) and
copies it to `GranTheftPraline/Content/City/Pedestrians/<Map>.walkways.csv`
(not committed, staged with the game). Without that file the game has no
pedestrians. Edge kinds: 0 sidewalk, 1 pedestrian street, 2 crossing, 3 join.

## Runtime

`UGTPPedestrianSubsystem` (GTPPedestrians.h) creates itself in game worlds.
Settings are in `DefaultGame.ini`, section `GTPPedestrianSubsystem`:

| Setting | Default | Meaning |
|---|---|---|
| DensityM1 / Fidelity / Ultra | 12 / 24 / 40 | people per graphics profile; `-Pedestrians=N` overrides, 0 = off |
| MaxVisible | 10 | only the closest ones are drawn |
| DetailDistance, FarLOD | 25 m, 2 | shadow and full LOD inside, lower LOD outside |
| DrawDistance | 90 m | nothing drawn beyond |
| WalkAnimation, IdleAnimation, RunAnimation | empty | loops to play; no walk animation = no pedestrians |
| Meshes | empty | skeletal meshes; empty = the player's character mesh |

Find candidate assets with `tools/pedestrians/find_walk_assets.py` (editor
Python). Free first: the Game Animation Sample mannequin and its locomotion
loops need no purchase; MetaHuman or Fab meshes can be listed in `Meshes` later.

Other systems listen to `OnPedestrianHit` and `OnPedestrianScared`
(location, cause actor).

Stable interface for other systems (the wanted system subscribes, this module
depends on none of them):

```cpp
DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPPedestrianEvent, const FVector& Location, AActor* Cause);
UGTPPedestrianSubsystem::OnPedestrianHit;    // a person was knocked down
UGTPPedestrianSubsystem::OnPedestrianScared; // a person started running away
```
Cause is null for now (the car is not identified yet).

## Hooks and variety

- `ScareAround(Location, Radius, Cause)` makes everybody nearby run away (shots,
  explosions, police). It broadcasts `OnPedestrianScared` for each person.
- `CrossingBlockedQuery` (a `TFunction<bool(const FVector& Where, const FVector& WalkDirection)>`) lets the traffic
  lights say a crossing is red for people; unset, they only look out for cars.
- Variety from one mesh: height varies by `HeightSpread` and the body materials
  get a clothing colour from a small city palette through the vector parameters
  of `TintParameters` (those a material lacks are ignored). More meshes go in `Meshes`.

- Street behaviour: a quarter of the people walk in pairs (the partner stays beside
  and a little behind and does what the leader does), and people closer than 90 cm
  step sideways round each other (`Separate` in `GTPWalkLogic.h`, checked at 12 and
  60 people by `walk_sim`).

## Checks

- `python3 tools/pedestrians/check_walk_logic.py [walkways.csv]` builds
  `walk_sim` and runs 200 walkers for 10 minutes (no jumps, they keep walking,
  they flee from a threat). Python tests: `tools/city/tests/test_pedestrians.py`.
- Cost on the M1 Pro: not yet measured (ADR-0005 validation criteria). The
  stats line `LogGTPPedestrians: active=.. drawn=..` appears every 10 s, also in
  Shipping.

## Limits

No avoidance between people, one mesh and one animation set, no sitting or
shop scenes, no reaction to shots yet (the events exist for it).
