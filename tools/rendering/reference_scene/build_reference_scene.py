"""Build the reference street scene (greybox v0) inside the Unreal Editor.

Run from the editor (Tools > Execute Python Script) or headless:
  UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
      -script="<repo>/tools/rendering/reference_scene/build_reference_scene.py"

Creates under /Game/ReferenceScene:
  Geo/         static meshes imported from assets/geo/processed/rue-republique
  Materials/   M_Greybox and its dry / wet instances
  Sequences/   LS_StreetPath (60 s camera path along rue de la République)
  Movie/       MRQ_Still_<n> configs (one frame each) for still captures
  Maps/        ReferenceStreet_<State> for the four lighting states

Physical units: sun in lux, local lights in lumens, exposure in EV100.
See docs/rendering/rendering-strategy.md, section 7.
"""

import json
import math
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
GEO_NAME = "rue-republique"
STREET = "Rue de la République"
ROOT = "/Game/ReferenceScene"
FPS = 30
PATH_SECONDS = 60
EYE_HEIGHT_CM = 170.0
STILL_FRAMES = (300, 900, 1500)

# Lyon local time is UTC+2 in summer, UTC+1 in winter.
STATES = {
    "Noon": dict(date=(2026, 6, 21), hour_utc=11.75, sun_lux=100000.0, cloud=0.25, fog=0.01,
                 wet=False, night=False, ev=(10.0, 15.0)),
    "GoldenHour": dict(date=(2026, 10, 15), hour_utc=16.25, sun_lux=100000.0, cloud=0.35, fog=0.02,
                       wet=False, night=False, ev=(7.0, 14.0)),
    "Rain": dict(date=(2026, 11, 9), hour_utc=10.0, sun_lux=20000.0, cloud=0.95, fog=0.06,
                 wet=True, night=False, ev=(7.0, 12.0)),
    "Night": dict(date=(2026, 11, 9), hour_utc=21.5, sun_lux=0.0, cloud=0.4, fog=0.03,
                  wet=True, night=True, ev=(-2.0, 6.0)),
}

STREET_LIGHT = dict(spacing_m=25.0, offset_m=6.0, height_m=5.0, lumens=4000.0, kelvin=3000.0,
                    radius_cm=2500.0)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log("[ReferenceScene] " + msg)


# --------------------------------------------------------------------------- sun

def sun_angles(lat, lon, year, month, day, hour_utc):
    """NOAA solar position: returns (elevation_deg, azimuth_deg from north, clockwise)."""
    n = (month - 1) * 30.6 + day  # rough day of year, enough for lighting
    if month > 2:
        n -= 2 if year % 4 else 1
    gamma = 2 * math.pi / 365 * (n - 1 + (hour_utc - 12) / 24)
    eqtime = 229.18 * (0.000075 + 0.001868 * math.cos(gamma) - 0.032077 * math.sin(gamma)
                       - 0.014615 * math.cos(2 * gamma) - 0.040849 * math.sin(2 * gamma))
    decl = (0.006918 - 0.399912 * math.cos(gamma) + 0.070257 * math.sin(gamma)
            - 0.006758 * math.cos(2 * gamma) + 0.000907 * math.sin(2 * gamma)
            - 0.002697 * math.cos(3 * gamma) + 0.00148 * math.sin(3 * gamma))
    tst = hour_utc * 60 + eqtime + 4 * lon
    ha = math.radians(tst / 4 - 180)
    phi = math.radians(lat)
    cos_zen = math.sin(phi) * math.sin(decl) + math.cos(phi) * math.cos(decl) * math.cos(ha)
    zen = math.acos(max(-1.0, min(1.0, cos_zen)))
    az = math.degrees(math.atan2(math.sin(ha), math.cos(ha) * math.sin(phi) - math.tan(decl) * math.cos(phi))) + 180
    return 90 - math.degrees(zen), az % 360


def sun_rotation(elevation, azimuth):
    """Directional light rotation in a world where +X = east, +Y = south.

    A light points along its +X axis, i.e. away from the sun. Unreal yaw is
    measured from +X towards +Y (east towards south), so compass azimuth
    (from north, clockwise) maps to yaw = azimuth - 90; the light points to
    the opposite direction.
    """
    yaw = (azimuth - 90.0) + 180.0
    return unreal.Rotator(roll=0.0, pitch=-elevation, yaw=yaw)


# --------------------------------------------------------------------------- assets

def import_geo():
    dest = ROOT + "/Geo"
    glb = os.path.join(REPO, "assets", "geo", "processed", GEO_NAME, GEO_NAME + ".glb")
    if eal.does_directory_exist(dest) and eal.list_assets(dest, recursive=False):
        log("geo already imported")
    else:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", glb)
        task.set_editor_property("destination_path", dest)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        asset_tools.import_asset_tasks([task])
        log("imported %s" % glb)
    meshes = []
    for path in eal.list_assets(dest, recursive=True):
        asset = eal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            meshes.append(asset)
    log("%d static meshes" % len(meshes))
    return meshes


def make_materials():
    folder = ROOT + "/Materials"
    base_path = folder + "/M_Greybox"
    if eal.does_asset_exist(base_path):
        base = eal.load_asset(base_path)
    else:
        base = asset_tools.create_asset("M_Greybox", folder, unreal.Material, unreal.MaterialFactoryNew())
        mel = unreal.MaterialEditingLibrary
        albedo = mel.create_material_expression(base, unreal.MaterialExpressionScalarParameter, -400, 0)
        albedo.set_editor_property("parameter_name", "Albedo")
        albedo.set_editor_property("default_value", 0.35)
        rough = mel.create_material_expression(base, unreal.MaterialExpressionScalarParameter, -400, 200)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", 0.8)
        mel.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.recompile_material(base)
        eal.save_loaded_asset(base)

    def instance(name, albedo, roughness):
        path = folder + "/" + name
        if eal.does_asset_exist(path):
            mi = eal.load_asset(path)
        else:
            mi = asset_tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                          unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", base)
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "Albedo", albedo)
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
        eal.save_loaded_asset(mi)
        return mi

    # Albedo values within the physically plausible range for stone, render and asphalt.
    return {
        "facade_dry": instance("MI_Facade_Dry", 0.40, 0.85),
        "facade_wet": instance("MI_Facade_Wet", 0.30, 0.45),
        "ground_dry": instance("MI_Ground_Dry", 0.18, 0.75),
        "ground_wet": instance("MI_Ground_Wet", 0.10, 0.12),
    }


# --------------------------------------------------------------------------- path

def load_meta():
    path = os.path.join(REPO, "assets", "geo", "processed", GEO_NAME, GEO_NAME + ".meta.json")
    with open(path) as f:
        return json.load(f)


def resample(points, step):
    """Resample a polyline [(x, y)] every `step` units; returns points and headings."""
    out = []
    acc = 0.0
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        seg = math.hypot(x1 - x0, y1 - y0)
        heading = math.degrees(math.atan2(y1 - y0, x1 - x0))
        t = acc
        while t <= seg:
            f = t / seg if seg else 0
            out.append((x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, heading))
            t += step
        acc = t - seg
    return out


def make_sequence(axis_cm):
    folder = ROOT + "/Sequences"
    path = folder + "/LS_StreetPath"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    seq = asset_tools.create_asset("LS_StreetPath", folder, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    frames = FPS * PATH_SECONDS
    seq.set_display_rate(unreal.FrameRate(FPS, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(frames)

    # Spawnable camera so the sequence works in any of the four maps.
    camera = seq.add_spawnable_from_class(unreal.CineCameraActor)
    # Disable depth of field: the default manual focus at 1 km blurs nearby facades.
    template = camera.get_object_template()
    focus = template.camera_component.get_editor_property("focus_settings")
    focus.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE)
    template.camera_component.set_editor_property("focus_settings", focus)
    cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
    cut = cut_track.add_section()
    cut.set_range(0, frames)
    cut.set_camera_binding_id(seq.get_binding_id(camera))

    transform = camera.add_track(unreal.MovieScene3DTransformTrack)
    section = transform.add_section()
    section.set_range(0, frames)
    channels = section.get_all_channels()

    samples = resample(axis_cm, 100.0)
    for i in range(frames + 1):
        pos = (len(samples) - 1) * i / frames
        a = samples[int(pos)]
        b = samples[min(int(pos) + 1, len(samples) - 1)]
        f = pos - int(pos)
        x, y = a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f
        if i % 15:
            continue  # one key every half second, cubic interpolation in between
        t = unreal.FrameNumber(i)  # display-rate frame (add_key default time unit)
        # look slightly to the right side facades, like a pedestrian
        yaw = a[2] + 12.0 * math.sin(i / frames * math.pi * 4)
        for ch, v in zip(channels[:6], (x, y, EYE_HEIGHT_CM, 0.0, -3.0, yaw)):
            ch.add_key(t, v)
    eal.save_loaded_asset(seq)
    log("sequence: %d frames along %d samples" % (frames, len(samples)))
    return seq


def make_still_configs():
    folder = ROOT + "/Movie"
    configs = []
    for n, frame in enumerate(STILL_FRAMES, 1):
        name = "MRQ_Still_%d" % n
        path = folder + "/" + name
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
        cfg = asset_tools.create_asset(name, folder, unreal.MoviePipelinePrimaryConfig, None)
        out = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
        out.set_editor_property("output_resolution", unreal.IntPoint(1920, 1080))
        out.set_editor_property("use_custom_playback_range", True)
        out.set_editor_property("custom_start_frame", frame)
        out.set_editor_property("custom_end_frame", frame + 1)
        # render_stills.py overrides the name per job (state and shot).
        out.set_editor_property("file_name_format", "shot%d_frame{frame_number}" % n)
        out.set_editor_property("output_directory", unreal.DirectoryPath(
            os.path.join(REPO, "docs", "rendering", "test-results", "stills")))
        cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
        cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
        aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
        aa.set_editor_property("engine_warm_up_count", 120)  # let Lumen and TSR converge
        aa.set_editor_property("render_warm_up_frames", True)
        eal.save_loaded_asset(cfg)
        configs.append(cfg)
    return configs


# --------------------------------------------------------------------------- level

def spawn(cls, loc=(0, 0, 0), rot=None, label=None):
    actor = actor_sub.spawn_actor_from_class(cls, unreal.Vector(*loc), rot or unreal.Rotator(0, 0, 0))
    if label:
        actor.set_actor_label(label)
    return actor


def build_state(name, st, meshes, mats, meta, seq):
    map_path = ROOT + "/Maps/ReferenceStreet_" + name
    if eal.does_asset_exist(map_path):
        eal.delete_asset(map_path)
    level_sub.new_level(map_path)

    for mesh in meshes:
        actor = spawn(unreal.StaticMeshActor, label=mesh.get_name())
        comp = actor.static_mesh_component
        comp.set_static_mesh(mesh)
        ground = mesh.get_name() in ("Ground", "Roads")
        key = ("ground" if ground else "facade") + ("_wet" if st["wet"] else "_dry")
        for i in range(max(1, len(comp.get_materials()))):
            comp.set_material(i, mats[key])

    lat, lon = meta["latitude_deg"], meta["longitude_deg"]
    elev, az = sun_angles(lat, lon, *st["date"], st["hour_utc"])
    log("%s: sun elevation %.1f, azimuth %.1f" % (name, elev, az))

    sun = spawn(unreal.DirectionalLight, (0, 0, 5000), sun_rotation(max(elev, -10.0), az), "Sun")
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_editor_property("atmosphere_sun_light", True)
    sc.set_editor_property("intensity", st["sun_lux"] if elev > -2 else 0.0)
    sc.set_editor_property("cast_cloud_shadows", True)
    sc.set_mobility(unreal.ComponentMobility.MOVABLE)

    if st["night"]:
        moon = spawn(unreal.DirectionalLight, (0, 0, 5000), unreal.Rotator(0, -35, 30), "Moon")
        mc = moon.get_component_by_class(unreal.DirectionalLightComponent)
        mc.set_editor_property("intensity", 0.3)
        mc.set_editor_property("light_color", unreal.Color(170, 190, 255, 255))
        mc.set_editor_property("atmosphere_sun_light_index", 1)
        mc.set_editor_property("atmosphere_sun_light", True)
        mc.set_mobility(unreal.ComponentMobility.MOVABLE)

    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, label="SkyLight")
    skc = sky.get_component_by_class(unreal.SkyLightComponent)
    skc.set_editor_property("real_time_capture", True)
    skc.set_mobility(unreal.ComponentMobility.MOVABLE)

    # Cloud coverage per state is tuned in a later pass (st["cloud"]); v0 uses the default cloud material.
    spawn(unreal.VolumetricCloud, label="VolumetricCloud")

    fog = spawn(unreal.ExponentialHeightFog, label="HeightFog")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fc.set_editor_property("fog_density", st["fog"])
    fc.set_editor_property("enable_volumetric_fog", True)

    ppv = spawn(unreal.PostProcessVolume, label="PostProcess")
    ppv.set_editor_property("unbound", True)
    s = ppv.get_editor_property("settings")
    for prop, value in (("override_auto_exposure_method", True),
                        ("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM),
                        ("override_auto_exposure_min_brightness", True),
                        ("auto_exposure_min_brightness", st["ev"][0]),
                        ("override_auto_exposure_max_brightness", True),
                        ("auto_exposure_max_brightness", st["ev"][1]),
                        ("override_bloom_intensity", True), ("bloom_intensity", 0.2),
                        ("override_vignette_intensity", True), ("vignette_intensity", 0.25),
                        ("override_scene_fringe_intensity", True), ("scene_fringe_intensity", 0.0)):
        try:
            s.set_editor_property(prop, value)
        except Exception as exc:  # property names differ slightly between versions
            unreal.log_warning("[ReferenceScene] post-process %s: %s" % (prop, exc))
    ppv.set_editor_property("settings", s)

    if st["night"]:
        place_street_lights(meta)

    player = spawn(unreal.PlayerStart, tuple(meta["streets"][STREET]["axis_ue_cm"][0]) + (EYE_HEIGHT_CM,),
                   label="PlayerStart")
    player.set_actor_location(unreal.Vector(meta["streets"][STREET]["axis_ue_cm"][0][0],
                                            meta["streets"][STREET]["axis_ue_cm"][0][1], 100.0), False, False)

    seq_actor = spawn(unreal.LevelSequenceActor, label="StreetPath")
    seq_actor.set_sequence(seq)
    ps = seq_actor.get_editor_property("playback_settings")
    ps.set_editor_property("auto_play", True)
    seq_actor.set_editor_property("playback_settings", ps)

    level_sub.save_current_level()
    log("saved " + map_path)
    return map_path


def place_street_lights(meta):
    axis = [tuple(p) for p in meta["streets"][STREET]["axis_ue_cm"]]
    step = STREET_LIGHT["spacing_m"] * 100.0
    for i, (x, y, heading) in enumerate(resample(axis, step)):
        side = 1 if i % 2 else -1
        h = math.radians(heading)
        ox, oy = -math.sin(h) * side * STREET_LIGHT["offset_m"] * 100.0, math.cos(h) * side * STREET_LIGHT["offset_m"] * 100.0
        light = spawn(unreal.PointLight, (x + ox, y + oy, STREET_LIGHT["height_m"] * 100.0), label="StreetLight_%02d" % i)
        lc = light.get_component_by_class(unreal.PointLightComponent)
        lc.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
        lc.set_editor_property("intensity", STREET_LIGHT["lumens"])
        lc.set_editor_property("use_temperature", True)
        lc.set_editor_property("temperature", STREET_LIGHT["kelvin"])
        lc.set_editor_property("attenuation_radius", STREET_LIGHT["radius_cm"])
        lc.set_editor_property("source_radius", 15.0)
        # ADR-0002: at most about 8 shadow-casting local lights in view
        lc.set_editor_property("cast_shadows", i % 3 == 0)
        lc.set_mobility(unreal.ComponentMobility.MOVABLE)


def main():
    meta = load_meta()
    meshes = import_geo()
    mats = make_materials()
    seq = make_sequence([tuple(p) for p in meta["streets"][STREET]["axis_ue_cm"]])
    make_still_configs()
    maps = [build_state(name, st, meshes, mats, meta, seq) for name, st in STATES.items()]
    log("done: " + ", ".join(maps))


main()
