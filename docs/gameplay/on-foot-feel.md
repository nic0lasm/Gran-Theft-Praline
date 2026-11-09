# On-foot feel: camera, prompts, HUD, ambience

*French version: [on-foot-feel.fr.md](on-foot-feel.fr.md).*
*Version 0.2. Last updated: 2026-11-09. Status: code ready, compile and playtest on the Mac pending.*

## 1. Goal

On foot, the game should read like the open-world references of the genre
(third person action games set in a modern or western open world): a camera
that follows with weight but never fights the player, a simple contextual
prompt when something can be used, an almost empty screen, and a street that
sounds alive. Locomotion itself (walk, run, sprint, starts, stops, pivots,
jumps, crouch, vaults) stays with the Game Animation Sample motion matching
([player-character.md](player-character.md)), which already gives credible
transitions and stops; this work wraps it.

Everything below is our own C++ code and config (MIT), no editor asset to
author; every value is in `Config/DefaultGame.ini` and can be tuned without
rebuilding.

## 2. Third person camera (`UGTPFollowCameraComponent`)

Owned by the player controller; it drives a camera actor that becomes the
view target. The sample's own camera assets are left untouched:
`bEnabled=False` gives the view back to them.

| Behaviour | Value (default) | Why |
| ---- | ---- | ---- |
| Pivot | 62 cm above the capsule centre (shoulders) | Frames the whole body and the street ahead |
| Distance | 290 cm walking, +45 cm at full sprint, -45 cm crouched | Speed reads on screen; crouching brings the camera in |
| Shoulder | 38 cm right, 8 cm up | Character slightly left of centre, the street stays visible |
| Look up | Arm down to 55 % at +60° pitch | The camera does not dig into the ground |
| Position lag | 11/s horizontal, 6/s vertical, 3.5/s airborne, max 55 / 70 cm | Weight on starts, stops, jumps and steps without floating |
| Rotation lag | none | Mouse and stick feel direct |
| Collision | 14 cm sphere sweep from the shoulders on the Camera channel; pulls in at once, lets out at 4/s | Never behind a wall in the narrow Presqu'île streets, no popping |
| Too close | Under 45 cm the body is hidden for the player only (shadow kept) | Pressed against a wall the view does not show the inside of the head |
| Field of view | 78° walking, 86° at full sprint (blend 480 → 650 cm/s), 3/s | Speed sensation |
| Auto recenter | After 2.5 s without look input, while moving above 120 cm/s and less than 100° away: drifts behind the direction of travel, at most 35°/s | Running sideways curves gently; walking towards the camera keeps the face in view |
| Manual recenter | Middle mouse: behind the character at -10° pitch in 0.25 s | Quick reorientation |

First person (V) is unchanged (head camera, 90°); switching blends between
the two camera actors in 0.2 s. Leaving a car blends from the chase camera
to the follow camera in 0.5 s.

## 3. Contextual interaction (E)

- `UGTPInteractableComponent`: add it to any actor (door, counter,
  machine), place it where the hand goes, set `Prompt` (the verb, for
  example "Entrer") and react to `OnInteracted`. `Message` shows a subtitle
  on use, as placeholder feedback until the real action exists (shop
  interiors).
- `UGTPInteractionComponent` on the controller: ten times a second, picks the
  interactable within its radius (180 cm), in front of the view (cosine >
  0.35) and with nothing solid in between, closest and most centred first.
- Keys: E (D-pad right on the gamepad), in the always-active interaction
  context built in code (`UGTPVehicleInput`). Vehicles keep F (Y).
- Prompts: "E  <verb>" for the focused interactable and "F  Monter" when a
  car can be entered.

## 4. HUD (`AGTPHUD`)

Drawn on the canvas, no widget asset, hidden while a menu is open:
- a 2.5 px centre dot with a dark ring, 45 % opaque in third person, 80 % in
  first person;
- prompts at the bottom right, one line each: the key in a light box, then
  the verb, on a translucent dark panel, fading and sliding in (0.12 s);
- a subtitle line at the bottom centre for short messages.
Sizes are given for 1080 lines and scaled with the window.

## 5. Street ambience

`tools/audio/make_street_ambience.py` synthesises a 120 s seamless stereo
loop (48 kHz): distant traffic rumble that breathes slowly, cars, scooters
and the odd bus passing with Doppler shift and stereo travel, a faint
pedestrian murmur and a few sparrows. No recording is used, so no licence
question and no cost. `tools/audio/import_ambience.py` imports it as
`/Game/Sound/Ambience/SW_Amb_Street_Day` (looping, regenerated, not
versioned); the controller plays it in 2D at volume 0.5 with a 3 s fade-in.
Footsteps, jumps and landings come from the sample's foley.

Next steps: positional sources (cafés, a tram on the quays, fountains),
night variant, recorded CC0 layers if the synthetic bed sounds thin.

## 6. Checks (playtest)

`tools/character/playtest_character.sh` now also checks:

| Check | Pass |
| ---- | ---- |
| Framing at rest | FOV 74-82°, camera at least 2 m away |
| Framing sprinting | FOV 82-90°, camera at least 2.4 m away |
| Stop after a sprint | Stops within 0.2-3 m and 1.2 s |
| Camera never behind a wall | Line from the shoulders to the camera clear on 99 % of frames or more |
| Auto recenter | Running diagonally for 6 s turns the view 15-200° |
| Third person view | Head visible, camera more than 1.2 m away |

`GTP_CAPTURE=<seconds>` films the run (frames by `UGTPFrameCapture`); frame
times come from a separate, unfilmed run with the m1 profile.

## 7. Results

To be filled after the Mac run: playtest report, video, frame times before
and after (m1 profile).
