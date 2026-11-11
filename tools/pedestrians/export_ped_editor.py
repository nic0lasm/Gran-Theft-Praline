"""Editor-mode wrapper of export_ped_source.py (ped-run.sh step 1).

The FBX export of a skeletal mesh bakes its materials through a skeletal mesh component, which needs a
render mesh: in a commandlet (no rendering) it hits the assertion "MeshObject" in
SkinnedMeshComponent.cpp and the engine crashes. So the export runs in the editor, on the empty
/Engine/Maps/Entry (never the project map), and quits when done:

    GTP_PED_OUT=<staging>/ped UnrealEditor-Cmd <uproject> /Engine/Maps/Entry \
        -ExecutePythonScript=tools/pedestrians/export_ped_editor.py -unattended -nosplash -stdout

quit_editor() ends the process with a non-zero code: ped-run.sh checks the exported files instead.
"""
import os
import runpy

import unreal

try:
    runpy.run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "export_ped_source.py"), run_name="__main__")
    unreal.log_warning("[ped_source] editor-mode export finished")
except BaseException as exc:  # noqa: BLE001 - always quit the editor, say why
    unreal.log_error(f"[ped_source] export failed: {exc}")
finally:
    unreal.SystemLibrary.quit_editor()
