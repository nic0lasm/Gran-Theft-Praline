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
| DetailDistance, FarLOD | 20 m, 2 | shadow and full LOD inside, lower LOD outside |
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

- Pauses: about a third of the stops are phone calls (they stand in place, with
  `PhoneAnimation` if one is set), the rest are window shopping: they walk over to the
  facade (`facade_side` of the walkway) and face it. A car passing within 3 m on
  their walkway makes them step away from it. People are frightened from outside by
  `ScareAround` (the audio thread calls it for shots, explosions and crashes).

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

## Lending pedestrians (shops, scenes)

`UGTPPedestrianSubsystem::AcquireFromPool(Location, YawDegrees)` lends one pedestrian of the pool
(idle, drawn with full detail, left alone by the pool) and `ReturnToPool(Actor)` gives it back.
A free one is taken first, else the walker farthest from the player (never a ragdoll). Returns
`nullptr` when pedestrians are off or nobody can be lent. The borrower plays its own loop with
`AGTPPedestrian::PlayLoop` and must return the actor; a borrowed one counts in `GetBorrowedCount()`.

## Living street: panic, curiosity, demo events

- **Panic spreads.** Whoever starts running (a fast car, a shot, a crash) passes the panic once to the
  quiet people within `PanicRadius` (7 m, 65 % each, at most two steps: `PanicMaxGeneration`), so a
  group scatters like a herd instead of one person at a time (`GTPWalk::CatchPanic`).
- **Curiosity.** Beyond the scare radius of a loud sound (2.5 times it) and around a car hit
  (25 m), people stop and look at the spot for 4 to 9 s (`GTPWalk::Gawk`, pause kind Gawk, facing
  the event), then walk on. Not those already running, not ragdolls, not lent ones.
- **Demo events** for filming and measuring: `-PedDemo=panic|gawk|shot` repeats that event every 25 s
  next to the closest drawn person (first one 8 s after the start). `panic` scares 4 m around,
  `gawk` plays a car hit (hit sound and gawkers), `shot` sends a loud sound through
  `UGTPAudioWorld::OnLoudSound` (the full chain: runners, then gawkers).
- Checked offline: `tools/pedestrians/check_walk_logic.py` (panic never beyond the 2nd generation, every
  gawker faces the event and walks on afterwards, nobody teleports).
- **Held up.** With `-ThreatResponse=1` every pooled pedestrian gets a `UGTPThreatResponseComponent`
  (temperament: 45 % flee, 40 % comply, 15 % frozen). `OnThreatState` drives them: alarmed = stand and
  look, hands up / complying = frozen facing the aggressor (`EPause::HandsUp`, until the fear drops),
  backing / fleeing = run away from him. `-ThreatDemo` (threat subsystem) exercises it.
- Fix: a walker turned round by `Flee` no longer jumps sideways (its lateral offset is mirrored with the direction).
