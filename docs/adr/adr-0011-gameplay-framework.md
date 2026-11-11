# ADR-0011: Gameplay framework for combat

*French version: [adr-0011-gameplay-framework.fr.md](adr-0011-gameplay-framework.fr.md)*

## Status

Proposed on 2026-11-11. Rules checked outside Unreal; the Unreal side is not written
yet. Accepted when the first Unreal slice (health, pistol, hit) passes the merge gate.

## Date

2026-11-11

## Decision Makers

nic0lasm (project lead)

## Summary

No Gameplay Ability System. Combat is a handful of plain components over rule headers
with no Unreal types (`GTPHealthLogic.h`, `GTPDamageLogic.h`, `GTPWeaponLogic.h`,
`GTPCombatMeleeLogic.h`, `GTPCoverLogic.h`), checked by `tools/combat/combat_logic_test.cpp`
in CI, as for the wanted level, the ragdoll and the audio. One world subsystem is the
single entry point for every hit and broadcasts what other systems listen to. No
StateTree for 0.7; it is the candidate for enemy groups from 0.11.

## Context

Version 0.7 ("Armed") needs bare-hands fighting, a pistol, cover, health and armour,
death and respawn, localised reactions, impacts per surface, people who flee guns and
police who shoot at two stars. Version 0.8 needs an armed threat for the shop hold-up
(the fast-food of Bellecour). Several systems already exist and own part of the answer:

- Ragdoll thread: ragdoll and get-ups (`UGTPRagdollComponent`, ADR-0006), localised hit
  reactions (`UGTPHitReactionComponent`, `GTPHitLogic.h`, branch feat/hit-reactions) and
  one fear for the whole game (`UGTPThreatSubsystem`, `UGTPThreatResponseComponent`,
  branch feat/threat-response). Their contract: combat **tells** them about aims, shots
  and resolved hits; they decide the reaction.
- Police (ADR-0004): `UGTPWantedComponent::ReportCrime`, `EGTPCrimeKind`.
- Pedestrians (ADR-0005): pooled bodies, `OnPedestrianHit` / `OnPedestrianScared`.
- Interiors (ADR-0012, branch feat/reference-interior): hold-up script, tills, alarm.
- Budget (ADR-0002): M1 Pro capped at 40 fps, 25.0 ms frames, GPU p95 ≤ 24 ms with the
  whole street running; "night and effects" has 2 ms of GPU for everything.

The game is single player, on a Mac M1 Pro and a Windows PC, with one weapon in 0.7 and
an arsenal only in 0.11.

## Decision

1. **Rules without Unreal types**, one header per concern, all numbers in settings
   structs that the components fill from `DefaultGame.ini` (`[/Script/GranTheftPraline.*]`):
   - `GTPHealthLogic.h`: health, armour that takes 70 % of a hit while it lasts,
     regeneration to half health after 6 s without damage, stamina, death timer,
     respawn grace, nearest hospital.
   - `GTPDamageLogic.h`: zone from the bone name, zone factors, distance falloff, the
     player takes 45 % of NPC damage and never more than 1.5 × on the head, the impact
     kind per surface (ids shared with the footstep map `GTPSurfaceLogic.h`) and the
     particle cap per impact kind.
   - `GTPWeaponLogic.h`: magazine and reserve, semi-automatic fire rate, reload that
     only fills at the end and can be cancelled, spread (hip, aimed, movement, bloom,
     blind fire ×4), recoil that comes back, seeded random numbers (a replayed fight is
     identical), and gamepad aim assist (snap on aim start, friction).
   - `GTPCombatMeleeLogic.h`: a fighter state machine (windup, strike, recovery,
     three-blow combo with a heavy third blow, dodge with invulnerability in its middle,
     guard that costs stamina and breaks, stun) and the takedown rule.
   - `GTPCoverLogic.h`: low and high cover from three traces, edges, how to aim from
     cover, the next cover in the stick direction, whether a body in cover is protected
     from a given shooter (for NPC decisions).
2. **Unreal components**, thin, owned by this thread:
   - `UGTPHealthComponent` on the player, police officers and shop staff. It is **the**
     health of a body. Pooled pedestrians keep a plain health value in the pool record
     (no component per pooled body).
   - `UGTPWeaponComponent`: holds a `GTPWeapon::FState`, traces the shot, used the same
     way by the player and by police AI (AI passes its own accuracy factor).
   - `UGTPCombatMeleeComponent`: one `GTPMelee::FFighter`, a sphere sweep on the landing
     step, montages by name.
   - `UGTPCoverComponent` (player only in 0.7): runs the probes when the cover key is
     pressed, never every frame.
   - `UGTPCombatSubsystem` (world): **the single entry point** `ApplyHit(FGTPCombatHit)`
     resolves damage, applies it to health, calls `UGTPHitReactionComponent::ReactToHit`,
     tells `UGTPThreatSubsystem` (`SetThreat` while an armed body aims, `NotifyShot` on
     each shot), spawns the impact effect from a capped pool, and broadcasts the
     delegates `OnShotFired`, `OnDamageDealt`, `OnPawnKilled`, `OnPlayerDied`,
     `OnPlayerRespawned`. It ticks the health of all registered bodies at 10 Hz. It
     handles death and respawn at the nearest fictional hospital (`ATargetPoint` tagged
     `GTPHospital`). The `-Combat=0` switch turns all of it off.
3. **Animation** stays on the GASP character: motion matching for the legs, an
   upper-body layer for the pistol (aim offset, recoil additive), montage slots for
   blows, dodges, reloads and takedowns, a looping pose set for cover. Clips are listed
   in `docs/animation/combat-sources.md`; missing clips are keyed by hand with the
   `tools/animation/` method.
4. **Boundaries** (files owned elsewhere, never rewritten here):
   - Ragdoll: reactions, ragdoll, fear. Combat calls them. The player's car and fall
     damage (`UGTPPlayerReactionComponent`) is routed to `UGTPHealthComponent` when one is
     present: a small additive change proposed to the ragdoll thread.
   - Police: subscribes to the delegates and calls its own `ReportCrime`. New crime kinds
     proposed to the police thread: `Assault`, `ShotsFired`, `Killing`, `KillPolice`,
     `ArmedThreat`. Police shooting AI uses `UGTPWeaponComponent`.
   - Pedestrians: read fear from `UGTPThreatResponseComponent` / `OnThreatState`, and
     pooled bodies take hits through `ApplyHit` with their pool health.
   - Camera and on-foot feel (`GTPFollowCamera*`, `GTPCameraMode*`): aim camera,
     shoulder swap and cover camera are specified here and proposed to their owner.
   - HUD (reticle, health, armour, ammunition, hit marker): specified here, drawn by the
     interface thread. Input actions: created here, remapping by the interface thread.
   - Sounds: specification to the audio thread, which makes and mixes them.
   - Engine config, cvars, budgets: proposed to the optimisation thread.

## Alternatives Considered

- **Gameplay Ability System** (as in Lyra). Strong for many abilities, gameplay tags,
  effects and cues, and network prediction. Against it here: the project is single
  player, has one weapon until 0.11, and checks its rules outside Unreal; GAS adds three
  modules, an ability system component per body (cost on pooled pedestrians and police),
  attribute sets that cannot be tested without the engine, and a long learning and
  compile loop on the Mac. The rule headers stay usable inside GAS abilities if the
  arsenal of 0.11 calls for it; that question is reopened then, not before.
- **Unreal's built-in damage** (`TakeDamage`, `ApplyPointDamage`). Kept out of the path:
  one entry (`ApplyHit`) is easier to measure, to switch off and to replay. Actors that
  only know `TakeDamage` (breakables) can still receive it from the subsystem.
- **StateTree for police combat now.** The 0.7 police decision (shoot, take cover,
  close in) is small enough for rules in the police thread; StateTree (plugin not enabled
  today) is the candidate for enemy groups in 0.11.
- **Physical animation for hit reactions** (partial ragdoll). The better look, but CPU
  cost on the M1 with several bodies; the ragdoll thread's flinch and stagger clips come
  first, physical animation is a "fidelity" profile option later.

## Consequences

- Combat code depends on feat/hit-reactions and feat/threat-response: those land on
  next first, then combat's Unreal code is based on them.
- Two health values exist until the ragdoll thread routes `UGTPPlayerReactionComponent`
  to `UGTPHealthComponent`.
- Numbers (damage, time to kill) are set in one place and their balance is checked by
  the test: an officer falls to 5 torso or 2 head shots of the pistol at 10 m; the player
  survives 12 such hits without armour and 23 with full armour; a full fist combo does
  not kill.
- Effects have a hard cap (see the budget in `docs/gameplay/combat.md`); when it is
  reached the oldest impact disappears, the game never waits for an effect.

## Validation Criteria

- `tools/combat/combat_logic_test.cpp` passes in CI (`Combat rules` job), and the headers
  are clean under strict conversion warnings (MSVC is stricter than g++).
- Control compilation on the Windows PC (editor and game, Win64) of every combat branch.
- M1 bench, step C capped at 40 fps, 4 scenes × 120 s plus a 2-minute firefight scene:
  ≥ 99 % of frames at 25.0 ms ± 1, no frame over 100 ms, GPU p95 ≤ 24 ms; A/B `-Combat=0`
  within the combat budget line.
- Video of a 2-minute fight (fists, pistol, cover, death, hospital).

## Related

ADR-0002 (budget), ADR-0003 (movement), ADR-0004 (wanted level), ADR-0005
(pedestrians), ADR-0006 (ragdoll and reactions), ADR-0009 (audio mix), ADR-0012
(interiors), docs/gameplay/combat.md, docs/animation/combat-sources.md.
