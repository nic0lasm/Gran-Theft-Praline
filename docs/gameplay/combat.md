# Combat and weapons (version 0.7 "Armed")

*French version: [combat.fr.md](combat.fr.md)* · ADR-0011 · switch: `-Combat=0` (off)

Status 2026-11-11: rules written and checked outside Unreal (`tools/combat/combat_logic_test.cpp`,
150 checks, CI job `Combat rules`). Unreal components not written yet: they wait for the ragdoll
thread's hit reactions and threat response to reach `next`.

## Numbers (defaults, all in `DefaultGame.ini` once the components exist)

| What | Value |
| --- | --- |
| Health, armour, stamina | 100, 0 to 100, 100 |
| Armour | takes 70 % of a hit while it lasts; melee and falls go through it |
| Regeneration | after 6 s without damage, 4 points/s, up to 50 (the rest: medkit, food, hospital) |
| Respawn | 6 s after death, nearest fictional hospital, 3 s without damage, armour lost |
| Pistol (fictional 9 mm) | 12 rounds, reserve up to 96, 330 rounds/min semi-automatic, reload 1.5 s (auto when empty) |
| Pistol damage | 20 at the torso up to 15 m, down to 10 at 45 m; head × 2.5, arm × 0.6, leg × 0.7 |
| Spread (half-angle) | hip 3°, aimed 0.5°, +0.7° per shot up to +4°, recovers 6°/s, +2° running, blind fire × 4 |
| Recoil | 1.4° up per shot (± 0.35° sideways), comes back at 10°/s |
| Player as target | takes 45 % of NPC damage, head never more than × 1.5 |
| Fists | 8, 10, then 18 (heavy, knocks down); 8 stamina per blow; combo of 3 |
| Dodge | 0.45 s, invulnerable from 0.05 to 0.30 s, 18 stamina |
| Guard | stops 80 % of a frontal blow (± 70°) for 14 stamina; heavy blow or no stamina = guard broken, stunned 1.2 s |
| Takedown | within 1.5 m, from behind (± 60°) on someone unaware, or on anyone stunned; never on someone aiming a gun |
| Aim assist (gamepad) | snap within 9° and 40 m on aim start (hostiles preferred), friction × 0.45 within 3.5° |
| Cover | taken within 1.2 m of a wall facing the player (± 50°); low (car, low wall) or high; jump to a cover up to 12 m in a 40° cone |

Balance checked by the test: an officer falls to 5 torso or 2 head shots at 10 m; the player survives 12
torso hits without armour, 23 with full armour; a full fist combo (36) never kills a fresh body.

## Contract for the other threads

**Out (delegates of `UGTPCombatSubsystem`):**

| Delegate | Arguments | Who listens |
| --- | --- | --- |
| `OnShotFired` | shooter, origin, direction, weapon, the bullet's `FHitResult` and whether it hit | Police (`ShotsFired`), Audio (gunshot, tail, echo by street), Pedestrians if they need more than fear |
| `OnDamageDealt` | instigator, victim, damage, zone, melee or not, health after | Police (`Assault`, `HitPolice`), HUD (hit marker), Audio (impact) |
| `OnPawnKilled` | killer, victim, melee or not | Police (`Killing`, `KillPolice`), missions, statistics |
| `OnPlayerDied` / `OnPlayerRespawned` | place, hospital | Police (clear the level), economy (hospital bill if wanted), save, HUD |
| `OnArmedThreat` | source, target, aiming or not | Hold-up script (fast-food), Police (`ArmedThreat`) |

**Calls this thread makes:** `UGTPHitReactionComponent::ReactToHit(FGTPHitInfo)` after each hit;
`UGTPThreatSubsystem::SetThreat(Source, bArmed, Origin, Aim)` every frame an armed body aims, `ClearThreat()`
when the aim ends, `NotifyShot(From, Target)` at each shot. This replaces `-ThreatDemo`.

**Proposed to the police thread:** new `EGTPCrimeKind` values `Assault` (fists, 15 heat), `ShotsFired` (20),
`ArmedThreat` (25), `Killing` (60), `KillPolice` (90). At two stars, officers shoot with `UGTPWeaponComponent`
(AI accuracy factor 2.5 × the player's spread, one shooter at a time per unit in 0.7).

**Proposed to the ragdoll thread:** when the body has a `UGTPHealthComponent`, `UGTPPlayerReactionComponent`
sends its car and fall damage there instead of its own `Health`.

**HUD (interface thread):** small dot reticle that opens with the spread (radius = spread angle on screen);
hit marker (a cross, 0.15 s, red on a kill); health and armour as two thin arcs around the mini-map, shown
only when changed or below 50 %; ammunition `12 / 36` bottom right only while armed; red vignette below 25 %
health; cover prompt and takedown prompt in the existing help corner; aim assist on/off and sensitivity in
the controls menu.

**Sounds (audio thread):** pistol shot close (dry, short) and far (crack and tail, delayed by distance),
street echo by width of the street (the zone map exists), dry click, magazine out and in, slide; bullet
impacts per impact kind (dust, sparks, car body, glass, wood, dirt, water, flesh); whizz of a near miss;
fist blows light and heavy, a block, a dodge swish; body falls exist already (ragdoll). Gunshots must duck
the ambience and traffic for 0.5 s.

**Controls (created here, remapped by the interface thread):**

| Action | Keyboard and mouse | Gamepad |
| --- | --- | --- |
| Aim (guard when unarmed) | right button | left trigger |
| Fire (punch when unarmed or not aiming) | left button | right trigger |
| Reload | R | X / Square |
| Dodge | Left Alt | B / Circle |
| Cover (take, leave) | Q | right bumper |
| Swap shoulder | X | d-pad left |
| Holster, draw | 1 | d-pad down (d-pad up is the big map) |

To be checked against the existing actions (`F` enters a car, `E` interacts, `Space` jumps) by the
interface thread.

## Effects budget (proposed to the optimisation thread)

Line in the "night and effects" envelope (2 ms GPU), for the worst case of a firefight on the street at
step C, M1 profile, capped at 40 fps:

| Item | Cap on M1 | Cost target |
| --- | --- | --- |
| Impact effects (Niagara, CPU simulation, no lights, no collisions) | 12 alive at once, 0.6 s life, particles per kind 8 to 48 (`GTPDamage::MaxParticles`) | ≤ 0.25 ms GPU p95 |
| Impact decals (pooled, no normal-map blending) | 32 in the world, oldest recycled, 20 s fade | ≤ 0.05 ms GPU |
| Blood ("GTA-like", chosen by Nico on 2026-11-11) | a pool decal under each dead body (8 at most, 90 s, fade 10 s); stains on clothes through the material scalar `GTPBlood` (0 to 1, +0.25 per bullet), no extra draw | ≤ 0.05 ms GPU |
| Muzzle flash | sprite + a light of 0.05 s **without shadows**, player only; NPC shots: sprite only | ≤ 0.05 ms GPU |
| Smoke | one small puff per shot (≤ 6 particles, low overdraw), none in rain | included above |
| Tracers | off on M1, on in "fidelity" | 0 on M1 |
| Glass breaking (shop windows) | existing breakables (ragdoll thread's budget) | not counted here |
| Game thread (traces, health ticks at 10 Hz, aim assist) | 3 shooters, 1 trace per shot, cover probes on key press only | ≤ 0.4 ms CPU |

Total proposed (blood included): **≤ 0.35 ms GPU p95 and ≤ 0.4 ms game-thread CPU** in the firefight scene, A/B against
`-Combat=0`. When a cap is reached the oldest effect is removed; nothing waits. A "fidelity" profile can
double the caps and enable tracers.

Agreed by the optimisation thread on 2026-11-11 with three amendments, now part of the budget:

- The margin it comes from: on next 8b16340 the gate's GPU p95 is 20.05 / 22.31 / 22.27 / 22.54 ms (fixed /
  route / night / rain) against 24 ms, so 1.5 ms of margin in the worst scene; this line takes a quarter of it.
- Checked **at night and in the rain**, not only at noon (translucent sparks, smoke and glass cost more there).
  Hard caps on live particles and decals, pooled and recycled; no dynamic light casts shadows; muzzle flashes
  never cast shadows.
- **No frame over 100 ms at the first shot**: Niagara systems and their PSOs are preloaded when the level starts
  (a hidden warm-up spawn), and the first shot is measured cold.

## Plan

1. Rules outside Unreal (done, this branch).
2. Once feat/hit-reactions and feat/threat-response are on next: `UGTPHealthComponent`,
   `UGTPCombatSubsystem` (`ApplyHit`, delegates, `-Combat=0`), `UGTPWeaponComponent` with the pistol and
   impact effects, hospital respawn. Control compilation on the PC, then a Mac slot (gate + firefight scene).
3. Fists, dodge, guard, takedown (`UGTPCombatMeleeComponent`).
4. Cover (`UGTPCoverComponent`), blind fire.
5. Police shooting at two stars (with the police thread), 2-minute fight video.

## GTA-like shooting (batch of 13/11)

What the batch adds on top of the above:

- **Two weapons** (rules in `GTPWeaponLogic.h`, namespace `GTPArsenal`, tested outside Unreal):

  | | 9 mm pistol (fictional) | Carbine (fictional) |
  |---|---|---|
  | Fire | semi-automatic, 330 rpm | automatic, 600 rpm (trigger held) |
  | Magazine / starting reserve | 12 / 36 | 30 / 90 |
  | Reload | 1.5 s | 2.2 s |
  | Range | 60 m | 90 m |
  | Spread hip / aimed | 3° / 0.5° | 4.5° / 0.6° |
  | Recoil per shot | 1.4°, back at 10°/s | 0.75°, back at 4°/s (bursts climb) |
  | Torso damage at 10 m | 20 (officer: 5 shots) | 26 (officer: 4 shots) |
  | From a car | yes (one hand) | no (two hands) |

  The player carries both; police and shop staff only the pistol. Switching takes 0.45 s.
- **Over-the-shoulder aim**: while aiming, the on-foot camera (`UGTPFollowCameraComponent`) comes in to 135 cm
  behind the shoulder, 58 cm to the side, 62° field of view, no auto-recenter. The shoulder swap slides across.
  `Aim*` settings in `DefaultGame.ini`.
- **Gamepad aim assist**: the left trigger snaps to the body nearest the reticle (9°, 40 m, police first, never
  a dead body or someone behind a wall). The mouse is never assisted.
- **Drive-by**: at the wheel, aiming (right mouse, or left bumper on the gamepad) takes the pistol out; left
  mouse, or the right bumper while aiming (the handbrake comes back afterwards), fires out of the side windows or
  the windscreen, never straight back (`GTPDriveBy`: 150° either side, -30° to +35°). Spread ×1.6, growing with
  speed (+2.5° at 108 km/h). The player's car is never hit by its own driver's shots.
- **Police and panic**: every player shot goes to the threat system (pedestrians flee, the Mass crowd too, and
  traffic brakes or flees). Crimes go through one table only, the police thread's
  (`UGTPWantedComponent::BindCombat`, docs/gameplay/wanted.md), bound to the combat delegates: shots fired, armed
  threat, assault, officer hit, killing, police killing; witnesses decide whether it counts. Combat guarantees
  two things to that table: `OnArmedThreat` fires only when someone is in the sights (12°, 25 m), not when aiming
  at an empty street; at the wheel the shooter announced is the player's car (`UGTPWeaponComponent::GetShooter`),
  so a shot through the window counts like a shot on foot. `UGTPCombatSubsystem::IsBenchRunning()` tells
  listeners the `-CombatBench=1` bench is running.
- **Lyra** (Epic content, out of Git, migrated on the PC with its original paths): muzzle flash
  `NS_WeaponFire_MuzzleFlash_Rifle` (warmed up at load), impacts `NS_ImpactConcrete` and `NS_ImpactGlass`
  (`ImpactSystems`). Weapon meshes and animations: paths to fill in `Looks` and `Clips` (`UGTPWeaponComponent`)
  from the PC's report. Each missing asset is logged once; the game runs without it.

Added controls: next weapon = mouse wheel down, 2, or left bumper on foot; previous weapon = wheel up.

### Lyra port (evening of 13/11)

From the PC's survey (`/mnt/project-files/combat/lyra-armes-valeurs.md`). Epic content, out of Git; no Lyra code
is taken: Lyra stands on GAS and ShooterCore, which ADR-0011 leaves out. We take its assets.

- **Guns in the hand**: `SK_Pistol` and `SK_Rifle` (Lyra paths), shots and flash from their `Muzzle` socket,
  visible only when drawn. Placement: the player's `weapon_r` socket when it exists, else `hand_r` with
  `AttachOffset` (set from the script's report).
- **Animations**: the Lyra montages (Fire, Reload, Equip, DryFire for both weapons) and the aim poses
  `MM_*_Idle_ADS` are on the UE5 Mannequin skeleton; the player is a UEFN Mannequin. The script
  `tools/combat/unreal/lyra_weapons_setup.py` (editor, Python) retargets them into `/Game/Combat/Weapons/Anim/`
  (suffix `_UEFN`), lists the slots in the player's animation blueprints, says whether `weapon_r` exists, and
  writes the `DefaultGame.ini` lines matching what really exists (`Saved/GTPCombat/lyra-weapons-setup.txt`).
- **Slots**: Lyra plays Fire in `FullBodyAdditivePreAim` and the rest in `UpperBody`. If the player's animation
  blueprint lacks them, a clip can force its slot (`Clips=(…,Slot="DefaultSlot")`); an additive animation never
  plays in a non-additive slot. The aim pose (`_AimLoop`) is held for the whole aim, in its slot. Without an
  upper-body slot it stays off: that is the main blocker, lifted by a change to the animation blueprint
  (`UpperBody` slot + AimOffset), a Content asset.
- **Impacts**: `NS_ImpactConcrete` and `NS_ImpactGlass` (if they do not depend on Lyra's data channels), bullet
  hole decals: ours (`M_GTP_BulletHole`, made by `make_blood_material.py`) and Lyra's on glass. 24 at most, 30 s,
  attached to what was hit, warmed up at load.
- **Sounds**: Lyra's fire MetaSounds (`MSS_Weapons_Pistol_Fire`, `MSS_Weapons_Rifle_Fire`) and the dry click are
  offered to the audio thread, which owns combat sound playback.
- **Values**: Lyra's readable multipliers (aim 0.65; crouch 0.6 to 0.65; jump 1.25 to 1.6) are close to ours;
  Lyra's fire rate, magazine, reload and damage cannot be read from Python, ours stay those of the table above.

### Disarming and pickups (night of 13/11)

- **Who drops**: a non-player body that holds ammunition drops its gun when it dies, or when it takes a heavy
  blow or a takedown with its gun drawn (`GTPPickup::Disarms`). A disarmed officer keeps the weapon component but
  has nothing left (`HasAmmo()` false): the police thread switches it to melee or arrest. A gun already dropped
  never drops twice.
- **What lies on the ground**: `AGTPWeaponPickup`, the gun's mesh (or a small dark box when the mesh is missing)
  turning slowly, with its magazine plus half of its reserve, at least 6 rounds and at most two magazines
  (`GTPPickup::DropRounds`). Six on the ground at most (`MaxPickups`, the oldest goes), 60 s each
  (`PickupLifeSeconds`).
- **Picking up**: walking over it, or driving over it (the driver's weapon, `FindHeld`). A weapon the player
  lacks is added with those rounds as reserve (the carbine taken from an officer); otherwise the rounds go to
  its reserve up to the maximum. Full: the pickup stays.
- **Crowd and shots**: every shot heard (`UGTPCombatAudio::PlayShot` → `UGTPAudioWorld::OnLoudSound`) scares the
  crowd within `ShotScareRadius`, and the threat system makes pedestrians and traffic flee. With `-Audio=0`
  only the threat system remains: no broadcast of loud sounds.
- **Lyra sounds**: `lyra_weapons_setup.py` also lists the full paths of the fire and dry fire sounds found after
  the migration, for the audio settings.

### Upper body and retarget (13/11, after the migration)

- **First run on the PC**: the Lyra assets are there, the retarget was blocked: no UEFN IK rig at the sample's
  path and no Lyra mannequin mesh. The script now builds its own retargeter (`RTG_GTP_UE5_to_UEFN`) from the
  first IK rigs found (target: the pedestrians' `IK_SKM_UEFN_Mannequin` when the sample's is missing), maps the
  chains by name (the method `create_pedestrian_retargeter.py` proved on 5.8), never reuses another thread's
  retargeter, takes Lyra's `SKM_Manny` as source mesh (or any mesh on the animations' skeleton), and calls the
  5.8 batch retarget with keywords (`target_path`).
- **Upper body only**: the player's blueprint (`SandboxCharacter_CMC_ABP`) has only `DefaultSlot`. Clips played
  in a slot go through a blend profile made at load from the player's skeleton, in WeightFactor mode (`UpperBodyRoot`,
  `spine_01` and its children weigh 1, every other bone 0; a BlendMask profile asserts in a montage slot in 5.8), so drawing, reloading and the aim pose leave the legs walking. No change to the
  blueprint. Without the bone, or if the mask cannot be made, clips play on the full body as before (logged once).
- **Clips**: full-pose Lyra actions (`MM_*_Equip`, `_Reload`, `_DryFire`, `_Idle_ADS`) in `DefaultSlot`. Lyra's
  Fire exists only as an additive: it waits for an additive slot in the player's blueprint, the camera kick and
  the flash carry the shot until then.
- **Kellan (MetaHuman visual)**: the gun goes in the hand that shows. When the pawn's own mesh is hidden and the
  visual's body hangs from it, the gun is attached to the body's `hand_r`, placed each frame at the grip of the
  pawn's `weapon_r` moved by the gap between the two hands (right orientation whatever the body's bone axes);
  otherwise on the pawn's `weapon_r`. With `-PlayerVisualBaked`, drawing a weapon puts the body back on the runtime
  retarget (it follows the pawn, weapon clips included) and holstering returns it to the baked clips. Kellan is
  the default visual (`PlayerVisualClass` in `DefaultGame.ini`, runtime retarget with `RTG_UEFNped_to_Metahuman`;
  `bPlayerVisualBaked=True` there or `-PlayerVisualBaked` switches to the baked clips); `-PlayerVisual=` (empty)
  shows the mannequin for weapon tests.
- **Test benches**: `-CombatBench=1` fires every 0.45 s standing, shoulder aim held (aim pose, aim camera);
  `-CombatBench=2` walks a slow circle (half speed, 15 degrees a second) and fires from the hip, so the wide camera
  shows the legs walking while the reload plays on the upper body.
- **Aim pose held (fix of 13/11 night)**: the capture of candidate 27 showed no aim pose. Every clip played as a
  montage ended the aim pose: Lyra's Fire montage (same montage group) at every shot, the reload and the draw in
  `DefaultSlot`, and nothing started it again. Fire now plays without stopping the other montages, and while the
  aim lasts the aim pose comes back as soon as its slot is free again (checked at most every 0.3 s). The player's
  log says `weapon: aim pose … playing` when it starts, and every 2 s while aiming `weapon: aim probe`: slot
  active, montage weight, and where `hand_r` is from the head (height, reach forward) on the pawn and on the
  visible body: a raised hand well ahead of the head is an aim, a hand far below and level with the body is not.
