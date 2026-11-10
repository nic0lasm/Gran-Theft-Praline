# Car enter and exit poses (hand-keyed, free)

*French version: [car-poses.fr.md](car-poses.fr.md)*

The paid Fab animation pack was rejected on price (2026-11-10). The enter and exit
animations are keyed by hand from data:

- `tools/animation/car_poses.json`: key poses (reach, door open, turn and lower,
  seated, close door, hands on wheel) as rotation deltas per UE mannequin bone. The
  exit replays the enter keys backwards, from seated to standing.
- `tools/animation/build_car_poses.py`: builds the actions in Blender and exports
  `AM_EnterCar_Left.fbx` and `AM_ExitCar_Left.fbx` (30 fps, 62 and 40 frames).
  Without `--input` it uses a stand-in skeleton with the mannequin's bone names:
  the cloud run prints PASS. With the mannequin FBX exported from Unreal on the Mac
  (`--input`), it keys the real bones.

The values are a first pass: they must be looked at on the real mannequin in
Blender and tuned in the JSON (no code change). Then import the FBX in Unreal and
set them as `EnterMontage` and `ExitMontage` of `AGTPVehicle`; the right-hand door
mirrors the left one.
