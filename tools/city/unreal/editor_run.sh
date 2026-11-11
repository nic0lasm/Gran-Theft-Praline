# Sourced by the city scripts (run_city_import.sh, regen_city.sh, profile_details.sh).
#
# run_editor <name> <editor command...>
#   Runs one Unreal launch with a time-stamped line before and after it, a progress line every
#   EDITOR_BEAT_S seconds (default 60) with the size of its own log, and a time limit
#   (EDITOR_MAX_S, default 900 s): past it the launch and its children (ShaderCompileWorker...)
#   are stopped and the function returns 124. The editor log goes to
#   GranTheftPraline/Saved/Logs/City_<name>.log (-log= takes a bare file name: no space to split).
#   Environment prefixes (GTP_PY_SCRIPT=... run_editor ...) reach the editor.
#   Exit hang (seen on the Mac 11/11: the editor printed "Shutdown handler: cleanup" and never
#   exited): with EDITOR_DONE_FILE (a report the script writes) or EDITOR_DONE_GREP (a line of its
#   log), once the work is done and the process is still there EDITOR_EXIT_GRACE_S later
#   (default 90 s), it is stopped and the launch counts as a success.
# stamp <text>: one time-stamped line, for the steps between launches.

stamp() { echo "== $(date +%H:%M:%S) $*"; }

_editor_kill_tree() {
  local p
  for p in $(pgrep -P "$1" 2> /dev/null); do _editor_kill_tree "$p"; done
  kill -TERM "$1" 2> /dev/null || true
}

_editor_work_done() {   # <editor log> <start epoch>
  if [ -n "${EDITOR_DONE_FILE:-}" ] && [ -f "$EDITOR_DONE_FILE" ]; then
    local m
    m=$(stat -c %Y "$EDITOR_DONE_FILE" 2> /dev/null || stat -f %m "$EDITOR_DONE_FILE")   # GNU, then BSD (macOS)
    [ "$m" -ge "$2" ] && return 0
  fi
  if [ -n "${EDITOR_DONE_GREP:-}" ] && [ -f "$1" ] && grep -q -- "$EDITOR_DONE_GREP" "$1"; then
    return 0
  fi
  return 1
}

run_editor() {
  local name="$1"
  shift
  local logs="${EDITOR_LOG_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)/GranTheftPraline/Saved/Logs}"
  local log="$logs/City_$name.log" max="${EDITOR_MAX_S:-900}" beat="${EDITOR_BEAT_S:-60}"
  local t0 now next pid rc size done_at=0 grace="${EDITOR_EXIT_GRACE_S:-90}"
  mkdir -p "$logs"
  rm -f "$log"
  t0=$(date +%s)
  next=$((t0 + beat))
  stamp "start $name (limit ${max} s, editor log Saved/Logs/City_$name.log)"
  "$@" "-log=City_$name.log" &
  pid=$!
  while kill -0 "$pid" 2> /dev/null; do
    sleep 5
    now=$(date +%s)
    if [ $((now - t0)) -gt "$max" ]; then
      _editor_kill_tree "$pid"
      sleep 5
      stamp "TIMEOUT $name after $((now - t0)) s; last editor log lines:"
      tail -n 20 "$log" 2> /dev/null || true
      return 124
    fi
    if [ "$done_at" -eq 0 ] && _editor_work_done "$log" "$t0"; then
      done_at=$now
      stamp "$name: work done (${EDITOR_DONE_FILE:-${EDITOR_DONE_GREP:-}}), waiting up to ${grace} s for the editor to exit"
    fi
    if [ "$done_at" -gt 0 ] && [ $((now - done_at)) -gt "$grace" ]; then
      _editor_kill_tree "$pid"
      sleep 5
      stamp "end $name: work done but the editor did not exit in ${grace} s, stopped it (counted as success)"
      return 0
    fi
    if [ "$now" -ge "$next" ]; then
      size=0
      if [ -f "$log" ]; then size=$(wc -c < "$log" | tr -d ' '); fi
      stamp "running $name, $((now - t0)) s, editor log ${size:-0} bytes, last: $(tail -n 1 "$log" 2> /dev/null | cut -c1-160)"
      next=$((now + beat))
    fi
  done
  rc=0
  wait "$pid" || rc=$?
  stamp "end $name, exit $rc, $(( $(date +%s) - t0 )) s"
  return "$rc"
}
