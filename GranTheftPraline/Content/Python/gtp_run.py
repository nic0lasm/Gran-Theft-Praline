"""Run an editor Python script whose path has spaces, without copying it anywhere.

Unreal splits the -ExecutePythonScript value and the "py" console command on spaces (a file
"with optional arguments"), and the project lives under "Claude work". Copying the scripts to
a temporary folder without spaces worked but wrote outside the project. Instead, this module
sits in Content/Python, which Unreal puts on sys.path for every editor and -game run, and the
command line names it by a call with no space at all:

    GTP_PY_SCRIPT="/path with spaces/tools/city/unreal/import_city.py" \\
      UnrealEditor Project.uproject -unattended "-ExecutePythonScript=__import__('gtp_run').main()"

    UnrealEditor Project.uproject Map -game ... -ExecCmds="r.SetRes 1920x1080w, py __import__('gtp_run').main()"

A command that does not end in .py is run as Python code, not as a file. The script runs as
__main__ with its own __file__; GTP_PY_ARGS (split like a shell line) becomes sys.argv[1:].
When GTP_PY_DONE names a file, it is written once the script returns (or raises).
"""

import os
import runpy
import shlex
import sys


def main(script=None):
    path = script or os.environ.get("GTP_PY_SCRIPT", "")
    if not path or not os.path.isfile(path):
        raise FileNotFoundError("gtp_run: GTP_PY_SCRIPT is not a file: %r" % path)
    sys.argv = [path] + shlex.split(os.environ.get("GTP_PY_ARGS", ""))
    folder = os.path.dirname(os.path.abspath(path))
    if folder not in sys.path:
        sys.path.insert(0, folder)   # like "python script.py": sibling modules import
    try:
        return runpy.run_path(path, run_name="__main__")
    finally:
        # GTP_PY_DONE: a file the caller watches (tools/city/unreal/editor_run.sh) to know the
        # script returned, since the editor can hang on exit afterwards.
        done = os.environ.get("GTP_PY_DONE")
        if done:
            with open(done, "w") as f:
                f.write(path + "\n")
