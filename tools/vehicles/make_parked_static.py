"""Static, instanceable body of a prepared car, for parked cars (HISM), in Blender.

    blender --background --python tools/vehicles/make_parked_static.py -- \
        --id V-01 --staging assets/staging/vehicles --stage export|finish [--tris 8000] [--texture 1024]

Run through tools/vehicles/make_parked_static.sh: Blender is far too slow at decimating a 1.4 M
triangle car (18 min, and it missed the budget), so the work is split in three:
  1. export  (Blender): armature applied in rest pose, morph targets dropped, one mesh, raw GLB;
  2. decimate_parked.py (Python, meshoptimizer, seconds): to the triangle budget;
  3. finish  (Blender): origin on the ground at the centre, 1K textures, FBX for Unreal.
Input : assets/staging/vehicles/<id>/SKM_<Name>.fbx and textures/T_<Name>_BC|ORM|N.png (prepare_vehicle.py).
Output: assets/staging/vehicles/<id>/parked/SM_Parked_<Name>_A.fbx + textures at 1K.
Body and wheels are joined into one mesh with one material, decimated to the budget
(8000 triangles at LOD0, the LOD chain 1/0.5/0.25/0.125 is built in Unreal), origin on the
ground at the centre of the car, +X forward like the rolling actor. Glass panels are left out:
the Unreal import gives the body a dark glass look where the body UVs have it; check with a
capture at the first slot. No Unreal needed here.
"""
import argparse
import json
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--id", required=True)
    p.add_argument("--staging", required=True)
    p.add_argument("--stage", choices=("export", "finish"), required=True)
    p.add_argument("--tris", type=int, default=8000)
    p.add_argument("--texture", type=int, default=1024)
    return p.parse_args(argv)


def main():
    a = parse_args()
    root = os.path.join(a.staging, a.id)
    with open(os.path.join(root, "report.json"), encoding="utf-8") as f:
        report = json.load(f)
    name = report.get("name") or json.load(open(os.path.join(HERE, "vehicles.json")))[a.id]["name"]
    out = os.path.join(root, "parked")
    os.makedirs(os.path.join(out, "textures"), exist_ok=True)

    if a.stage == "export":
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=os.path.join(root, f"SKM_{name}.fbx"))
        meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        arm = [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]
        # Bake the rest pose into the mesh and drop the skeleton.
        for m in meshes:
            if m.data.shape_keys:  # Dent_* morph targets: a parked car does not need them
                bpy.context.view_layer.objects.active = m
                m.shape_key_clear()
            for mod in list(m.modifiers):
                if mod.type == "ARMATURE":
                    bpy.context.view_layer.objects.active = m
                    bpy.ops.object.modifier_apply(modifier=mod.name)
            m.parent = None
        for o in arm:
            bpy.data.objects.remove(o)
        bpy.ops.object.select_all(action="DESELECT")
        for m in meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
        body = bpy.context.view_layer.objects.active
        body.name = f"SM_Parked_{name}_A"
        body.data.name = body.name
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        while len(body.material_slots) > 1:  # one material
            body.active_material_index = len(body.material_slots) - 1
            bpy.ops.object.material_slot_remove()

        bpy.ops.export_scene.gltf(filepath=os.path.join(out, "_raw.glb"), use_selection=False,
                                  export_format="GLB", export_materials="NONE", export_apply=False)
        print(f"[parked] {a.id} raw export done")
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.gltf(filepath=os.path.join(out, "_dec.glb"))
        body = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
        bpy.context.view_layer.objects.active = body
        body.name = f"SM_Parked_{name}_A"
        body.data.name = body.name
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        tris = sum(len(p.vertices) - 2 for p in body.data.polygons)

        # Origin: ground (min Z), centre of the footprint.
        zs = [v.co.z for v in body.data.vertices]
        xs = [v.co.x for v in body.data.vertices]
        ys = [v.co.y for v in body.data.vertices]
        for v in body.data.vertices:
            v.co.x -= (max(xs) + min(xs)) / 2
            v.co.y -= (max(ys) + min(ys)) / 2
            v.co.z -= min(zs)

        for kind in ("BC", "ORM", "N"):
            src = os.path.join(root, "textures", f"T_{name}_{kind}.png")
            img = bpy.data.images.load(src)
            if img.size[0] > a.texture:
                img.scale(a.texture, a.texture)
            img.filepath_raw = os.path.join(out, "textures", f"T_Parked_{name}_{kind}.png")
            img.file_format = "PNG"
            img.save()

        bpy.ops.object.select_all(action="DESELECT")
        body.select_set(True)
        # The source FBX are in centimetres: the vertices hold centimetre values, so export with a
        # centimetre scene unit (as prepare_vehicle.py does), or Unreal gets everything x100.
        bpy.context.scene.unit_settings.scale_length = 0.01
        bpy.ops.export_scene.fbx(
            filepath=os.path.join(out, f"{body.name}.fbx"), use_selection=True, object_types={"MESH"},
            apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", path_mode="STRIP",
            mesh_smooth_type="OFF", use_tspace=True, axis_forward="-Z", axis_up="Y")
        with open(os.path.join(out, "parked.json"), "w", encoding="utf-8") as f:
            json.dump({"id": a.id, "name": name, "triangles": tris, "texture_px": a.texture}, f, indent=1)
        print(f"[parked] {a.id} {name}: {tris} tris")


main()
