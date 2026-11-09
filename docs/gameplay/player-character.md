# Playable character: Game Animation Sample integration

*French version: [player-character.fr.md](player-character.fr.md).*
*Version 0.1 "One-click play", tasks 3 and 4 of the [status board](../../production/roadmap-status.md).*
*Last updated: 2026-11-09. Status: plan ready, editor integration pending.*

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

## 3. Licensing and Git: Epic content stays out of the public repo

The repo is public under the MIT licence. GASP assets (`.uasset`) are Epic
content under the Epic Content License: we may use them and ship them
**embedded in the packaged game**, but not redistribute them in source form
to the general public. Code, `.ini` files, the `.uproject` and our own
creations can be public.

Rule decided by the project lead on 2026-11-09: Epic and Fab content stays out of the public repo, kept locally.

| Content | Where | In Git? |
| ---- | ---- | ---- |
| GASP assets (characters, animations, pose databases, original Blueprints) | Local content plugin `Plugins/GameAnimationSample/` | **No** (`.gitignore`), reinstalled with the §6 procedure |
| Our child Blueprints, tuning, camera | `Content/Characters/Player/` | Yes (LFS) |
| Our input actions and mapping contexts | `Content/Input/` | Yes (LFS) |
| C++ code (camera, controller) | `Source/GranTheftPraline/` | Yes |

Keeping the GASP in a content plugin cleanly separates what belongs to Epic
from what belongs to us, and lets us move to a newer sample without
touching our content. The same rule will later apply to Megascans and other
Fab assets: used and packaged, never published in source form.

## 4. GTA-style camera

| Mode | Starting values | Notes |
| ---- | ---- | ---- |
| **Third person** (default) | Spring arm ~300 cm on foot, right-shoulder offset ~40 cm, ~60 cm above the pelvis, 75° FOV, camera collision with walls, auto-recenter behind the character after ~2 s without camera input (can be disabled) | Narrow Presqu'île streets: camera collision is essential |
| **First person** | Camera attached to the head, 90° FOV (adjustable 70 to 110°), head hidden for the player, body and shadow visible, same animations as third person | Use the engine's first-person rendering (separate FOV and scale for the body) if available in 5.8, to be checked in the editor |
| **Toggle** | `V` key / gamepad View button, quick blend (~0.2 s) | The chosen mode is saved in the options |

Implementation: start from the GASP camera (Gameplay Cameras) and add a
first-person mode; if that proves fragile, a small in-house C++ class
(`UGtpCameraModeComponent`) drives both modes on a regular spring arm. The
choice is made in the editor, based on measurement.

## 5. Input (Enhanced Input)

Contexts: `IMC_OnFoot_KBM` (keyboard and mouse), `IMC_OnFoot_Gamepad`,
`IMC_Global` (pause, camera). **AZERTY keyboard layout by default** (the
project lead's Mac), QWERTY as an option, full remapping planned for 0.12.

| Action | Type | AZERTY keyboard | QWERTY keyboard | Gamepad |
| ---- | ---- | ---- | ---- | ---- |
| `IA_Move` | 2D axis | Z Q S D | W A S D | Left stick |
| `IA_Look` | 2D axis | Mouse | Mouse | Right stick |
| `IA_Sprint` | Hold | Left Shift | Left Shift | A / Cross (hold) |
| `IA_Walk` | Toggle | Left Alt | Left Alt | Left stick click |
| `IA_Jump` | Press | Space | Space | X / Square |
| `IA_Crouch` | Toggle | C | C | B / Circle |
| `IA_ToggleCameraMode` | Press | V | V | View / Share button |
| `IA_CameraRecenter` | Press | Middle mouse | Middle mouse | Right stick click |
| `IA_Pause` | Press | Escape | Escape | Menu / Options |

Modifiers: 0.2 radial dead zone on sticks, adjustable mouse and gamepad
sensitivity, optional vertical axis inversion. Action names are kept for
later versions (`IA_Aim`, `IA_Fire`, `IA_EnterVehicle` arrive in 0.3 and
0.4).

## 6. Integration steps

Preparation (without the project editor):
1. Get the GASP 5.8 on the Mac: Epic launcher, Fab tab, "Game Animation
   Sample", create the project in `Samples/GameAnimationSample`, next to
   `GTAL` (outside the repo).
2. Read the exact plugins, tags and CVars from its `.uproject` and `.ini`
   files, and update this document.

In the editor (one Unreal session at a time, after the project's C++
conversion):
3. In the GASP project: create a `GameAnimationSample` content plugin, move
   the sample content into it, fix up redirectors.
4. Copy the plugin into `GranTheftPraline/Plugins/`, enable the engine
   plugins, merge tags and CVars.
5. Create `BP_PlayerCharacter` (child of the GASP character) in
   `Content/Characters/Player/`, our `IA_*` and `IMC_*` in `Content/Input/`,
   and wire it to the C++ `GameMode`.
6. Add the first-person mode and the toggle.
7. Test in the greybox: 2-minute walkthrough, video capture, frame time
   capture (indicative in 0.1), gamepad check.

## 7. Definition of done

- Walk, run, sprint, jump, crouch and vault with keyboard-mouse and gamepad
  in `L_RueRepublique`.
- Third-person camera never clips through walls; first-person toggle with
  no frame hitch and no visible head.
- Evidence: 2-minute video and screenshots of both modes, stored with the
  0.1 evidence; no Epic asset tracked by Git (`git ls-files Plugins/`
  check).

## 8. Risks

| Risk | Mitigation |
| ---- | ---- |
| Incomplete GASP migration (camera not starting, missing tags), already reported by the community | Plugins enabled before copying, tags and CVars carried over, immediate test in an empty map |
| Motion matching cost on the M1 Pro | Measured at integration; budget required only from 0.2 |
| First person: body clipping, motion sickness | Engine first-person rendering, adjustable FOV, no head bob by default |
| Epic content pushed by mistake | `.gitignore` on `Plugins/GameAnimationSample/` and a check before every push |
