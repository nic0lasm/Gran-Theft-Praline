"""Builds hand-keyed car enter and exit animations from tools/animation/car_poses.json.

Run with Blender's Python (a venv with `pip install bpy`, or `blender -b -P`):

  python3 tools/animation/build_car_poses.py --out OUT_DIR [--input mannequin.fbx]

Without --input it builds a small stand-in armature with the UE mannequin bone
names, enough to check the pipeline (the cloud has no mannequin: it is Epic
content kept out of Git). With the mannequin FBX exported from Unreal on the Mac
it keys the real bones; the result is exported as FBX with one action per
animation, ready to import into Unreal and retarget. Prints "PASS" when the
actions exist and every keyed bone moves.
"""

import argparse
import json
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SPEC = os.path.join(HERE, "car_poses.json")

# Stand-in skeleton: name, parent, head (x forward, y right... Blender: x right, y forward, z up).
STAND_IN = [
    ("root", None, (0, 0, 0), (0, 0, 10)),
    ("pelvis", "root", (0, 0, 95), (0, 0, 105)),
    ("spine_01", "pelvis", (0, 0, 105), (0, 0, 120)),
    ("spine_02", "spine_01", (0, 0, 120), (0, 0, 135)),
    ("head", "spine_02", (0, 0, 150), (0, 0, 175)),
    ("clavicle_l", "spine_02", (0, 0, 140), (-15, 0, 140)),
    ("upperarm_l", "clavicle_l", (-15, 0, 140), (-45, 0, 140)),
    ("lowerarm_l", "upperarm_l", (-45, 0, 140), (-70, 0, 140)),
    ("hand_l", "lowerarm_l", (-70, 0, 140), (-80, 0, 140)),
    ("clavicle_r", "spine_02", (0, 0, 140), (15, 0, 140)),
    ("upperarm_r", "clavicle_r", (15, 0, 140), (45, 0, 140)),
    ("lowerarm_r", "upperarm_r", (45, 0, 140), (70, 0, 140)),
    ("hand_r", "lowerarm_r", (70, 0, 140), (80, 0, 140)),
    ("thigh_l", "pelvis", (-10, 0, 95), (-10, 0, 50)),
    ("calf_l", "thigh_l", (-10, 0, 50), (-10, 0, 8)),
    ("thigh_r", "pelvis", (10, 0, 95), (10, 0, 50)),
    ("calf_r", "thigh_r", (10, 0, 50), (10, 0, 8)),
]


def build_stand_in():
    arm_data = bpy.data.armatures.new("Mannequin")
    arm = bpy.data.objects.new("Mannequin", arm_data)
    bpy.context.scene.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    bones = {}
    for name, parent, head, tail in STAND_IN:
        b = arm_data.edit_bones.new(name)
        b.head, b.tail = head, tail
        if parent:
            b.parent = bones[parent]
        bones[name] = b
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


def find_armature():
    for obj in bpy.context.scene.objects:
        if obj.type == "ARMATURE":
            return obj
    return None


def pose_at(spec_pose):
    return {bone: [math.radians(a) for a in angles] for bone, angles in spec_pose.items()}


def key_pose(arm, frame, pose, loc, rest_loc):
    for pb in arm.pose.bones:
        pb.rotation_mode = "XYZ"
        rot = pose.get(pb.name, [0.0, 0.0, 0.0])
        pb.rotation_euler = rot
        pb.keyframe_insert("rotation_euler", frame=frame)
    pelvis = arm.pose.bones.get("pelvis")
    if pelvis:
        # centimetres in Unreal; the FBX import brings Blender units, so keep the offset in the same unit as the rig.
        dx, dy, dz = loc.get("pelvis", [0, 0, 0])
        pelvis.location = (dy, dx, dz) if rest_loc is None else (rest_loc[0] + dy, rest_loc[1] + dx, rest_loc[2] + dz)
        pelvis.keyframe_insert("location", frame=frame)


def new_action(arm, name):
    arm.animation_data_create()
    action = bpy.data.actions.new(name)
    arm.animation_data.action = action
    return action


def build(spec, arm):
    fps = spec["fps"]
    bpy.context.scene.render.fps = fps
    bpy.ops.object.mode_set(mode="POSE")
    enter_keys = spec["enter"]["keys"]
    labels = {k.get("label"): k for k in enter_keys if k.get("label")}
    actions = []

    enter = new_action(arm, spec["enter"]["name"])
    for k in enter_keys:
        key_pose(arm, k["f"], pose_at(k["pose"]), k.get("loc", {}), None)
    actions.append(enter)

    ex = spec["exit"]
    exit_action = new_action(arm, ex["name"])
    frame = 0
    for label in ex["from_enter"]:
        k = labels[label]
        key_pose(arm, frame, pose_at(k["pose"]), k.get("loc", {}), None)
        frame += ex["frames_between"]
    key_pose(arm, frame, pose_at(ex["end_pose"]), {}, None)
    actions.append(exit_action)
    bpy.ops.object.mode_set(mode="OBJECT")
    return actions


def fcurves_of(action):
    """F-curves of an action: Blender 5 layered actions, or the old flat list."""
    if hasattr(action, "fcurves"):
        return list(action.fcurves)
    curves = []
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                curves.extend(bag.fcurves)
    return curves


def mirror_pose(pose):
    out = {}
    for bone, angles in pose.items():
        if bone.endswith("_l"):
            name = bone[:-2] + "_r"
        elif bone.endswith("_r"):
            name = bone[:-2] + "_l"
        else:
            name = bone
        out[name] = [-angles[0], angles[1], angles[2]]
    return out


def mirror_loc(loc):
    return {b: [v[0], -v[1], v[2]] for b, v in loc.items()}


def build_extra(spec, arm):
    """Right-door versions and the generic clips (get-ups, light hits)."""
    actions = []
    bpy.ops.object.mode_set(mode="POSE")
    mirror = spec.get("mirror")
    if mirror:
        enter_keys = spec["enter"]["keys"]
        labels = {k.get("label"): k for k in enter_keys if k.get("label")}
        act = new_action(arm, mirror["suffixes"]["enter"])
        for k in enter_keys:
            key_pose(arm, k["f"], pose_at(mirror_pose(k["pose"])), mirror_loc(k.get("loc", {})), None)
        actions.append(act)
        ex = spec["exit"]
        act = new_action(arm, mirror["suffixes"]["exit"])
        frame = 0
        for label in ex["from_enter"]:
            k = labels[label]
            key_pose(arm, frame, pose_at(mirror_pose(k["pose"])), mirror_loc(k.get("loc", {})), None)
            frame += ex["frames_between"]
        key_pose(arm, frame, pose_at({}), {}, None)
        actions.append(act)
    for clip in spec.get("clips", []):
        act = new_action(arm, clip["name"])
        for k in clip["keys"]:
            key_pose(arm, k["f"], pose_at(k["pose"]), k.get("loc", {}), None)
        actions.append(act)
    bpy.ops.object.mode_set(mode="OBJECT")
    return actions


def check(spec, actions, arm):
    ok = True
    for action in actions:
        moved = set()
        for fc in fcurves_of(action):
            values = [kp.co[1] for kp in fc.keyframe_points]
            if max(values) - min(values) > 1e-4 and fc.data_path.startswith("pose.bones"):
                moved.add(fc.data_path.split('"')[1])
        start, end = action.frame_range
        print(f"{action.name}: frames {start:.0f}-{end:.0f}, bones moving: {len(moved)}")
        light = action.name.startswith(("AM_HitLight", "AM_HandsUp"))
        if end - start < (12 if light else 20) or len(moved) < (3 if light else 6):
            print(f"FAIL {action.name}: too short or too few bones move")
            ok = False
        if not light:
            for needed in ("pelvis", "thigh_l" if "Left" in action.name or "GetUp" in action.name else "thigh_r"):
                if needed not in moved:
                    print(f"FAIL {action.name}: {needed} never moves")
                    ok = False
    # every bone named in the spec must exist in the armature
    names = {b.name for b in arm.pose.bones}
    used = set()
    for k in spec["enter"]["keys"]:
        used.update(k["pose"].keys())
    for clip in spec.get("clips", []):
        for k in clip["keys"]:
            used.update(k["pose"].keys())
    for missing in sorted(used - names):
        print(f"FAIL bone {missing} not in the armature")
        ok = False
    return ok


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--input")
    ap.add_argument("--spec", default=DEFAULT_SPEC)
    args = ap.parse_args(argv)
    spec = json.load(open(args.spec))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if args.input:
        bpy.ops.import_scene.fbx(filepath=args.input)
        arm = find_armature()
        if arm is None:
            print("FAIL no armature in the input")
            sys.exit(1)
    else:
        arm = build_stand_in()
    bpy.context.view_layer.objects.active = arm
    actions = build(spec, arm)
    actions += build_extra(spec, arm)
    ok = check(spec, actions, arm)
    os.makedirs(args.out, exist_ok=True)
    for action in actions:
        arm.animation_data.action = action
        path = os.path.join(args.out, action.name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=path, use_selection=False, bake_anim=True,
                                 bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                                 add_leaf_bones=False, object_types={"ARMATURE"})
        print("wrote", path, os.path.getsize(path), "bytes")
    print("PASS" if ok else "FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
