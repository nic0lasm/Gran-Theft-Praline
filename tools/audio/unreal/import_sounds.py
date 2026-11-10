"""Import a folder of sound banks into Unreal (run headless in the editor).

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
        -script="tools/audio/unreal/import_sounds.py <folder> [/Game/Sound/SFX]"

<folder> mirrors the bank layout read by UGTPAudioMix (ADR-0009):
  Footsteps/<Surface>/*.wav, Land/<Surface>/*.wav,
  Impacts/<Kind>/<Light|Medium|Heavy>/*.wav, Impacts/Glass/*.wav
as written by tools/audio/make_sfx.py (generated) or
tools/audio/stage_recorded.py (recorded, Sonniss GDC or CC0). Every WAV becomes a
SoundWave at the same relative path under the root; an existing asset of the
same name is replaced, other assets in the folder are kept (delete a bank
folder in the editor to start it over). Sound classes are set at run time,
not on the assets. Content/Sound/ is not versioned.
"""

import os
import sys

import unreal

DEFAULT_ROOT = "/Game/Sound/SFX"


def main(argv):
    # Paths may hold spaces: the Mac pass script hands them over by environment.
    if not argv and os.environ.get("GTP_AUDIO_SRC"):
        argv = [os.environ["GTP_AUDIO_SRC"]] + ([os.environ["GTP_AUDIO_ROOT"]] if os.environ.get("GTP_AUDIO_ROOT") else [])
    if not argv:
        raise SystemExit("usage: import_sounds.py <folder> [/Game/Sound/SFX]")
    source = os.path.abspath(argv[0])
    root = argv[1] if len(argv) > 1 else DEFAULT_ROOT
    if not os.path.isdir(source):
        raise SystemExit("folder not found: " + source)

    tasks = []
    for folder, _dirs, files in os.walk(source):
        rel = os.path.relpath(folder, source).replace(os.sep, "/")
        dest = root if rel == "." else root + "/" + rel
        for name in sorted(files):
            if not name.lower().endswith(".wav"):
                continue
            task = unreal.AssetImportTask()
            task.filename = os.path.join(folder, name)
            task.destination_path = dest
            task.destination_name = os.path.splitext(name)[0]
            task.replace_existing = True
            task.automated = True
            task.save = True
            tasks.append(task)
    if not tasks:
        raise SystemExit("no .wav under " + source)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    failed = [t.filename for t in tasks if not t.imported_object_paths]
    for path in failed:
        unreal.log_error("[Audio] import failed: " + path)
    unreal.log_warning("[Audio] imported %d of %d sounds under %s" % (len(tasks) - len(failed), len(tasks), root))
    if failed:
        raise SystemExit(1)


main(sys.argv[1:])
