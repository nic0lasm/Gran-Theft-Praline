# Playable character: Game Animation Sample integration

*French version: [player-character.fr.md](player-character.fr.md).*
*Version 0.1 "One-click play", tasks 3 and 4 of the [status board](../../production/roadmap-status.md).*
*Last updated: 2026-11-09. Status: integrated and tested headless in `L_RueRepublique` (third and first person); manual gamepad check and vault test pending.*

## 1. Goal

In 0.1 the player controls a character in the greybox Rue de la République:
walk, run, jump, crouch, with a **GTA-style camera**: classic third person by
default and a **first-person toggle** (project lead decision, 2026-11-09).
Bodycam remains a visual quality reference only: no body-camera effects.

Nothing is reinvented: locomotion comes from Epic's **Game Animation
Sample** (GASP, free on Fab, updated for 5.8), wired to our own input and
camera.

## 2. What the GASP provides

- A playable **motion matching** character (Pose Search) with 500+
  animations: walk, run, sprint, jumps, crouch, stops and pivots, traversal
  (vault, climb, mantle).
- `CharacterMovementComponent` (stable path; Mover is to be tested later,
  see the [asset survey](../sourcing/asset-sourcing.md)).
- UE5 and UEFN mannequins, Enhanced Input, a third-person camera.

Plugins to enable **before** any migration (to be checked against the GASP
5.8 `.uproject`, which is authoritative): Pose Search, Motion Trajectory,
Motion Warping, Animation Warping, Chooser, Animation Locomotion Library,
Deformer Graph, Gameplay Cameras, Mover, Live Link Control Rig.
Also carry over from the GASP: gameplay tags
(`Config/DefaultGameplayTags.ini`), collision channels and the project
CVars it declares.

### GASP 5.8 survey on the Mac (2026-11-09)

Project downloaded to `Samples/GameAnimationSample` (7.0 GB, 3,890
`.uasset`, 5 levels), Blueprint only.

- **`.uproject` plugins**: AnimationWarping, RigLogic, LiveLink,
  LiveLinkControlRig, PoseSearch, AnimationLocomotionLibrary, MotionWarping,
  HairStrands, Chooser, Mover, NetworkPrediction, ChaosMover,
  AnimationLayering, MoverExamples, MovieSceneAnimMixer, DrawDebugLibrary,
  SmartObjects, Locomotor, CurveExpression, GameplayInteractions. All are
  enabled, even though our character uses the `CharacterMovementComponent`:
  the sample Blueprints reference them. GameplayCameras is not listed but
  the camera uses it (enabled as a dependency, to be checked).
- **Collision channels to recreate before copying**, with the same
  indices: `GameTraceChannel1` Traversable (trace), `GameTraceChannel2`
  Mouse (trace), `GameTraceChannel3` Obstacle (object), plus the
  `TraversalObjectPreset`, `CharacterCapsule` and `ObstaclePreset` profiles.
  Without them traversal detects nothing. Our project has no custom channel
  yet.
- **Gameplay tags**: copy `DefaultGameplayTags.ini` as is (`MotionMatching.*`,
  `Foley.*`, `SmartObject.*` tags, redirects).
- **Data-driven CVars** (`DataDrivenConsoleVariableSettings`): carry them
  all over. We pick the `SandboxCharacter_CMC` character (stable path);
  `DDCVar.NewGameplayCameraSystem.Enable=True` keeps the new camera.
- **Physics**: substepping enabled (16.7 ms max step, 16 substeps), carry
  over.
- **Input**: `Content/Input/IMC_Sandbox` and 18 sample `IA_*` actions. The
  sample keeps its `/Game/Input` path; our own actions and contexts live in
  `/Game/Input/GTP`.
- **Camera**: Gameplay Camera System (`CameraAsset_SandboxCharacter`,
  `CameraDirector_SandboxCharacter`, Close, Medium and Far rigs), kept as is
  for third person.
- **Content actually needed** (asset registry closure, hard and soft
  references, from `SandboxCharacter_CMC`, its anim Blueprint, `GM_Sandbox`,
  `PC_Sandbox`, `/Game/Input` and the camera assets): 2,039 packages,
  2.0 GB. `Characters/UEFN_Mannequin` (1.97 GB), `Levels/LevelPrototyping`
  (38 MB, hard reference from `AC_TraversalLogic` to `LevelBlock_Traversable`),
  `Audio` (12 MB), `Blueprints` (31 MB), `Input`, `Misc`. The raw closure
  is 4.7 GB because `GM_Sandbox` soft-references the sample's alternative
  characters (Echo, Twinblast, UE4 mannequin, MetaHuman Kellan); those soft
  references are not followed, our game mode does not use them.
  **Not copied**: `Characters/Paragon`, `Characters/Echo`,
  `Characters/UE4_Mannequin`, `Characters/UE5_Mannequins`, `MetaHumans`,
  `IsolatedExamples`, `Movies`, the sample levels, `RetargetedCharacters`.

## 3. Licensing and Git: Epic content stays out of the public repo

The repo is public under the MIT licence. GASP assets (`.uasset`) are Epic
content under the Epic Content License: we may use them and ship them
**embedded in the packaged game**, but not redistribute them in source form
to the general public. Code, `.ini` files, the `.uproject` and our own
creations can be public.

Rule decided by the project lead on 2026-11-09: Epic and Fab content stays out of the public repo, kept locally.

| Content | Where | In Git? |
| ---- | ---- | ---- |
| GASP assets (characters, animations, pose databases, original Blueprints) | Same `/Game` paths as in the sample: `Content/Audio`, `Content/Blueprints`, `Content/Characters/UEFN_Mannequin`, `Content/Input` (except `GTP`), `Content/Levels`, `Content/Misc` | **No** (`.gitignore`), reinstalled with `tools/character/install_gasp_content.sh` |
| Our child Blueprint | `Content/Characters/Player/` | Yes (LFS); can be rebuilt by `tools/character/create_player_assets.py` |
| Our input actions and mapping contexts | `Content/Input/GTP/` | Yes (LFS), same script |
| C++ code (camera mode, controller), config, scripts | `Source/GranTheftPraline/`, `Config/`, `tools/character/` | Yes |

The sample keeps its own `/Game` paths (decision of 2026-11-09, instead of
a content plugin): no redirector to fix, the sample project stays
untouched, and a newer sample is installed by running the script again.

## 4. GTA-style camera

| Mode | Starting values | Notes |
| ---- | ---- | ---- |
| **Third person** (default) | Spring arm ~300 cm on foot, right-shoulder offset ~40 cm, ~60 cm above the pelvis, 75° FOV, camera collision with walls, auto-recenter behind the character after ~2 s without camera input (can be disabled) | Narrow Presqu'île streets: camera collision is essential |
| **First person** | Camera attached to the head, 90° FOV (adjustable 70 to 110°), head hidden for the player, body and shadow visible, same animations as third person | Use the engine's first-person rendering (separate FOV and scale for the body) if available in 5.8, to be checked in the editor |
| **Toggle** | `V` key / gamepad View button, quick blend (~0.2 s) | The chosen mode is saved in the options |

Implementation (chosen after measurement, 2026-11-09):
`UGTPCameraModeComponent`, a C++ component on our player controller (so the
mode survives a respawn).
- Third person: since 0.2 our follow camera, `UGTPFollowCameraComponent`
  (lag, shoulder, collision, sprint FOV, recentering; see
  [on-foot-feel.md](on-foot-feel.md)). With `bEnabled=False` it falls back
  to the pawn's own GASP gameplay camera (80° FOV in the sample setup).
- First person: the view target blends (0.2 s, cubic) to a camera actor
  placed at the `head` bone plus a small offset (12 cm forward, 6 cm up)
  and turned with the control rotation, 90° FOV. The `head` bone is hidden
  on the mesh, so the body (arms, legs) stays visible. The character's
  `WantsToStrafe` input state is set while in first person, so the body
  faces the camera.
- Why not a rig in the GASP camera director: the director and its rigs are
  Epic assets (not in Git) and would have to be edited in the editor; the
  view target switch works with the Gameplay Camera System untouched and is
  fully in our code.
- Known limit: hiding the bone also removes the head from the shadow. The
  engine first-person rendering (separate FOV and scale for the body)
  remains to be evaluated.

## 5. Input (Enhanced Input)

Contexts in `/Game/Input/GTP`: `IMC_OnFoot_KBM_AZERTY` (default),
`IMC_OnFoot_KBM_QWERTY` (`-Qwerty` on the command line or
`bQwertyKeyboard=True` in `DefaultGame.ini`) and `IMC_OnFoot_Gamepad`.
`AGTPPlayerController` removes the sample's `IMC_Sandbox` after each
possession and adds ours. Escape and the gamepad Menu button are reserved
for the pause menu, handled by game code. Full remapping planned for 0.12.

Letters: Unreal reads the character typed, so on an AZERTY keyboard the
key labelled Z arrives as `Z`; ZQSD is therefore the AZERTY layout of WASD.

| Action | Type | AZERTY keyboard | QWERTY keyboard | Gamepad |
| ---- | ---- | ---- | ---- | ---- |
| `IA_Move` (sample) | 2D axis | Z Q S D | W A S D | Left stick |
| `IA_Look` / `IA_Look_Gamepad` (sample) | 2D axis | Mouse | Mouse | Right stick |
| `IA_Sprint` (sample) | Hold | Left Shift | Left Shift | A / Cross (hold) |
| `IA_Walk` (sample) | Toggle | Left Alt | Left Alt | Left stick click |
| `IA_Jump` and `IA_Traverse` (sample) | Press | Space | Space | X / Square |
| `IA_Crouch` (sample) | Toggle | C | C | B / Circle |
| `IA_ToggleCameraMode` (ours) | Press | V | V | View / Share button |
| `IA_CameraRecenter` (ours, built in code) | Press | Middle mouse | Middle mouse | none (right stick click is taken) |
| `IA_Interact` (ours, built in code) | Press | E | E | D-pad right |

Modifiers copied from `IMC_Sandbox`: swizzle and negate on the movement
keys, mouse Y inverted by a scalar, 0.3 radial dead zone on the left
stick, 0.2 dead zone, exponential curve and 90/70 scaling on the right
stick. Sensitivity settings and axis inversion options come with the
options menu. Action names are kept for later versions (`IA_Aim`,
`IA_Fire`, `IA_EnterVehicle` arrive in 0.3 and 0.4).

## 6. Integration steps

1. Get the GASP 5.8 on the Mac: Epic launcher, Fab tab, "Game Animation
   Sample", create the project in `Samples/GameAnimationSample`, next to
   the repository (outside it).
2. `tools/character/install_gasp_content.sh`: lists the needed packages
   with a headless, read-only run in the sample
   (`gasp_dependency_closure.py`) and copies them into
   `GranTheftPraline/Content` with the same paths.
3. Config (in Git): sample plugins plus GameplayCameras in the
   `.uproject`; collision channels and profiles, data-driven CVars, physics
   substepping and Pose Search settings in `DefaultEngine.ini`;
   `DefaultGameplayTags.ini`; `PlayerPawnClass`, our controller section and
   `DirectoriesToAlwaysCook` in `DefaultGame.ini`. The sample's renderer
   settings are not copied (ours follow the ADRs).
4. Build the editor, then `tools/character/create_player_assets.py`
   (headless): `BP_PlayerCharacter`, `IA_ToggleCameraMode` and the three
   mapping contexts.
5. `tools/character/playtest_character.sh`: plays `L_RueRepublique` in
   `-game` mode and checks run, walk, sprint, jump, crouch, the first
   person toggle and a respawn in first person, with screenshots and frame
   times (§9).

## 7. Definition of done

- Walk, run, sprint, jump, crouch and vault with keyboard-mouse and gamepad
  in `L_RueRepublique`.
- Third-person camera never clips through walls; first-person toggle with
  no frame hitch and no visible head.
- Evidence: 2-minute video and screenshots of both modes, stored with the
  0.1 evidence; no Epic asset tracked by Git (`git ls-files GranTheftPraline/Content` check).

## 8. Risks

| Risk | Mitigation |
| ---- | ---- |
| Incomplete GASP migration (camera not starting, missing tags), already reported by the community | Plugins enabled before copying, tags and CVars carried over, immediate test in an empty map |
| Motion matching cost on the M1 Pro | Measured at integration; budget required only from 0.2 |
| First person: body clipping, motion sickness | Engine first-person rendering, adjustable FOV, no head bob by default |
| Epic content pushed by mistake | `.gitignore` on every copied sample folder and a check before every push |

## 9. Integration results (2026-11-09, M1 Pro, editor build in `-game` mode)

`tools/character/playtest_character.sh`, about 50 s of scripted play in
`L_RueRepublique` at 1920x1080, all checks pass:

| Check | Measured |
| ---- | ---- |
| Spawn | `BP_PlayerCharacter` at the PlayerStart (1212, -15665) |
| Run (default) | 500 cm/s |
| Walk (toggle) | 200 cm/s |
| Sprint (hold) | 700 cm/s |
| Jump | airborne, 127 cm rise |
| Crouch (toggle) | crouched, 225 cm/s |
| First person | camera actor view target, 90° FOV, head hidden, body visible looking down |
| Respawn in first person | new pawn, still first person, head hidden |
| Back to third person | pawn view (GASP camera), head shown |

Frame times over the run (warm-up excluded): frame p50 45 ms, p95 56 ms;
game thread p50 14 to 19 ms; GPU p50 45 ms, p95 55 ms. The run is GPU bound,
in line with the reference scene measurement (GPU p95 43 to 51 ms before
the rendering profiles); the game thread cost of motion matching is to be
budgeted in 0.2. No error from our assets in the log. Vaulting
(`IA_Traverse`) and a real gamepad remain to be checked by hand.

Cook check: `-run=cook -targetplatform=Mac -map=/Game/Maps/L_RueRepublique`
succeeds in 58 s, 2,899 packages, 0 errors, 0 warnings; our player
Blueprint, input actions and contexts are in the cooked asset registry.
