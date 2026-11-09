"""Import the street ambience loop into Unreal (run headless in the editor).

    UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
        -script="tools/audio/import_ambience.py <path/to/SW_Amb_Street_Day.wav>"

Creates /Game/Sound/Ambience/SW_Amb_Street_Day (looping), read by
AGTPPlayerController (AmbienceSound in Config/DefaultGame.ini). The WAV comes
from make_street_ambience.py; the asset is regenerated, not versioned.
"""

import os
import sys

import unreal

DEST = "/Game/Sound/Ambience"
NAME = "SW_Amb_Street_Day"


def main(argv):
    source = argv[0] if argv else os.path.join(os.getcwd(), NAME + ".wav")
    if not os.path.isfile(source):
        raise SystemExit("WAV not found: " + source)
    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = DEST
    task.destination_name = NAME
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    path = DEST + "/" + NAME
    sound = unreal.load_asset(path)
    if not sound:
        raise SystemExit("import failed: " + path)
    sound.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log_warning("[Ambience] imported %s (%.1f s)" % (path, sound.get_editor_property("duration")))


main(sys.argv[1:])
