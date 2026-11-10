#!/usr/bin/env python3
"""Run one step with a time limit and a heartbeat (permanent rule for Mac slots).

    tools/common/guard.py --name blender-V-01 --max 240 -- <command ...>

* prints "[guard] <name> alive, <n> s" every --beat seconds (default 60) so a log is never silent;
* if the step exceeds --max seconds (the planned duration x 2), it is killed (whole process group)
  and the exit code is 124 with the cause on stderr: the caller stops and hands the Mac back.
"""
import argparse
import os
import signal
import subprocess
import sys
import time


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True)
    ap.add_argument("--max", type=int, required=True, help="seconds: 2x the planned duration")
    ap.add_argument("--beat", type=int, default=60)
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    cmd = a.cmd[1:] if a.cmd and a.cmd[0] == "--" else a.cmd
    if not cmd:
        sys.exit("no command")
    t0 = time.time()
    proc = subprocess.Popen(cmd, start_new_session=True)
    last = t0
    while True:
        try:
            rc = proc.wait(timeout=1)
            print(f"[guard] {a.name} done in {int(time.time() - t0)} s (exit {rc})", flush=True)
            sys.exit(rc)
        except subprocess.TimeoutExpired:
            pass
        now = time.time()
        if now - t0 > a.max:
            os.killpg(proc.pid, signal.SIGTERM)
            time.sleep(5)
            if proc.poll() is None:
                os.killpg(proc.pid, signal.SIGKILL)
            print(f"[guard] {a.name} KILLED after {int(now - t0)} s (limit {a.max} s): "
                  f"step too slow, stop and hand the Mac back", file=sys.stderr, flush=True)
            sys.exit(124)
        if now - last >= a.beat:
            last = now
            print(f"[guard] {a.name} alive, {int(now - t0)} s", flush=True)


main()
