# One-click macOS build

*French version: [one-click-build.fr.md](one-click-build.fr.md).*

Version 0.1 goal: double-click `Builds/0.1/GranTheftPraline.app` and play,
with no editor and no terminal.

## Package

```sh
tools/build/package-mac.sh            # version from DefaultGame.ini (0.1), Shipping
tools/build/package-mac.sh 0.1 Development
```

- Runs `RunUAT BuildCookRun` (build, cook, pak, IoStore, archive) for Mac.
- Moves the app to `Builds/<version>/GranTheftPraline.app`, signs it ad hoc
  (`codesign --sign -`) so it opens on this Mac, and writes
  `build-info.txt` (commit, duration, size) and `package.log`.
- `UE_ROOT` overrides the engine path (default
  `/Users/Shared/Epic Games/UE_5.8`). `Builds/` is ignored by Git.

## Test

```sh
tools/build/test-build-mac.sh          # 0.1, 10 minutes, 2-minute video
tools/build/test-build-mac.sh 0.1 120 0
```

Launches the packaged game straight into gameplay, records every frame and
checks that it quits cleanly after the requested time, with no crash report.
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

The recorder works in Shipping builds too: it uses the engine thread
timings (`frame`, `game`, `render`, `gpu`), not the profiler.

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

The playable character pawn class must also be cooked: list its folder in
`DirectoriesToAlwaysCook` or reference it from a cooked asset.
