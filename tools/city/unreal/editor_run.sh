# Sourced by the city scripts (run_city_import.sh, regen_city.sh, profile_details.sh).
#
# run_editor <name> <editor command...>
#   One Unreal launch, with:
#   - time-stamped lines before and after it, its pid and exact command line, and a progress
#     line every EDITOR_BEAT_S seconds (default 60) with the size and last line of its log;
#   - its log in GranTheftPraline/Saved/Logs/City_<name>.log through -abslog (a bare -log= name
#     lands in ~/Library/Logs when the editor runs from UnrealEditor.app; the Mac launcher quotes
#     an argument with a space, so the absolute path is safe);
#   - a done marker: the work counts as done when EDITOR_DONE_FILE (a report the script writes
#     last) is written after the start, or else when gtp_run.py writes $GTP_PY_DONE as the script
#     returns (City_<name>.done next to the log). Do not rely on the marker for -game runs, whose
#     scripts return at once and work on ticks: give them EDITOR_DONE_FILE;
#   - exit hang (Mac 11/11: the editor prints "Shutdown handler: cleanup", then never exits and
#     ignores TERM and ALRM): EDITOR_EXIT_GRACE_S (default 20 s) after the work is done, the
#     editor and its children get SIGKILL and the launch counts as a success;
#   - a time limit EDITOR_MAX_S (default 900 s; the scripts set about twice each launch): SIGKILL
#     and return 124;
#   - before the launch, a wait of up to EDITOR_WAIT_S (60 s) for any earlier editor or shader
#     worker to be gone, then SIGKILL for what is left;
#   - a launch whose log is still empty after EDITOR_START_S (90 s) is killed and started once more.
#   Environment prefixes (GTP_PY_SCRIPT=... run_editor ...) reach the editor.
# stamp <text>: one time-stamped line, for the steps between launches.

stamp() { echo "== $(date +%H:%M:%S) $*"; }

_editor_kill_tree() {   # SIGKILL: the editor ignores TERM once it hangs on exit
  local p
  for p in $(pgrep -P "$1" 2> /dev/null); do _editor_kill_tree "$p"; done
  kill -KILL "$1" 2> /dev/null || true
}

_mtime() { stat -c %Y "$1" 2> /dev/null || stat -f %m "$1"; }   # GNU, then BSD (macOS)

_editor_work_done() {   # <start epoch> <marker>
  local f="${EDITOR_DONE_FILE:-$2}"
  [ -f "$f" ] && [ "$(_mtime "$f")" -ge "$1" ]
}

_editor_others() {   # pids of Unreal editors still alive (not crash reporters, services or trace)
  ps -axo pid=,command= | grep -E "UnrealEditor( |$)|ShaderCompileWorker" \
    | grep -vE "CrashReportClient|UnrealEditorServices|UnrealTraceServer|grep -E" | awk '{print $1}'
}

_editor_wait_clear() {   # wait for the previous launch to be gone, then kill what is left
  local waited=0 pids
  pids="$(_editor_others)"
  while [ -n "$pids" ] && [ "$waited" -lt "${EDITOR_WAIT_S:-60}" ]; do
    sleep 5
    waited=$((waited + 5))
    pids="$(_editor_others)"
  done
  if [ -n "$pids" ]; then
    stamp "previous Unreal still alive after ${waited} s (pids $(echo $pids)): SIGKILL"
    kill -KILL $pids 2> /dev/null || true
    sleep 3
  elif [ "$waited" -gt 0 ]; then
    stamp "previous Unreal gone after ${waited} s"
  fi
}

run_editor() {
  local rc=0
  _run_editor_once "$@" || rc=$?
  if [ "$rc" -eq 125 ]; then
    stamp "retrying $1 once"
    rc=0
    _run_editor_once "$@" || rc=$?
    if [ "$rc" -eq 125 ]; then rc=124; fi
  fi
  return "$rc"
}

_run_editor_once() {
  local name="$1"
  shift
  local logs="${EDITOR_LOG_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)/GranTheftPraline/Saved/Logs}"
  local log="$logs/City_$name.log" marker="$logs/City_$name.done"
  local max="${EDITOR_MAX_S:-900}" beat="${EDITOR_BEAT_S:-60}" grace="${EDITOR_EXIT_GRACE_S:-20}"
  local t0 now next pid rc size done_at=0
  mkdir -p "$logs"
  rm -f "$log" "$marker"
  _editor_wait_clear
  t0=$(date +%s)
  next=$((t0 + beat))
  stamp "start $name (limit ${max} s, log Saved/Logs/City_$name.log)"
  GTP_PY_DONE="$marker" "$@" "-abslog=$log" &
  pid=$!
  stamp "$name pid $pid, command: $(printf '%q ' "$@" "-abslog=$log")"
  while kill -0 "$pid" 2> /dev/null; do
    sleep 5
    now=$(date +%s)
    if [ $((now - t0)) -gt "$max" ]; then
      _editor_kill_tree "$pid"
      sleep 3
      stamp "TIMEOUT $name after $((now - t0)) s; last log lines:"
      tail -n 20 "$log" 2> /dev/null || true
      return 124
    fi
    if [ "$done_at" -eq 0 ] && [ $((now - t0)) -gt "${EDITOR_START_S:-90}" ] && [ ! -s "$log" ]; then
      _editor_kill_tree "$pid"
      sleep 3
      stamp "$name wrote no log in ${EDITOR_START_S:-90} s: it did not start; Unreal processes: $(_editor_others | tr '\n' ' ')"
      return 125
    fi
    if [ "$done_at" -eq 0 ] && _editor_work_done "$t0" "$marker"; then
      done_at=$now
      stamp "$name: work done ($(basename "${EDITOR_DONE_FILE:-$marker}")), editor gets ${grace} s to exit"
    fi
    if [ "$done_at" -gt 0 ] && [ $((now - done_at)) -gt "$grace" ]; then
      _editor_kill_tree "$pid"
      sleep 3
      stamp "end $name: work done, editor did not exit in ${grace} s, SIGKILL (counted as success), $((now - t0)) s"
      return 0
    fi
    if [ "$now" -ge "$next" ]; then
      size=0
      if [ -f "$log" ]; then size=$(wc -c < "$log" | tr -d ' '); fi
      stamp "running $name, $((now - t0)) s, log ${size} bytes, last: $(tail -n 1 "$log" 2> /dev/null | cut -c1-160)"
      next=$((now + beat))
    fi
  done
  rc=0
  wait "$pid" || rc=$?
  stamp "end $name, exit $rc, $(( $(date +%s) - t0 )) s"
  return "$rc"
}
