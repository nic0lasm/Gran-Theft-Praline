"""Build the facade kit, export it for Unreal, and assemble a street to check it.

Runs with Blender's Python (Blender 4.2+), either the app or the bpy module:
  blender -b -P tools/city/facade_kit/assemble.py -- --name presqu-ile --focus rue-de-la-republique
  python tools/city/facade_kit/assemble.py --name presqu-ile ...      (pip/uv package "bpy", Python 3.11)

Inputs (from generate.py): gen/<focus>/facade_modules.csv, buildings_gen.csv, shell.glb
Outputs (regenerated, not committed): gen/<focus>/kit/
  facade_kit.glb      one node per module (SM_Facade_*), pivot bottom centre, MI_* slots
  kit_manifest.json   module -> nominal width/height, triangles; kind -> module per family
  assembled.blend     optional (--save-blend), the street with kit instances and signs
  render_<view>.png   Cycles stills (--view label:ex,ey,ez,tx,ty,tz in local metres)

Placement is the same rule the Unreal script uses (place_facades.py): module at
(x, y, z), yaw so its -Y side faces outward, x scaled by width/nominal width and
z by height/nominal height; a cornice (Haussmann) or genoise (Lyonnais) on top
of every street bay of the last storey; a sign with the shop's real name or
brand on every shop fascia.
"""

import argparse
import csv
import json
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kit  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))


def args_from_cli():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", default="presqu-ile")
    ap.add_argument("--focus", default="rue-de-la-republique")
    ap.add_argument("--geo", default=os.path.join(REPO, "assets", "geo"))
    ap.add_argument("--view", action="append", default=[])
    ap.add_argument("--radius", type=float, default=220.0, help="only assemble modules this close to the first view")
    ap.add_argument("--samples", type=int, default=48)
    ap.add_argument("--width", type=int, default=1600)
    ap.add_argument("--height", type=int, default=900)
    ap.add_argument("--save-blend", action="store_true")
    ap.add_argument("--kit-only", action="store_true")
    ap.add_argument("--logos", action="store_true", help="brand logos on the fascias (local renders only)")
    return ap.parse_args(argv)


# --------------------------------------------------------------------------- preview materials
def node_material(name, build):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bsdf.outputs[0], out.inputs[0])
    build(nt, bsdf)
    return m


def stone(nt, bsdf, c1, c2, mortar, block=(0.62, 0.36), tint_from_object=False):
    """Ashlar or render: brick pattern on the metre UVs, noise for weathering."""
    tc = nt.nodes.new("ShaderNodeTexCoord")
    brick = nt.nodes.new("ShaderNodeTexBrick")
    brick.inputs["Scale"].default_value = 1.0
    brick.inputs["Brick Width"].default_value = block[0]
    brick.inputs["Row Height"].default_value = block[1]
    brick.inputs["Mortar Size"].default_value = 0.006 if mortar else 0.0
    brick.inputs["Color1"].default_value = (*c1, 1)
    brick.inputs["Color2"].default_value = (*c2, 1)
    brick.inputs["Mortar"].default_value = (c2[0] * 0.8, c2[1] * 0.8, c2[2] * 0.8, 1)
    nt.links.new(tc.outputs["UV"], brick.inputs["Vector"])
    noise = nt.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 1.8
    noise.inputs["Detail"].default_value = 8.0
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 0.35
    nt.links.new(brick.outputs["Color"], mix.inputs["A"])
    nt.links.new(noise.outputs["Color"], mix.inputs["B"])
    col = mix.outputs["Result"]
    if tint_from_object:
        info = nt.nodes.new("ShaderNodeObjectInfo")
        tint = nt.nodes.new("ShaderNodeMix")
        tint.data_type = "RGBA"
        tint.blend_type = "MULTIPLY"
        tint.inputs["Factor"].default_value = 1.0
        nt.links.new(col, tint.inputs["A"])
        nt.links.new(info.outputs["Color"], tint.inputs["B"])
        col = tint.outputs["Result"]
    nt.links.new(col, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.85
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.25
    nt.links.new(brick.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])


def setup_preview_materials():
    node_material("MI_Facade_LimestoneCream", lambda nt, b: stone(nt, b, (0.70, 0.64, 0.53), (0.64, 0.58, 0.47), True))
    node_material("MI_Facade_LimestoneGreyPlinth",
                  lambda nt, b: stone(nt, b, (0.42, 0.41, 0.39), (0.38, 0.37, 0.35), True, (0.9, 0.6)))
    node_material("MI_Facade_Render", lambda nt, b: stone(nt, b, (0.95, 0.93, 0.9), (0.9, 0.88, 0.85), False,
                                                          tint_from_object=True))
    node_material("MI_Facade_PierreDoree", lambda nt, b: stone(nt, b, (0.66, 0.50, 0.30), (0.6, 0.45, 0.27), True))

    def glass(nt, b):
        b.inputs["Base Color"].default_value = (0.03, 0.035, 0.04, 1)
        b.inputs["Roughness"].default_value = 0.03
        b.inputs["Specular IOR Level"].default_value = 0.8
    node_material("MI_Glass_Window", glass)

    def shop_glass(nt, b):
        b.inputs["Base Color"].default_value = (0.92, 0.95, 0.95, 1)
        b.inputs["Roughness"].default_value = 0.02
        b.inputs["Transmission Weight"].default_value = 1.0
        b.inputs["IOR"].default_value = 1.5
    node_material("MI_Glass_Shop", shop_glass)

    def ground(nt, b):
        stone(nt, b, (0.52, 0.50, 0.47), (0.47, 0.45, 0.42), True, (0.8, 0.8))
        b.inputs["Roughness"].default_value = 0.7
    node_material("M_Preview_Paving", ground)

    def shell(nt, b):
        attr = nt.nodes.new("ShaderNodeVertexColor")
        nt.links.new(attr.outputs["Color"], b.inputs["Base Color"])
        b.inputs["Roughness"].default_value = 0.75
    node_material("M_Preview_Shell", shell)

    def sign_letters(nt, b):
        b.inputs["Base Color"].default_value = (0.9, 0.88, 0.82, 1)
        b.inputs["Emission Color"].default_value = (1.0, 0.95, 0.85, 1)
        b.inputs["Emission Strength"].default_value = 0.6
    node_material("MI_Sign_Letters", sign_letters)

    def interior(nt, b):
        b.inputs["Base Color"].default_value = (0.85, 0.82, 0.76, 1)
        b.inputs["Emission Color"].default_value = (1.0, 0.92, 0.8, 1)
        b.inputs["Emission Strength"].default_value = 1.2  # shop lighting seen through the glass
    node_material("MI_Shop_Interior", interior)


# --------------------------------------------------------------------------- assembly
def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def place(objname, mesh, col, x, y, z, yaw_ue, sx, sz, colour=None):
    o = bpy.data.objects.new(objname, mesh)
    o.location = (x, y, z)
    o.rotation_euler = (0.0, 0.0, math.radians(-yaw_ue) + math.pi / 2)
    o.scale = (sx, 1.0, sz)
    if colour:
        o.color = (*hex_rgb(colour), 1.0)
    col.objects.link(o)
    return o


def sign_plane(col, image_path, x, y, z, yaw_ue, width, height, out):
    """Textured sign panel on the fascia (the texture made by tools/city/signs.py)."""
    key = os.path.basename(image_path)
    mat = bpy.data.materials.get(key)
    if mat is None:
        def build(nt, b):
            tex = nt.nodes.new("ShaderNodeTexImage")
            tex.image = bpy.data.images.load(image_path, check_existing=True)
            nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
            nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"])
            b.inputs["Emission Strength"].default_value = 0.15  # back-lit letters read at dusk
            b.inputs["Roughness"].default_value = 0.35
        mat = node_material(key, build)
    me = bpy.data.meshes.get("SignPlane")
    if me is None:
        bpy.ops.mesh.primitive_plane_add(size=1.0)
        tmp = bpy.context.active_object
        me = tmp.data
        me.name = "SignPlane"
        bpy.data.objects.remove(tmp)
        me.transform(__import__("mathutils").Matrix.Rotation(math.pi / 2, 4, "X"))  # plane in xz, facing -Y
    o = bpy.data.objects.new("Sign_" + key, me.copy())
    o.data.materials.append(mat)
    col.objects.link(o)
    a = math.radians(-yaw_ue)
    nx, ny = math.cos(a), math.sin(a)
    o.location = (x + nx * out, y + ny * out, z)
    o.rotation_euler = (0.0, 0.0, a + math.pi / 2)
    o.scale = (width, 1.0, height)
    return o


def main():
    args = args_from_cli()
    gen = os.path.join(args.geo, "processed", args.name, "gen", args.focus)
    out = os.path.join(gen, "kit")
    os.makedirs(out, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene

    kit_col = bpy.data.collections.new("Kit")
    scene.collection.children.link(kit_col)
    modules = kit.build_kit(kit_col)
    manifest = {"modules": {}, "kind_to_module": kit.KIND_TO_MODULE, "family_of_typology": kit.FAMILY_OF_TYPOLOGY}
    for name, (obj, w, h) in modules.items():
        obj.data.calc_loop_triangles()
        manifest["modules"][name] = {"nominal_width_m": w, "nominal_height_m": h,
                                     "triangles": len(obj.data.loop_triangles),
                                     "materials": [m.name for m in obj.data.materials]}
    for o in bpy.data.objects:
        o.select_set(o.name in modules)
    bpy.ops.export_scene.gltf(filepath=os.path.join(out, "facade_kit.glb"), use_selection=True,
                              export_format="GLB", export_apply=True)
    with open(os.path.join(out, "kit_manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    print("kit: %d modules, %d triangles in total" % (len(modules),
                                                     sum(m["triangles"] for m in manifest["modules"].values())))
    if args.kit_only:
        return
    kit_col.hide_render = True
    kit_col.hide_viewport = True

    setup_preview_materials()
    with open(os.path.join(gen, "facade_modules.csv"), encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    views = []
    for spec in args.view:
        label, nums = spec.split(":")
        views.append((label, [float(v) for v in nums.split(",")]))
    cx, cy = (views[0][1][0], views[0][1][1]) if views else (0.0, 0.0)

    signs_dir = os.path.join(gen, "signs")
    sign_of = {}
    if os.path.exists(os.path.join(signs_dir, "signs.csv")):
        with open(os.path.join(signs_dir, "signs.csv"), encoding="utf-8") as f:
            sign_of = {r["shop_osm_id"]: r for r in csv.DictReader(f) if r["has_fascia"] == "True"}
    street = bpy.data.collections.new("Street")
    scene.collection.children.link(street)
    top = {}
    roof_kinds = ("dormer", "mansard_blank")   # the cornice crowns the last stone storey (place_facades.py)
    for r in rows:
        if r["kind"] not in roof_kinds:
            top[r["building_id"]] = max(top.get(r["building_id"], 0), int(r["storey"]))
    placed = signs = 0
    for r in rows:
        x, y, z = float(r["x_m"]), float(r["y_m"]), float(r["z_m"])
        if math.hypot(x - cx, y - cy) > args.radius:
            continue
        fam = kit.FAMILY_OF_TYPOLOGY[r["typology"]]
        name = kit.module_for(r["typology"], r["kind"], r["building_id"])
        obj, nw, nh = modules[name]
        w, h, yaw = float(r["width_m"]), float(r["height_m"]), float(r["ue_yaw"])
        place(name + "_i", obj.data, street, x, y, z, yaw, w / nw, h / nh, r["colour"])
        placed += 1
        if r["side"] == "street" and r["kind"] not in roof_kinds and int(r["storey"]) == top.get(r["building_id"]):
            eave = kit.KIND_TO_MODULE[fam]["_eave"]
            place(eave + "_i", modules[eave][0].data, street, x, y, z + h, yaw, w / nw, 1.0, r["colour"])
        sg = sign_of.get(r["shop_osm_id"]) if r["kind"] == "shop" else None
        if sg:
            tex = sg["logo_texture"] if args.logos and sg["logo_texture"] else sg["texture"]
            zc, out_m = kit.sign_frame(name, fam, h, nh)
            fa = kit.FASCIA[fam]
            sign_plane(street, os.path.join(signs_dir, tex + ".png"), x, y, z + zc, yaw,
                       w * fa["width_share"] - 0.04, fa["height_m"] * h / nh - 0.04, out_m)
            signs += 1
    print("assembled %d modules and %d signs within %.0f m" % (placed, signs, args.radius))

    shell = os.path.join(gen, "shell.glb")
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=shell)
    for o in set(bpy.data.objects) - before:
        if o.type == "MESH":
            o.data.materials.clear()
            o.data.materials.append(bpy.data.materials["M_Preview_Shell"])

    # Ground: paving around the view (the pedestrian axis has light slabs).
    bpy.ops.mesh.primitive_plane_add(size=2 * args.radius + 200, location=(cx, cy, 0.0))
    g = bpy.context.active_object
    g.data.materials.append(bpy.data.materials["M_Preview_Paving"])
    g.location.z = min(float(r["z_m"]) for r in rows if math.hypot(float(r["x_m"]) - cx, float(r["y_m"]) - cy) < 60) \
        if rows else 0.0
    for li, loop in enumerate(g.data.loops):  # UVs in metres
        co = g.data.vertices[loop.vertex_index].co
        g.data.uv_layers.active.data[li].uv = (co.x, co.y)

    # Sky and sun: October midday over Lyon (art bible reference state).
    world = bpy.data.worlds.new("Sky")
    scene.world = world
    world.use_nodes = True
    sky = world.node_tree.nodes.new("ShaderNodeTexSky")
    sky.sky_type = "NISHITA"
    sky.sun_elevation = math.radians(33)
    sky.sun_rotation = math.radians(200)
    world.node_tree.links.new(sky.outputs[0], world.node_tree.nodes["Background"].inputs[0])
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.35
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    scene.collection.objects.link(sun)
    sun.data.energy = 3.2
    sun.data.angle = math.radians(0.6)
    sun.rotation_euler = (math.radians(90 - 33), 0, math.radians(200 - 90))

    scene.render.engine = "CYCLES"
    scene.cycles.samples = args.samples
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y = args.width, args.height
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.view_settings.exposure = -0.4
    if args.save_blend:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "assembled.blend"))
    for label, v in views:
        cam = bpy.data.objects.new("Cam_" + label, bpy.data.cameras.new("Cam_" + label))
        scene.collection.objects.link(cam)
        cam.data.lens = 24
        cam.location = v[:3]
        d = (v[3] - v[0], v[4] - v[1], v[5] - v[2])
        cam.rotation_euler = (math.atan2(math.hypot(d[0], d[1]), -d[2]), 0.0, math.atan2(d[1], d[0]) - math.pi / 2)
        scene.camera = cam
        scene.render.filepath = os.path.join(out, "render_%s.png" % label)
        bpy.ops.render.render(write_still=True)
        print(scene.render.filepath)


if __name__ == "__main__":
    main()
