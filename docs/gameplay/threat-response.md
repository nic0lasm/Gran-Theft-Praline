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
Fear decays 0.2 per second two seconds after the threat is gone. An unarmed hand frightens no one.

## Contract

- In: `UGTPThreatSubsystem::SetThreat(Source, bArmed, Origin, AimDirection)`, `ClearThreat()`, `NotifyShot(From, Target)`.
  Without the combat system, `-ThreatDemo` makes the player an armed person aiming where the camera looks.
- Out, per person: `OnThreatState(Body, Old, New)`, `GetFear()`, `GetSecondsSinceAimed()`, `ObeysOrders()`,
  `OnFledToTarget(Body)` after `SetFleeTarget(location)` (the shop's back door).
- Owned by the shop script, not here: till, cash, alarm, `ReportCrime`, which temperament each employee gets.
- Characters (shop staff) back away and run by themselves; pooled pedestrians are moved by the pedestrian system,
  which reads `OnThreatState` (`Fleeing` replaces its own scare).
- Clips (assign by name in `Clips`): `AM_HandsUp_Enter`, `AM_HandsUp_Idle` (tools/animation/car_poses.json).

### Bodies of the crowd change hands

The 24 near bodies of the Mass crowd are pooled: each is given to the nearest people in turn. When a body leaves a
person, the person's hands-up pause ends (nobody is left to lower his hands: he stood 60 s, and on a crossing the cars
waited for him), and the body forgets the fear, the state and the death of that person
(`UGTPThreatResponseComponent::Forget`), so the next one starts calm. While the player holds somebody up on a crossing
the cars do wait: that is the hold-up.

### What a hold-up must show

`-GTARun` passes its two hold-up steps only when the person aimed at stands upright with both hands higher than his head,
read on the bones of his body 1 s after his state says "held up"; a state that says so for 5 s on a body that does not
show it fails at once with the pose ("hands up in his state, not on his body"). The gate of candidate 48 passed both
steps on the count of states while the person aimed at ran and another walked into the player.

For a person who has one of the 24 bodies, the body's fear is the only authority: every frame the walker is put in line
with it (held up = standing, fleeing = running), and a loud sound or the panic of the walkers leaves a person held at
gunpoint alone. The loops (walk, idle, run, hands up) baked on the pack's own skeleton are taken when they exist
(`/Game/MetaHumans/GTP/AnimsCrowd/CS_*`, `retarget_player_animations.py` with `GTP_RTG_TARGET_MESH`, `GTP_RTG_PREFIX=CS_`,
`GTP_RTG_FOLDER`). The log says the pose of a body 2 s after each kind of animation begins ("crowd: pose of …").

### The hands-up pose

No Epic pack we have holds a hands-up animation, so the loop is made by `tools/pedestrians/make_handsup_animation.py` from
the idle, on each skeleton that plays it (UEFN mannequin `A_HandsUp`, MetaHuman `MH_A_HandsUp`, City Sample crowd
`CS_A_HandsUp`): upper arms up and a little out, forearms nearly vertical, palms forward. The pose is solved in the space
of the body from where the shoulder, elbow, wrist and thumb are, so it does not depend on the axes of a skeleton, and the
script proves its result by reading the saved asset ("HANDSUP RESULT …: OK (hands N and N cm above the head …)"). Its
solver is checked without the editor (`GTP_HANDSUP_SELFTEST=1 python3 tools/pedestrians/make_handsup_animation.py`).
The first version wrote fixed local angles with the position keys at zero: the arms lost their length and the hands
stopped 10 to 19 cm under the head on the crowd's bodies, which the strict test of candidate 49 showed in the editor.

A person run over by the player's car stays down where his body lies until the body is given to somebody else: he used
to walk on unseen after 10 s, and his state could go "hands up" under a gun while the body lay on the road.

A body of the crowd that fell (ragdoll) comes back upright when it is given to the next person: a simulating skeletal mesh
stays attached and takes the turn of its pelvis, so its place and turn under the actor are kept when it falls and put back
(`AGTPPedestrian::ResetBody`). Before, the body stayed lying, without physics, for everybody it was given to afterwards.
The 10 s line of the crowd ends with "fallen N, lying without physics N": the second number must be 0.
`-CrowdOwnLoops=0` keeps the `MH_` loops instead of the `CS_` ones.
Only a car knocks people of the crowd down: the player on foot runs at 500 cm/s, right on the speed threshold, and used to
knock down like a car the people he came within 1.7 m of.
