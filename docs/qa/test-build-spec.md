# Test build specification (logs, crashes, export key)

*French version: [test-build-spec.fr.md](test-build-spec.fr.md).*

Status: design for 0.6 → 0.7, written without a machine; to be checked against
the engine by the pilot and the Windows thread. Nothing here changes engine
config: proposals for cvars or ini files go to the Optimisation thread.

## Why

A Shipping build writes no log and no profile CSV, and the Test
configuration did not compile on the launcher engine (Development is the
fallback). Testers need logs and crash reports to give useful feedback.

## Requirements

1. **Configuration.** Package a `Test`-type build that keeps logging. If
   `Test` still does not compile, package `Development`, or Shipping with
   `bUseLoggingInShipping = true` in `GranTheftPraline.Target.cs`. Decision
   after the first compile check on the PC.
2. **Log.** `Saved/Logs/GranTheftPraline.log` at every launch (previous logs
   rotated, last 5 kept), first line holds the version, the build time, the
   commit and the graphics profile.
3. **Crash reports kept locally.** Crash Report Client is configured to never
   upload (`bAgreeToCrashUpload=False`, no endpoint); the report folder is
   `Saved/Crashes`. The tester zips it; nothing leaves the machine.
4. **Export key (F9).** Writes `Saved/Exports/export-<timestamp>.txt` holding
   version, commit, profile, OS, GPU, player position and orientation, current
   map cell, day time, weather, and the last 60 s of frame times (p50, p95,
   worst, frames above 100 ms). Shown on screen for 3 s: "Exported to …".
5. **Version.** Shown on the title screen and in the pause menu, from
   `ProjectVersion` stamped by `tools/qa/stamp_version.py` (widget by the
   Interface thread).
6. **Smoke test.** `tools/qa/check_game_log.py` on the log after the scripted
   60 s run of `test-build-mac.sh` (and its Windows equivalent): no fatal, no
   error outside `tools/qa/known-log-noise.txt`, at least 50 lines.

## Split of work

| Part | Owner |
| ---- | ---- |
| Stamp, archives, bundle check, log check, notices check | QA (this folder `tools/qa/`) |
| F9 exporter, log header, version widget | Interface for the widget; a small C++ class owned by QA, compile-checked on the PC |
| Target and ini switches (`bUseLoggingInShipping`, crash client) | Optimisation applies, QA proposes |
| Windows equivalent of the smoke run | Windows PC thread |
