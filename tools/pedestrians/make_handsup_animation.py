"""Make a hands-up animation for the pedestrians from their idle (editor script, nothing is downloaded).

  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="<repo>/tools/pedestrians/make_handsup_animation.py" -unattended -nosplash -nullrhi

Duplicates the idle loop of the UEFN mannequin to /Game/Characters/Pedestrians/Anims/A_HandsUp and, on every
frame, raises both arms (upper arm and forearm rotated up, hands open above the head). The pedestrians find
it by name ("hands" + "up") or through HandsUpAnimation in DefaultGame.ini. Lines "HANDSUP ...".
The bone names follow the UEFN mannequin (upperarm_l/r, lowerarm_l/r); GTP_HANDSUP_BONES overrides them
("upper_l,lower_l,upper_r,lower_r"). Everything stays under /Game, not versioned.
"""
import os

import unreal

SOURCE = "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Relaxed_Stand_Idle_Loop"
FOLDER = "/Game/Characters/Pedestrians/Anims"
NAME = "A_HandsUp"
bones = (os.environ.get("GTP_HANDSUP_BONES") or "upperarm_l,lowerarm_l,upperarm_r,lowerarm_r").split(",")

unreal.EditorAssetLibrary.make_directory(FOLDER)
path = FOLDER + "/" + NAME
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorAssetLibrary.delete_asset(path)
anim = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, path)
unreal.log("HANDSUP duplicate %s" % anim)
frames = unreal.AnimationLibrary.get_num_frames(anim)
unreal.log("HANDSUP frames %d, bones %s" % (frames, bones))

# Local rotations of the raised pose, one per arm bone (degrees): arms up, elbows bent.
poses = [
    (bones[0], unreal.Rotator(0.0, -150.0, 0.0)),
    (bones[1], unreal.Rotator(0.0, -35.0, 0.0)),
    (bones[2], unreal.Rotator(0.0, 150.0, 0.0)),
    (bones[3], unreal.Rotator(0.0, 35.0, 0.0)),
]
ctrl = getattr(anim, "controller", None)
unreal.log("HANDSUP controller %s" % ctrl)
done = 0
if ctrl is not None:
    ctrl.open_bracket("HandsUp")
    for bone, rot in poses:
        try:
            ctrl.set_bone_track_keys(bone, [unreal.Vector(0, 0, 0)] * frames, [rot.quaternion()] * frames,
                                     [unreal.Vector(1, 1, 1)] * frames)
            done += 1
        except Exception as err:
            unreal.log("HANDSUP bone %s failed: %s" % (bone, err))
    ctrl.close_bracket()
unreal.log("HANDSUP bones set %d of %d" % (done, len(poses)))
unreal.EditorAssetLibrary.save_asset(path)
unreal.log("HANDSUP saved %s" % path)
