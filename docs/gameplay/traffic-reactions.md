# Traffic reactions

The traffic drivers react to the street, on top of plain driving (lanes, lights, junctions, gaps, stopping for people ahead). Rules: `GTPTrafficReactLogic.h`, without engine types, checked by `tools/vehicles/react_test` in CI. Wiring: `UGTPAIDriverComponent::UpdateReactions` (`GTPTraffic.cpp`).

| State | When | What the driver does |
|---|---|---|
| Waiting | stopped behind something that does not move: the player, the player's car, a wreck, an abandoned car, a person standing in the road, a car that is itself waiting | first horn after 2.5 s (1 s for the player), then one every 3 s |
| Bypass | blocked 5 s and the left side clear for a lane width, from 5 m behind to 25 m past the obstacle | moves 2.9 m to the left at under half speed, back into the lane once the road ahead stayed clear 2 s (9 s at most) |
| Panic | a shot fired within 35 m, or a gun pointed at the moving driver (20 m, 10°) | horn, 1.6 times the lane speed for 8 s, red lights and junction turns ignored |
| Flee | a gun pointed at the stopped or slow driver for 1 s | stops, gets out (bails out above 40 km/h) and runs away from the gun; the car stays, free to take |
| Shaken | the player's car hits the car | one long horn, stands still 2.5 s, drives on |

A queue at a red light is not "still": the driver waits quietly. The horn is synthesised (`GTPHornSynth.h`, two tones near 410 and 512 Hz, a pitch of its own per car) and only sounds within 80 m of the camera. Ghost cars (far) do not react.

## Switches and settings

- `-TrafficReact=0`: no reactions at all (A/B for measurements).
- `-TrafficReactDemo=S`: every S seconds the nearest driven car 10 to 50 m away in view is, in turn, aimed at, rammed and shot near. The log says `traffic react demo: <event> on <car>`.
- `[/Script/GranTheftPraline.GTPTrafficNetwork]` in `DefaultGame.ini`: `ReactHonkAfter`, `ReactBypassAfter`, `ReactPanicSeconds`, `ReactAimRange`, `ReactAimHalfAngle`, `ReactShotRange`, `ReactHornDistance`.
- Every change of state is logged: `traffic react: <car> <from> -> <to> (ahead <actor> <m>, <km/h>)`.

## Demo scenes for the PC

1. Reactions in turn: `-TrafficReactDemo=12`, standing in a busy street, 120 s. Expected: `panic` or `flee` after `aim`, `shaken` after `ram`, `panic` after `shot`, and horns heard.
2. Blocked lane: `-TestDrive=hold-gentil -TestDriveMode=rail` holds the player's car still for 90 s at the start of the Gentil to Bourse drive. Expected, if traffic comes up behind it: `cruise -> waiting`, horns, then `waiting -> bypass` when the left side is free.
3. Cost: the same street-demo run with and without `-TrafficReact=0`. Expected: no measurable GPU change, game thread under 0.1 ms more (one side sweep only when a bypass is due).

## What is still missing compared with the target

Several car models (a single Clio today: assets, not code), lane changes on multi-lane roads, cars giving way to pedestrians at zebra crossings without lights, and a driver who gets back into a car the player left.
