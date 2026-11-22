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

## The rule of a hold-up, as a player meets it

- **Aim a weapon at somebody closer than 8 m: he stops and puts his hands up**, and keeps them up for as long as the gun
  stays on him (up to 10 m once his hands are up, so a step back does not free him). Everybody does, whatever his
  character; only the brave (shop staff can be) backs away first.
- **Look away**: the fearful bolt after one second, the others lower their hands after four and walk on.
- **Aim from further than 8 m**: he has room to run, and runs after a second or two. A few freeze on the spot with their
  hands up instead (15 % of the crowd).
- **Fire**: the person held may bolt, everybody around runs. **Who runs already does not stop for a gun.** To hold somebody
  up, walk up with the weapon lowered and raise it close.

The range is `SurrenderRangeCm` (`GTPThreatLogic.h`, checked offline by `tools/ragdoll/check_impact_logic.py`: every
temperament surrenders at 4 m and stays held 30 s, 7.9 m holds and 8.1 m runs, the fearful bolts 1 s after the gun looks
away). Neither Lyra (no civilians) nor City Sample (no weapons) has such a rule to take; City Sample's crowd plays its
reactions as Mass montages (`FMassMontageFragment`), which our pooled bodies do not use.

The point of a body a gun is aimed at, and the one its fear tests, is the chest (`UGTPThreatResponseComponent::AimPointOf`).
It was 80 cm above the actor for everybody, the belly of a pedestrian of the crowd: a gun held level on him from 1.2 m was
not "on him", and `-GTARun`, which aimed there, dived on him with the camera (the pictures of the close hold-up at gates 46
to 49: the player seen from above, the person out of the frame).

One fear told everywhere: when the street sends a person running for its own reasons (a crash, a shot heard, panic passed
on), the body's state says "fleeing" too. `-GTARun` raises the gun only inside the range, on a person who is not running,
and pictures the two hold-up steps by day with the motion blur off (it says so in the log).

The picture of the close hold-up of candidate 51 (by day, camera level) is black, with only the HUD on it; the one at 3 m
shows the person standing with open hands over his head. `-GTARun` now says where the camera is when each picture is
taken ("GTARun hold-up probe: the camera is … from the player's eyes …; within 8 cm of it: INSIDE … | nothing; … screen
fade …"). The test does not move the camera: a black screen 1.1 m from somebody is what the player would see.

Read at candidate 57: at the close hold-up nothing blocks the camera and the screen is not faded, but the camera is beside
the player (33 cm ahead of his eyes, 1.5 m to his side, 154 cm from the head of the one aimed at) and that person is not in
the picture; at 3 m it is behind the shoulder and the picture reads. The follow camera (`GTPFollowCameraComponent`) hangs
on the view alone, and in play the weapon turns the body to the view while the gun is raised; the test also turned the body
to the person every frame, which no player can do. Since candidate 59 it leaves the body to the weapon while the gun is
raised, waits up to 3 s after the proof for the view to come within 8 degrees of the person before taking the picture, and
says what the picture holds: "GTARun hold-up picture: the one aimed at is IN THE PICTURE, under the reticle | IN THE PICTURE,
away from the reticle | NOT IN THE PICTURE (…): his chest is N degrees from the reticle (N to the right, N up, …); from the
line from the player to him, the view looks N degrees aside and the player's body faces N aside". The step still passes or
fails on the pose alone. **Not proven**: that this was the cause; the line decides at the next gate.
