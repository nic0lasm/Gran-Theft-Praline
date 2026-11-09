# One-click macOS build

*French version: [one-click-build.fr.md](one-click-build.fr.md).*

Goal: double-click `Builds/<version>/GranTheftPraline.app` and play, with no
editor and no terminal (version 0.1 onward).

## Package

```sh
tools/build/package-mac.sh            # version from DefaultGame.ini (0.2.0 -> 0.2), Shipping
tools/build/package-mac.sh 0.2 Development
```

- Runs `RunUAT BuildCookRun` (build, cook, pak, IoStore, archive) for Mac.
- Moves the app to `Builds/<version>/GranTheftPraline.app`, signs it ad hoc
  (`codesign --sign -`) so it opens on this Mac, and writes
  `build-info.txt` (commit, duration, size) and `package.log`.
- `UE_ROOT` overrides the engine path (default
  `/Users/Shared/Epic Games/UE_5.8`). `Builds/` is ignored by Git.
- Watchdog: the cook can hang at the very end when the local Zen server
  stops. BuildCookRun is stopped when its log has not grown for
  `STALL_MINUTES` (default 25) or after `MAX_MINUTES` (default 240), then
  retried `RETRIES` times (default 1); the cook is incremental, so a retry
  resumes most of the work.
- App identity: bundle identifier `io.github.nic0lasm.GranTheftPraline`
  (`[/Script/MacTargetPlatform.XcodeProjectSettings]` in `DefaultEngine.ini`)
  and version from `ProjectVersion`. The script writes both into the packaged
  `Info.plist` and refuses to ship the template's `com.YourCompany`
  identifier.

## Test

```sh
tools/build/test-build-mac.sh          # current version, 10 minutes, 2-minute video
tools/build/test-build-mac.sh 0.2 120 0
GAME_ARGS="-GfxProfile=ultra" tools/build/test-build-mac.sh 0.2 300 0
```

Launches the packaged game straight into gameplay, records every frame and
checks that it quits cleanly after the requested time, with no crash report.
By default the video and soak runs play the demo route (`-TestVehicle
-DemoRoute`) so walking and driving are filmed and measured; `DEMO_ROUTE=0`
keeps the player still.
Evidence goes to `Builds/<version>/evidence/<timestamp>/`: `frametimes.csv`,
`frametimes.summary.txt` (p50, p95, p99, share of frames within 33.3 ms,
time to first frame), `title.jpg`, `capture.mov`, `result.txt`. The video
and the title still are filmed by the game itself, then assembled by
`tools/build/frames-to-movie.swift` (system frameworks only). The evidence kept for a
version is copied to `production/evidence/<version>/` (video through Git LFS).

## Game command-line switches

| Switch | Effect |
| ---- | ---- |
| `-SkipTitle` | Start in gameplay, skipping the title screen |
| `-FrameLog` | Record frame times to `Saved/Profiling/frametimes-<date>.csv` |
| `-FrameLogFile=<path>` | Record frame times to this file |
| `-FrameLogWarmup=<s>` | Ignore the first seconds (default 5) |
| `-ExitAfter=<s>` | Quit cleanly after this many seconds |
| `-CaptureVideo=<s>` | Save rendered frames as JPEG for this many seconds (`-CaptureFps`, default 10; `-CaptureDelay`, default 3; `-CaptureDir`) |
| `-GfxProfile=<name>` | Rendering profile from ADR-0002: `m1` (default), `fidelity` or `ultra` (project settings only) |
| `-DemoRoute` | Plays a fixed route by itself (look around, walk, get in the test car, drive, get out) to film and measure without a player; use with `-SkipTitle -TestVehicle` |

The recorder works in Shipping builds too: it uses the engine thread
timings (`frame`, `game`, `render`, `gpu`), not the profiler. The Metal GPU
timer sometimes returns absurd values (about 89 000 ms): those samples stay
in the CSV but are left out of the GPU statistics, and the summary counts
them. The summary also states the graphics profile, the viewport size and
the screen percentage.

## Graphics profiles

`UGTPGraphicsProfile` applies the profile at start, with the values of
`tools/rendering/reference_scene/profiles.json`, set in
`[/Script/GranTheftPraline.GTPGraphicsProfile]` of `DefaultGame.ini`.
`m1` (TSR, Lumen Lite, shadows Medium) keeps the measured internal
resolution, 50 % of 1080p, whatever the display: the screen percentage is
recomputed from the viewport height, so a Retina panel does not render three
times the pixels. Values are set with code priority, so saved user settings
cannot override what is measured.

## Code layout

`GranTheftPraline/Source/GranTheftPraline/`:

| Class | Role |
| ---- | ---- |
| `UGTPGameInstance` | Lives for the whole run; remembers that the title screen was dismissed |
| `AGTPGameMode` | Default game mode; playable pawn set by `PlayerPawnClass` in `DefaultGame.ini` |
| `AGTPPlayerController` | Title screen and pause menu (Escape, gamepad Menu) |
| `UGTPTitleMenuWidget`, `UGTPPauseMenuWidget` | Menus built in C++ with UMG, no widget asset |
| `UGTPFrameTimeRecorder` | Frame time CSV and soak test exit |
| `UGTPFrameCapture` | Films the game from the inside (no screen-recording permission) |
| `UGTPGraphicsProfile` | Applies the ADR-0002 rendering profile (`-GfxProfile`) |
| `UGTPDemoRoute` | Self-playing route for filmed and measured test runs (`-DemoRoute`) |

The playable character pawn class must also be cooked: list its folder in
`DirectoriesToAlwaysCook` or reference it from a cooked asset.
