"""Render one Haussmann frontage from the kit modules, without the city data, to judge the relief.

Stacks modules bay by bay (shop or entrance, entresol, storeys, cornice), like the placement does,
and renders a Cycles still from the pavement with a low sun, so reveals and mouldings cast shadows.
Runs with the bpy module (pip "bpy", Python 3.13 for bpy 5.x) or Blender:
  python tools/city/facade_kit/preview_stack.py --out stack.png [--bays 5] [--view street|front|close]
"""

import argparse
import math
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import kit  # noqa: E402
from kit_layout import HAUSSMANN  # noqa: E402

GROUND = ["SM_Facade_Haussmann_Shop_B", "SM_Facade_Haussmann_Shop", "SM_Facade_Haussmann_Entrance",
          "SM_Facade_Haussmann_Shop", "SM_Facade_Haussmann_Shop_C", "SM_Facade_Haussmann_ShopShutter"]
STOREYS = [("SM_Facade_Haussmann_Entresol", "entresol"), ("SM_Facade_Haussmann_WindowBalcony_C", "upper"),
           ("SM_Facade_Haussmann_Window_C", "upper"), ("SM_Facade_Haussmann_Window_B", "upper"),
           ("SM_Facade_Haussmann_WindowBalcony", "upper")]
VIEWS = {  # camera location, look-at (metres; facade plane y = 0, street at -Y)
    "street": ((-9.0, -11.0, 1.7), (3.0, 0.0, 9.0)),
    "front": ((0.0, -30.0, 9.0), (0.0, 0.0, 10.0)),
    "close": ((-2.5, -5.0, 1.7), (1.0, 0.0, 6.5)),
}


def args_from_cli():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="stack.png")
    ap.add_argument("--bays", type=int, default=5)
    ap.add_argument("--view", default="street", choices=sorted(VIEWS))
    ap.add_argument("--samples", type=int, default=24)
    ap.add_argument("--width", type=int, default=1200)
    ap.add_argument("--height", type=int, default=900)
    return ap.parse_args(argv)


def look_at(obj, target):
    d = [t - o for t, o in zip(target, obj.location)]
    yaw = math.atan2(d[1], d[0]) - math.pi / 2
    pitch = math.atan2(d[2], math.hypot(d[0], d[1]))
    obj.rotation_euler = (math.pi / 2 + pitch, 0.0, yaw)


def main():
    a = args_from_cli()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    lib = bpy.data.collections.new("kit")
    mods = kit.build_kit(lib)  # not linked to the scene: instanced below
    W = HAUSSMANN["bay"]
    tris = 0
    for i in range(a.bays):
        x = (i - (a.bays - 1) / 2) * W
        z = 0.0
        column = [(GROUND[i % len(GROUND)], "gf")] + STOREYS + [("SM_Facade_Haussmann_Cornice", None)]
        for name, key in column:
            src = mods[name][0]
            ob = bpy.data.objects.new("%s_%d" % (name, i), src.data)
            ob.location = (x, 0.0, z)
            scene.collection.objects.link(ob)
            tris += sum(len(p.vertices) - 2 for p in src.data.polygons)
            z += HAUSSMANN[key] if key else 0.0
    # pavement and road
    bpy.ops.mesh.primitive_plane_add(size=1, location=(0, -12, 0))
    g = bpy.context.object
    g.scale = (80, 24, 1)
    gm = bpy.data.materials.new("ground")
    gm.use_nodes = True
    gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.22, 0.21, 0.2, 1)
    g.data.materials.append(gm)
    # sky and a low sun across the facade, so every projection casts a shadow
    world = bpy.data.worlds.new("w")
    scene.world = world
    world.use_nodes = True
    sky = world.node_tree.nodes.new("ShaderNodeTexSky")
    world.node_tree.links.new(sky.outputs[0], world.node_tree.nodes["Background"].inputs[0])
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.35
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.0
    sun.data.angle = math.radians(0.6)
    sun.rotation_euler = (math.radians(58), 0.0, math.radians(-62))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.lens = 22
    loc, tgt = VIEWS[a.view]
    cam.location = loc
    look_at(cam, tgt)
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = a.samples
    scene.cycles.use_denoising = True
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.exposure = -0.8
    scene.render.resolution_x, scene.render.resolution_y = a.width, a.height
    scene.render.filepath = os.path.abspath(a.out)
    bpy.ops.render.render(write_still=True)
    print("[preview_stack] %s: %d bays, %d triangles in view" % (a.out, a.bays, tris))


main()
