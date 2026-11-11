# Test drives

*Français : [test-drives.fr.md](test-drives.fr.md)*

A test drive is a named route that the game replays by itself, so that a measurement, a video
or a gameplay check drives the same streets at the same speed on every build.

## Pieces

| Piece | Role |
|---|---|
| `tools/test/scenarios.json` | named drives: waypoints in Unreal metres (x, y) |
| `tools/test/drive_route.py` | snaps the waypoints to the driving lanes of the map, links them along the lanes (one-way streets, no U-turn), adds bend, acceleration and braking speeds, writes a timed path `t_s,x_cm,y_cm,speed_kmh` |
| `GranTheftPraline/Content/Test/Drives/<name>.csv` | the timed paths, packaged with the game |
| `UGTPTestDrive` (`-TestDrive=<name>`) | places the car at the first point, puts the player in it, then follows the path |
| `tools/test/run-drive.sh` | runs one drive in a packaged build, checks it reached its end |

## Modes

- `-TestDriveMode=rail` (default): the car is carried along the path by game time with physics
  off. Same camera path on every run, whatever the frame rate, the traffic or the build: use it
  for every A/B measurement.
- `-TestDriveMode=physics`: real pedals and steering (`GTPDriveLogic.h`, as the AI drivers),
  stops for what is ahead and backs off when stuck. Use it to test driving itself.

Other arguments: `-TestDriveLoop`, `-TestDriveQuit`, `-TestDriveDelay=<s>` (default 3). Add
`-TestVehicle` when the map has no parked car. Not combined with `-DemoRoute`.

## Everyday use

```sh
# check every drive against the lanes of the map (exit 2 on an unreachable waypoint)
python3 tools/test/drive_route.py --lanes GranTheftPraline/Content/City/Traffic/L_City_RueDeLaRepublique.lanes.csv --list
# add a drive: append it to scenarios.json, then write its path and a top view
python3 tools/test/drive_route.py --lanes <lanes.csv> --scenario <name> \
  --out GranTheftPraline/Content/Test/Drives/<name>.csv --svg /tmp/<name>.svg
# play it in a build (rail), with a video, or with real driving
tools/test/run-drive.sh Builds/0.3 hopital-bellecour
VIDEO=1 tools/test/run-drive.sh Builds/0.3 hopital-bellecour
tools/test/run-drive.sh Builds/0.3 grolee-cordeliers physics
# in a bench: the drive replaces -DemoRoute
EXTRA_ARGS="-TestVehicle -TestDrive=hopital-bellecour -TestDriveLoop" tools/perf/bench.sh <build>
```

The game logs `DemoPos t=... phase=testdrive` every 5 s of driving and `TestDrive end` at the
end; `tools/perf/compare_demo_positions.py` compares two runs. In rail mode two runs should match
within a few centimetres (to be confirmed on the first Mac run). The paths in Git were written from the lanes of `tools/city/traffic.py` (focus
rue-de-la-republique); if the lanes change, `--list` says which drive no longer fits.

## Shipped drives

| Name | Length | Time | Content |
|---|---|---|---|
| `grolee-cordeliers` | 172 m | 38 s | straight street, one junction |
| `gentil-bourse` | 166 m | 35 s | short, a right turn |
| `hopital-bellecour` | 576 m | 110 s | bends, four junctions, place Bellecour |
| `longest` | 737 m | 136 s | longest chain of lanes of the map (`--longest`) |

Limits: rail mode shows the car without wheel spin or engine load (the HUD shows the path
speed in the log, not on screen); physics mode depends on the traffic and is not bit-identical.
