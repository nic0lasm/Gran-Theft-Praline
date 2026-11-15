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

## 10. Nicolito, the player's face (decision of 2026-11-13)

The player is a fictional white man named Nicolito (not the likeness of a real person). The project lead set
aside the Game Animation Sample's MetaHuman Kellan and chose, among three free routes, a face from the City Sample
crowd (MetaHuman-derived characters, already on the PC with the complete City Sample). Kellan stands in until
then; the swap is `PlayerVisualClass` and its cook folder in `DefaultGame.ini`, since the body follows the pawn
by the run-time retarget and the weapon clips play on the pawn.

Steps, each through the PC's queue: `tools/character/citysample_crowd_survey.py` (plain Python, read only) lists
the crowd folders and their weight, the heads, bodies, outfits and hair by name, and the packages that need the
sample's own C++ classes; then three male faces in captures for the project lead to choose from; then the chosen
head, body, outfit and hair assembled into Nicolito's blueprint in our project.

**Portraits (2026-11-14).** `-NicolitoFaces` (`UGTPFaceLineupSubsystem`, nothing without the switch) films every
normal-weight male head of the pack found under `/Game/Crowd/Character/Male` (asset registry, `*_nrw_FaceMesh`, at
most `MaxHeads`): 10 s after the start, each head in turn on the crowd's normal-weight male body (crewneck, jeans,
loafers), 4 m in front of the player and turned towards him, face at its finest level, framed head and shoulders
by a camera of its own (`CameraDistanceCm` 100, `CameraFOV` 30), then saved as `Nicolito_<head>.png` in the
screenshot folder (`Saved/Screenshots/...`); the player's view comes back at the end. Which way the face looks is
measured in the body's reference pose (from the left clavicle to the right one), not assumed, and logged. These
are the bare heads of the pack, without hair (the crowd assembly has none) and in the reference pose; the hair
comes with the blueprint, once the face is chosen. Run in the editor in game mode, the pack's assets being
uncooked: `UnrealEditor GranTheftPraline.uproject -game -NicolitoFaces -SkipTitle -DayNight=0 -Pedestrians=0
-Police=0`; the log says `face lineup: ...` for each head and each image.

First run on the PC (2026-11-14, 6 heads, m_001 to m_006: City Sample has no m_007 to m_012): no error, but the
images were the game's view, Kellan from behind and the head a few pixels wide 5 m away. The game's follow camera
takes the view back every frame when it is not its own (`UGTPFollowCameraComponent`). The bench now pauses it,
hides the player (pawn and the visual attached to it) and the HUD while the portraits last, puts everything back
at the end, and logs for each image the view target at the shot (`ours` or `NOT OURS`) with the camera distance
and FOV. Measured on that run: the pack's normal-weight male body faces +Y in its mesh, like the mannequin.

**Assembly (ready, waits for the choice).** `tools/character/create_nicolito_visual.py` (editor, game module built;
`GTP_NICOLITO_HEAD=m_00N`) copies BP_Kellan, never touched, to `/Game/MetaHumans/GTP/Nicolito/BP_Nicolito` and puts
the chosen head on its `Face` component, without Kellan's material overrides; Kellan's body, outfit, retarget and
grooms handling stay, his grooms are emptied (bound to his face; the game mode hides a groom without an asset),
then those named in `GTP_NICOLITO_GROOMS` (default `Eyebrows`) are put back with a new binding to the chosen face
(`/Game/MetaHumans/GTP/Nicolito/Grooms`, HairStrands `GroomBlueprintLibrary`, transferred from the face the groom was
made on: MetaHuman faces share one topology).
`GTP_NICOLITO_MODE=full` also puts the crowd's normal-weight male body and outfit (`BP_Nicolito_Full`), saved only
if that body has the skeleton of the body's retarget blueprint. The face keeps Kellan's face animation only if the
new head has the skeleton it was made for; otherwise the game mode makes the face follow the body's head and neck
(log: `Face has no animation of its own`). The C++ side is `UGTPAnimGraphTools::DescribeBlueprintComponents` and
`SetBlueprintComponentAsset` (construction script templates, set as the details panel does). Report in
`Saved/GTPCombat/nicolito-visual.txt`; trial with `-PlayerVisual=/Game/MetaHumans/GTP/Nicolito/BP_Nicolito.BP_Nicolito_C`,
then `PlayerVisualClass` once a capture shows it right. Nicolito is bald until his hair is chosen, and the neck
shows whether Kellan's body skin matches the new face (otherwise `full`). The capture is `-NicolitoPortrait` (same
bench, on the player as he is in the game, left visible): `Nicolito_player_face.png` (chin to skull, 120 cm, FOV 30)
and `Nicolito_player_body.png` (feet to skull from the front, `PlayerBodyDistanceCm` 380, `PlayerBodyFOV` 50).

The project lead chose the head **m_004** (2026-11-14, on the six portraits).

First run of the script on the PC (2026-11-14, m_004): saved. The crowd head has its own `Face_Archetype_Skeleton`
(`/Game/Crowd/Character/Shared/Rig`), another asset than the MetaHumans' one: the face follows the body, without
facial animation. Every groom was emptied, eyebrows included, hence putting them back with `GTP_NICOLITO_GROOMS`.
Its capture showed: hands much darker than the face (Kellan's body skin under m_004's head), the front of Kellan's
hoodie hanging to the knees like a cape, the chin cut on the face shot and the head cut on the full-length one
(framing corrected above). Next run in `full` mode: the crowd's body and outfit made for these heads; the body's
retarget blueprint (`ABP_GenericRetarget`) is a template made for no skeleton, which the check now accepts. The report
lists the face's and the body's skin material parameters, to match them if the tones still differ.

`full` run (2026-11-14): saved, framing right on both shots, no seam at the neck, hands and face the same tone, but
that tone is dark brown, a dark panel hangs from the waist between the legs, and the eyebrows were not bound
(`GroomBlueprintLibrary.create_new_groom_binding_asset_with_path` is not exposed to Python). The crowd's synthesized
skin takes its colour from atlases (`FaceColorAtlas`, `BodyColorAtlas`) at `SourceU`/`SourceV` with an
`AtlasSelector`, left at the instances' defaults in our copy (City Sample sets them per character at run time).
`tools/character/nicolito_skin_probe.py` (read only) lists those parameters for the six heads and the body, exports
both atlases as PNG and lists the groom calls the editor's Python has.

Probe results (2026-11-14): each atlas is 8192 px, a 4x4 grid of 2048 px tiles, 12 used, the same tones tile for tile
in the face and body atlases; `SourceU`/`SourceV` stay at 0.5 (the middle of the tile). Each male head's own material
reads its own tile (`AtlasSelector` m_001 = 0 to m_006 = 5, m_004 = 3); the body's shared material reads tile 0.
City Sample gives the head and the body the same tile (`CrowdBlueprintLibrary.cpp`: `AtlasSelector` =
`SkinTextureDefinition.TextureAtlasIndex` on both). So the full run put a tile-3 face on a tile-0 body. The binding
call exists under its script name, `unreal.GroomLibrary`.
`create_nicolito_visual.py` now gives every face and body slot that reads another tile a material instance of its own
material reading the head's tile (or `GTP_NICOLITO_SKIN`), under `/Game/MetaHumans/GTP/Nicolito/Materials`, without
touching the crowd's materials; it binds the eyebrows through `GroomLibrary`; in `full` mode it lists, for Kellan's
body and each garment, the bones the crowd body's skeleton lacks (a garment follows the body by bone name; a body
without the bones its retarget was set up on stays in its reference pose, the A pose seen), their cloth and
post-process blueprint, and the crowd garments that fit this body (`GTP_NICOLITO_OUTFIT` swaps one).
The tile order in the atlas (row by row from the top left, or otherwise) is not proven: the first run keeps the
head's own tile, the tone of the portraits the project lead chose from.

Second `full` run (2026-11-14, fixed light with `-DayNight=0`): face and hands now the same tone, but tile 3 is a
medium tanned brown, not the white man asked for; the eyebrows' binding was created (transferred from Kellan's face)
yet they do not show; the panel and the A pose remain; the diagnosis stopped on the cloth list. Next run: light skin
(`GTP_NICOLITO_SKIN=1`, which also tells the tile order: light if row by row), the eyebrows transferred from the
source mesh of Kellan's binding (the head the groom was made on) or projected directly, under a new name, with both
faces' LOD 0 sections in the report; the diagnosis prints the bones first and also lists the bones the player's
retarget writes (`RTG_UEFNped_to_Metahuman`, its target IK rig's root and chains) that the crowd body lacks.

The same panel and the same slight A pose were on the face-mode images (Kellan's own body and outfit, the panel under
his hoodie, which hangs like a chasuble): they come neither from the crowd body nor from its garments, and Kellan,
the player's current visual, likely has them in the game too (the follow camera looks at his back). The game log
says the retarget is set (`RTG_UEFNped_to_Metahuman`, 5 settings) and the body ticks every frame. To see in numbers
what the images show, `-NicolitoPortrait` now logs at each shot (`rig: ...`): the left upper arm's angle from
straight down on the pawn's hidden mesh and on the body (the same angle if the body follows the pawn's pose), the
body's LOD, the body's bones left at the component's origin while their reference place is not (never written by
its pose), and for each mesh following the body's pose the bones it lacks and those it reads at the origin (a
vertex weighted to such a bone is pulled towards the feet: a point hanging between the legs).

Third `full` run (2026-11-14, tile 1, `-DayNight=0`): face and hands light and the same tone. The `rig:` lines: the
left upper arm is 17 to 18 degrees from straight down on the pawn's mesh and on the body alike, so the body follows
the pawn's pose (the "A pose" is the sample's idle); one bone of the body, the pelvis, sits at the component's origin
while its reference place is not, and every part following the body reads it there. The pelvis is the retarget's
root: the other bones are placed by the retarget's chains, the pelvis by its pelvis and root ops. Every vertex
weighted to the pelvis is pulled towards the feet: the shirt or hoodie hem hanging like a tunic or a cape, the point
between the legs, baggy trousers; Kellan, the player's current visual, has it in the game too. The eyebrows bound
directly landed across the upper lip: no groom is bound by default any more. `tools/character/nicolito_rig_probe.py`
(read only) gives the retarget's settings (the retargeter and both IK rigs: retarget root, chains, ops, each exported
as text) and lists the eyebrow, groom, binding and crowd hair assets, for the fix and for the hair choice.

Probe results (2026-11-14): both IK rigs have the pelvis as retarget root (the target has a `root` chain more, and
`LeftToe`/`RightToe` for the source's `LeftFoot`/`RightFoot`); the ops cannot be listed from Python on 5.8 (their
settings are in the retargeter's text export). The project has only Kellan's four grooms, but City Sample's crowd
ships baked eyebrow textures (`Eyebrows_M_Full`, `Natural`, `Thick`, `SlightArch`, `Messy`, `Scraggly`, under
`/Game/Crowd/Character/Shared/Textures/Hair`) and two baked short hair ones (`Hair_S_AfroFade`, `Hair_S_Coil`), which
its head material paints on the face (`EyebrowsAttributeMap`, `HairAttributeMap`, `ShowBakedGroomTextures` on).
`create_nicolito_visual.py` now sets them on Nicolito's head instances, the same ones that set the skin tile:
`GTP_NICOLITO_BROWS` (default `Natural`) and `GTP_NICOLITO_HAIR` (default none, bald), only where the parameter's
default is of the same kind (virtual texture or not: the other would not compile). No groom, no binding.

The pelvis, found (2026-11-14, the retargeter's text export): in `RTG_UEFNped_to_Metahuman` the Root Motion op has
`TargetRoot = pelvis` (with `SourceRoot = root`, `CopyFromSourceRoot`): it puts the target's pelvis where the source's
root is, on the ground. And the op stack is there three times (Pelvis Motion, FK Chains, Run IK Rig, Root Motion, then
the same suffixed `_0` and `_1`): `create_pedestrian_retargeter.py` added the default ops at each run, so the body
was retargeted three times per frame. `tools/character/fix_player_retargeter.py` fixes a copy,
`RTG_UEFNped_to_Metahuman_Fixed` (the game's retargeter is untouched until proven): the copies of the ops are
removed and Root Motion is disabled (the visual hangs from the pawn's mesh, which the movement component moves: the
body needs no root motion of its own). Tried with `-PlayerVisualRetargeter=`, then `PlayerVisualRetargeter` in
`DefaultGame.ini`. `create_pedestrian_retargeter.py` no longer adds the default ops to a retargeter that has some,
and disables Root Motion.

Proven on the PC (2026-11-14, portrait with `-PlayerVisualRetargeter=` the fixed copy): the script removed ops 5 to
12 (all copies) and disabled Root Motion; the `rig:` lines give "0 at the origin" for the body at LOD 0 and 1, no
part reads the pelvis at the origin any more, and the panel between the legs is gone (crewneck stopping at the
waist, jeans, loafers). The baked eyebrows show on the brow, thin and light brown, painted, readable from the front.
`PlayerVisualRetargeter` in `DefaultGame.ini` is now the fixed copy (Kellan, the player's current visual, is fixed
too, and the body is retargeted once per frame instead of three times); a copy of the project without the fixed asset
runs `fix_player_retargeter.py` once.

**Hair (2026-11-14).** The project has none of the crowd's hair grooms, but the full City Sample has them, each
with a binding already made for every crowd face: `Crowd/Character/Male/GroomBindings/GB_<head>_nrw_FaceMesh_<groom>`
(78 for the six male faces and 13 grooms). m_004's own in City Sample is `Hair_S_BuzzCut` (strands); the others
nearest a young man's: `Hair_M_SideSweptFringe` (m_003), `Hair_S_Messy` (m_002), `Hair_S_PulledBack` (m_004,
cards). `GTP_NICOLITO_HAIRSTYLE=<style>` sets the groom and its binding for the chosen head on the blueprint's
`Hair` component (Kellan's, emptied), and by default its baked texture under the strands when the pack has one
(`GTP_NICOLITO_HAIR=auto`); `GTP_NICOLITO_SUFFIX` keeps variants side by side for a choice
(`BP_Nicolito_Full_BuzzCut`). A groom or binding missing stops the save. What to copy, with the editor closed,
at the same `/Game` paths (references kept): `tools/character/crowd_hair_deps.py` writes the grooms, bindings and
baked textures missing, and every package they depend on that the project lacks, to
`Saved/CitySampleCopy/missing.txt`; `tools/vehicles/copy_citysample_content.py --list` copies it (patterns now
accepted, `--dry-run` first); run again until 0 missing. Only the chosen head's bindings are copied: `/Game/Crowd`
is cooked whole.

**Grooms (2026-11-14).** Kellan has six groom components (hair, eyebrows, eyelashes, peach fuzz, moustache, beard),
each ticking in the package against a 13 ms budget. Strand simulation is off (`r.HairStrands.Simulation=0`). The
game mode also hides, without tick, the grooms whose name contains a word of `PlayerGroomsHidden` (`Fuzz`,
`Eyelashes`: not visible from the third-person camera) and holds the others at `PlayerGroomForcedLOD` (1) through
their `SetForcedLOD`, called by reflection (the HairStrands plugin is not a dependency). The log gives each
groom's forced LOD before and after, and once its asset's LODs (strands, cards or meshes), so that a LOD sync of the
blueprint that would set it back shows. `-PlayerGrooms=full` keeps them as the blueprint made them. The same
applies to Nicolito's visual if it has grooms.

Measured on the PC (development package): groom ticks went from 6 to 4. `SetForcedLOD` cannot be reached by
reflection (not a UFUNCTION on 5.8), so the forced LOD is not applied; it would lower the GPU cost only, the
game-thread cost being the ticks. The blueprint sets the visible flag of the two hidden grooms back; the log now
gives the real state (visible flag, hidden in game, tick) and a groom without an asset (likely Kellan's beard, whose asset LODs did not show) is hidden
without tick too. Once per groom the log gives the asset's LOD count and its first cards LOD (the export leaves the
default, strands, out: Hair's LOD 1 is decimated strands, not cards).

## 11. Foot Placement assertion (2026-11-14)

Candidate 35, `-GTARun` in the editor: 6/6, then 20 s later, the player aiming at a standstill with three officers
2 m away, `Assertion failed: !PelvisTransformCS.ContainsNaN()` in `AnimNode_FootPlacement.cpp` (Animation Warping
plugin), in the Game Animation Sample's `SandboxCharacter_CMC_ABP` on an `SKM_UEFN_Mannequin` mesh. The crash context
does not name the actor: the player and every traffic and police driver (`DriverClass` empty, so the player's class)
share that mesh and blueprint. Read in the engine's source (5.8): the node divides a foot's movement by the delta
time accumulated since its last evaluation, without a guard (line 1545); an evaluation with no update since the
previous one gives a NaN. Two windows where that can happen are closed, the cause itself not being proven:

- the player's hidden mannequin, posed every frame for the visual, runs without update rate optimisations (they
  can evaluate a hidden mesh on a frame without an update); the log says whether they were on;
- a driver hidden in his seat is not posed at all (`bNoSkeletonUpdate`, tag `GTPSeatDormant`) until
  `AGTPVehicle::ReleaseCharacter`, instead of being left with its montages ticking.

The proof is the gate: the editor `-GTARun` and the packaged one running past `GTARun done` without the assertion.
