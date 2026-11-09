"""Render the still shots of the reference scene with Movie Render Queue.

Run inside the Unreal Editor (it needs the editor to stay open while it
renders):
  UnrealEditor GranTheftPraline.uproject \
      -ExecutePythonScript="<repo>/tools/rendering/reference_scene/render_stills.py"

Queues one job per lighting state and per MRQ_Still_<n> config (3 frames
of the camera path), renders them in-editor (PIE executor), writes PNGs to
docs/rendering/test-results/stills/ and quits the editor when done.
"""

import unreal

ROOT = "/Game/ReferenceScene"
STATES = ("Noon", "GoldenHour", "Rain", "Night")
SHOTS = 3

subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
queue = subsystem.get_queue()
queue.delete_all_jobs()

for state in STATES:
    for n in range(1, SHOTS + 1):
        job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
        job.job_name = "%s_shot%d" % (state, n)
        job.map = unreal.SoftObjectPath("%s/Maps/ReferenceStreet_%s" % (ROOT, state))
        job.sequence = unreal.SoftObjectPath(ROOT + "/Sequences/LS_StreetPath")
        job.set_configuration(unreal.load_asset("%s/Movie/MRQ_Still_%d" % (ROOT, n)))

executor = unreal.MoviePipelinePIEExecutor()


def on_finished(_executor, success):
    unreal.log("[ReferenceScene] stills finished, success=%s" % success)
    unreal.SystemLibrary.quit_editor()


executor.on_executor_finished_delegate.add_callable_unique(on_finished)
subsystem.render_queue_with_executor_instance(executor)
