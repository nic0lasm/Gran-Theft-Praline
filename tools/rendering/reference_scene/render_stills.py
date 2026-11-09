"""Render the still shots of the reference scene with Movie Render Queue.

Run inside the Unreal Editor (it needs the editor to stay open while it
renders):
  UnrealEditor GranTheftPraline.uproject -unattended \
      -ExecCmds="py <path without spaces>/render_stills.py"

Do not use -ExecutePythonScript: in UE 5.8 it quits the editor as soon as the
script returns, before Movie Render Queue has rendered anything. The "py"
console command takes the rest of the line as the path, so copy the script to
a folder without spaces first (the output path does not depend on it).

Queues one job per lighting state and per MRQ_Still_<n> config (3 frames
of the camera path), renders them in-editor (PIE executor), writes PNGs to
docs/rendering/test-results/<date>/stills/ and quits the editor when done.

Options (after the script path in the "py" command):
  --profile <name>   apply the console variables of a profile from
                     profiles.json and add "_<name>" to the file names
                     (default: none, project settings only)
  --states Noon,Rain only these lighting states
  --shots 2          only these shots (comma-separated, 1 to 3)
Example: -ExecCmds="py /tmp/render_stills.py --profile res50 --states Noon --shots 2"
"""

import argparse
import datetime
import json
import os

import unreal

ROOT = "/Game/ReferenceScene"
STATES = ("Noon", "GoldenHour", "Rain", "Night")
SHOTS = 3
# Repository root = parent of the Unreal project folder (works with or without __file__).
REPO = os.path.abspath(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), ".."))
OUT = os.path.join(REPO, "docs", "rendering", "test-results", datetime.date.today().isoformat(), "stills")

TOOLS = os.path.join(REPO, "tools", "rendering", "reference_scene")


def profile_cvars(name):
    """Console variables of a profile from profiles.json (base profile first)."""
    with open(os.path.join(TOOLS, "profiles.json")) as f:
        profiles = json.load(f)
    profile = profiles[name]
    cvars = profile_cvars(profile["base"]) if "base" in profile else {}
    cvars.update(profile["cvars"])
    return cvars


parser = argparse.ArgumentParser(prog="render_stills.py")
parser.add_argument("--profile")
parser.add_argument("--states", default=",".join(STATES))
parser.add_argument("--shots", default=",".join(str(n) for n in range(1, SHOTS + 1)))
args = parser.parse_args()
cvars = profile_cvars(args.profile) if args.profile else {}
suffix = "_" + args.profile if args.profile else ""

os.makedirs(OUT, exist_ok=True)

subsystem = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
queue = subsystem.get_queue()
queue.delete_all_jobs()

for state in args.states.split(","):
    for n in (int(v) for v in args.shots.split(",")):
        job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
        job.job_name = "%s_shot%d" % (state, n)
        job.map = unreal.SoftObjectPath("%s/Maps/ReferenceStreet_%s" % (ROOT, state))
        job.sequence = unreal.SoftObjectPath(ROOT + "/Sequences/LS_StreetPath")
        job.set_configuration(unreal.load_asset("%s/Movie/MRQ_Still_%d" % (ROOT, n)))
        out = job.get_configuration().find_setting_by_class(unreal.MoviePipelineOutputSetting)
        out.set_editor_property("output_directory", unreal.DirectoryPath(OUT))
        # {sequence_name} and {level_name} resolve to empty strings in UE 5.8 PIE
        # renders, so name the files explicitly or they overwrite each other.
        out.set_editor_property("file_name_format", "%s_shot%d_frame{frame_number}%s" % (state, n, suffix))
        if cvars:
            cv = job.get_configuration().find_or_add_setting_by_class(unreal.MoviePipelineConsoleVariableSetting)
            for name, value in cvars.items():
                cv.add_or_update_console_variable(name, float(value))
            unreal.log("[ReferenceScene] %s: profile %s %s" % (job.job_name, args.profile, cvars))

executor = unreal.MoviePipelinePIEExecutor()


def on_finished(_executor, success):
    unreal.log("[ReferenceScene] stills finished, success=%s" % success)
    unreal.SystemLibrary.quit_editor()


executor.on_executor_finished_delegate.add_callable_unique(on_finished)
subsystem.render_queue_with_executor_instance(executor)
