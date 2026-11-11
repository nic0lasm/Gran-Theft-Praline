# Threat response: people held up (version 0.7 hold-ups)

*French version: [threat-response.fr.md](threat-response.fr.md)* · ADR-0006 · switch: `-ThreatResponse=1` (off by default)

One fear for the whole game. The combat system tells the world who aims at whom; each person
decides what to do about it. The shop hold-up script (feat/reference-interior) and the
pedestrians read this fear; they do not compute their own.

Code: `GTPThreatLogic.h` (rules, no Unreal types, checked by `python3 tools/ragdoll/check_impact_logic.py`),
`GTPThreat.h` (`UGTPThreatSubsystem`, `UGTPThreatResponseComponent`).

## States

Calm, Alarmed, HandsUp, **Complying** (hands up and obeying orders: the shop script opens the till),
Backing, Fleeing, Down (hit by a knockdown or killed, via `UGTPHitReactionComponent`).

## Temperaments (set per person at creation)

| Temperament | Behaviour |
| --- | --- |
| Compliant | hands up at fear 0.4, obeys after 1 s, runs only at 0.92 (a shot, a blow, panic around) |
| Frozen | hands up, obeys after 0.5 s, never runs, even under fire; after a fall, hands up again |
| Brave | fear builds at half speed; backs away when aimed at within 4 m; hands up at 0.65; runs at 0.97 |
| Fleeing | runs at 0.45 |

Fear grows while an **armed** person aims at him (cone 12 degrees, 20 m, line of sight): 0.9 per second at point
blank, 0.2 per second at 20 m, but aiming alone never goes past 0.8. A shot at him adds 0.6, a shot within 18 m
adds 0.35, a person within 12 m who is afraid adds up to 0.35 per second (contagion), a flinch adds 0.25.
Fear decays 0.12 per second two seconds after the threat is gone. An unarmed hand frightens no one.

## Contract

- In: `UGTPThreatSubsystem::SetThreat(Source, bArmed, Origin, AimDirection)`, `ClearThreat()`, `NotifyShot(From, Target)`.
  Without the combat system, `-ThreatDemo` makes the player an armed person aiming where the camera looks.
- Out, per person: `OnThreatState(Body, Old, New)`, `GetFear()`, `GetSecondsSinceAimed()`, `ObeysOrders()`,
  `OnFledToTarget(Body)` after `SetFleeTarget(location)` (the shop's back door).
- Owned by the shop script, not here: till, cash, alarm, `ReportCrime`, which temperament each employee gets.
- Characters (shop staff) back away and run by themselves; pooled pedestrians are moved by the pedestrian system,
  which reads `OnThreatState` (`Fleeing` replaces its own scare).
- Clips (assign by name in `Clips`): `AM_HandsUp_Enter`, `AM_HandsUp_Idle` (tools/animation/car_poses.json).
