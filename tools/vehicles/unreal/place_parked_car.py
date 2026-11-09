"""Park a drivable car in /Game/Maps/L_RueRepublique (version 0.3).

The car stands at the kerb a little down the street from the PlayerStart,
parallel to the street, unlocked: walk to it, F (or Y on a gamepad) to drive.
Rerunning replaces the cars this script placed (label ParkedCar_*).

Without the UI, from a copy in a folder without spaces (the editor quits at the end):
    UnrealEditor GranTheftPraline.uproject -unattended -nosplash \
        -ExecutePythonScript=/tmp/place_parked_car.py
GTP_PARKED_CAR picks another Blueprint (default: the Clio V).
"""

import os

import unreal

MAP = "/Game/Maps/L_RueRepublique"
CAR = os.environ.get("GTP_PARKED_CAR", "/Game/Vehicles/Cars/Renault_ClioV/BP_Renault_ClioV")
# From the PlayerStart, in its frame: 14 m down the street, 2.8 m to the right
# (away from the -TestVehicle spot, 6 m ahead and 2.5 m right), 30 cm up to settle.
OFFSET = unreal.Vector(1400.0, 280.0, 30.0)

level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log_warning("[place_parked_car] " + msg)


def main():
    if not level_sub.load_level(MAP):
        raise RuntimeError("could not load " + MAP)
    actors = actor_sub.get_all_level_actors()
    for a in actors:
        if a.get_actor_label().startswith("ParkedCar_"):
            actor_sub.destroy_actor(a)
            log("removed " + a.get_actor_label())
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    if not starts:
        raise RuntimeError("no PlayerStart in " + MAP)
    start = starts[0]
    yaw = start.get_actor_rotation().yaw
    rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)
    loc = start.get_actor_location() + rot.rotate_vector(OFFSET)

    # Rest on the ground: trace down from 5 m above.
    hit = unreal.SystemLibrary.line_trace_single(
        start, loc + unreal.Vector(0, 0, 500), loc - unreal.Vector(0, 0, 1000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    if hit:
        loc.z = hit.to_tuple()[4].z + 30.0  # location of the hit on the ground

    cls = unreal.EditorAssetLibrary.load_blueprint_class(CAR)
    if cls is None:
        raise RuntimeError("car Blueprint not found: " + CAR + " (run import-vehicle-mac.sh)")
    car = actor_sub.spawn_actor_from_class(cls, loc, rot)
    car.set_actor_label("ParkedCar_" + CAR.rsplit("/", 1)[1].replace("BP_", ""))
    car.set_folder_path("Gameplay/Vehicles")
    log("parked %s at %s yaw %.1f" % (car.get_actor_label(), loc, yaw))
    level_sub.save_current_level()
    log("saved " + MAP)


try:
    main()
finally:
    unreal.SystemLibrary.quit_editor()
