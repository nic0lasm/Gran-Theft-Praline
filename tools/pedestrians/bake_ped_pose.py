"""Step 2 of the far-crowd mesh (Blender, no Unreal): freeze the walk pose into one static mesh.

    blender --background --python-exit-code 1 --python tools/pedestrians/bake_ped_pose.py -- \
        --dir <staging>/ped --stage export|finish [--phase 0.25] [--texture 512]

export: import SKM.fbx and Anim.fbx, apply the animation at <phase> of the loop (legs apart about
        25 %), bake the pose with the armature modifier, one mesh, raw GLB (_raw.glb);
        decimation is tools/vehicles/decimate_parked.py <dir> --tris 1500 (meshoptimizer);
finish: import _dec.glb, keep orientation and scale of the mannequin, feet at Z=0 on the centre,
        write SM_PedFar_A.fbx and ped.json.
Run through tools/pedestrians/make_ped_far.sh.
"""
import argparse
import json
import os
import sys

import bpy


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--dir", required=True)
    p.add_argument("--stage", choices=("export", "finish"), required=True)
    p.add_argument("--phase", type=float, default=0.25)
    return p.parse_args(argv)


def armature_of(objs):
    return next(o for o in objs if o.type == "ARMATURE")


def main():
    a = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if a.stage == "export":
        bpy.ops.import_scene.fbx(filepath=os.path.join(a.dir, "SKM.fbx"))
        skin_objs = list(bpy.context.scene.objects)
        arm = armature_of(skin_objs)
        before = set(bpy.data.actions)
        bpy.ops.import_scene.fbx(filepath=os.path.join(a.dir, "Anim.fbx"))
        actions = [x for x in bpy.data.actions if x not in before] or list(bpy.data.actions)
        action = max(actions, key=lambda x: x.frame_range[1] - x.frame_range[0])
        # Anim.fbx brings its own armature and mesh-less skeleton: drop everything it created.
        for o in list(bpy.context.scene.objects):
            if o not in skin_objs:
                bpy.data.objects.remove(o)
        arm.animation_data_create()
        arm.animation_data.action = action
        slots = getattr(action, "slots", None)  # Blender 4.4+: an action is bound through a slot
        if slots is not None and len(slots) and hasattr(arm.animation_data, "action_slot"):
            arm.animation_data.action_slot = slots[0]
        f0, f1 = action.frame_range
        frame = f0 + a.phase * (f1 - f0)
        bpy.context.scene.frame_set(int(frame), subframe=frame - int(frame))
        bpy.context.view_layer.update()
        meshes = [o for o in skin_objs if o.type == "MESH"]
        for m in meshes:
            if m.data.shape_keys:
                bpy.context.view_layer.objects.active = m
                m.shape_key_clear()
            bpy.context.view_layer.objects.active = m
            for mod in list(m.modifiers):
                if mod.type == "ARMATURE":
                    bpy.ops.object.modifier_apply(modifier=mod.name)
            m.parent = None
        bpy.data.objects.remove(arm)
        bpy.ops.object.select_all(action="DESELECT")
        for m in meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        if len(meshes) > 1:
            bpy.ops.object.join()
        body = bpy.context.view_layer.objects.active
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        bpy.ops.export_scene.gltf(filepath=os.path.join(a.dir, "_raw.glb"), export_format="GLB",
                                  export_materials="NONE", use_selection=False, export_apply=False)
        print(f"[ped] posed at frame {frame:.1f} of {f0:.0f}-{f1:.0f} ({action.name})")
    else:
        bpy.ops.import_scene.gltf(filepath=os.path.join(a.dir, "_dec.glb"))
        body = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
        body.name = "SM_PedFar_A"
        body.data.name = body.name
        bpy.context.view_layer.objects.active = body
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        xs = [v.co.x for v in body.data.vertices]
        ys = [v.co.y for v in body.data.vertices]
        zs = [v.co.z for v in body.data.vertices]
        for v in body.data.vertices:  # feet on Z=0, centred on the footprint
            v.co.x -= (max(xs) + min(xs)) / 2
            v.co.y -= (max(ys) + min(ys)) / 2
            v.co.z -= min(zs)
        height = max(zs) - min(zs)
        tris = sum(len(p.vertices) - 2 for p in body.data.polygons)
        bpy.ops.object.select_all(action="DESELECT")
        body.select_set(True)
        bpy.ops.export_scene.fbx(
            filepath=os.path.join(a.dir, "SM_PedFar_A.fbx"), use_selection=True, object_types={"MESH"},
            apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", path_mode="STRIP",
            mesh_smooth_type="OFF", axis_forward="-Z", axis_up="Y")
        with open(os.path.join(a.dir, "ped.json"), "w", encoding="utf-8") as f:
            json.dump({"triangles": tris, "height_m": round(height, 3)}, f, indent=1)
        print(f"[ped] SM_PedFar_A: {tris} tris, {height:.2f} m")


main()
