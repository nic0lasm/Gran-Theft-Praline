# Missions and activities

*French version: [missions.fr.md](missions.fr.md)*

Decision and alternatives: [ADR-0014](../adr/adr-0014-missions-framework.md). No story, no
named character: an activity is a short job with a goal, a failure and a payment.

## How a mission runs

- **Start**: a marker (map, from 0.9) or a point in the street. Standing in the start zone shows
  the prompt "Start: <title>". Starting is refused while another mission runs, during the
  cooldown, once done (non-repeatable) or while a prerequisite is missing.
- **Objectives**: played in order. The HUD shows the hint text, the map shows the waypoint.
  Events that arrive before their objective begins are ignored.
- **Failure**: arrested, dead, a time limit, or a rule of the objective (car destroyed, car
  abandoned). A card offers Retry or Give up. Retry resumes at the last checkpoint or restarts
  (per mission); the clock of the attempt restarts; nothing was paid.
- **Success**: the money reward is paid once, the completion is counted, the game saves.
- **Save**: completion counts, cooldowns, running mission and its checkpoint. Loading resumes
  the running mission at its checkpoint.

## Data

`GranTheftPraline/Content/Data/Missions/`: `missions.csv`, `objectives.csv`, `zones.csv`,
`strings.tsv`. Objective types: `GoTo` (zone, `max_stars`), `EnterInterior` (interior id),
`WaitEvent` (event name, `min`), `EnterVehicle` (vehicle tag, `search_zone`), `DeliverVehicle`
(zone, `min_health`, `abandon_m`, `abandon_s`), `LoseWanted` (`hold_s`). Rules checked by CI
(`tools/missions/mission_test`): see [ADR-0014](../adr/adr-0014-missions-framework.md).

```
g++ -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/missions/mission_test/mission_test.cpp -o mission_test
./mission_test GranTheftPraline/Content/Data/Missions tools/missions/mission_test/fixtures
```

## What the framework reads and sends

| Read from | What | Used for |
|---|---|---|
| Police (`UGTPWantedComponent`) | stars, arrest, `player.arrested` | LoseWanted, GoTo `max_stars`, failure |
| Reference interior | current interior id, `holdup.cash_taken` (value = euros) | EnterInterior, WaitEvent |
| Traffic and vehicles | driven vehicle tag, health, distance to the mission car, `vehicle.destroyed` | EnterVehicle, DeliverVehicle |
| Player | position, `player.died` | zones, failure |

| Sent to | Command |
|---|---|
| Economy (shops thread) | `GiveReward(amount, runKey)`, paid once per run key |
| SaveGame (ADR-0013) | mission data, written when the save is dirty |
| Police | `EnsureStars` (flee activity), through `ReportCrime`, no change to the police code |
| Interface | objective hint key, waypoint zone, result, Retry or Give up card |
| Traffic / parked cars | `SpawnVehicle` (tag near a zone) if no matching car exists |

## Activity 1: rob the shop (`act_holdup_shop`)

- **Pitch**: walk into the fast-food restaurant on place Bellecour, have a cashier empty the till
  under threat, get away from the police.
- **Start**: in the street in front of the restaurant (zone `z_start_holdup`).
- **Objectives**: 1. enter `fastfood_bellecour`. 2. wait for `holdup.cash_taken` (any amount > 0;
  the loot itself goes to the player's money through the till, not through this mission).
  3. lose the police: stars at 0 for 2 s, within 180 s.
- **Failure**: arrested, dead, 180 s ran out. Retry restarts at the entrance. Cash already taken
  stays with the player or is lost by the economy rules (the police can recover it), not decided here.
- **Reward**: 150 € for a clean escape (on top of the loot). Repeatable, 60 s cooldown.
- **Fake danger**: the alarm (`holdup.alarm_raised`) is not an objective: the police thread's
  response to the crime is the pressure.
- **Depends on**: reference interior (till, hold-up script, `ReportCrime`), combat (threat).

## Activity 2: steal a car to order (`act_order_car`)

- **Pitch**: a client wants a blue sedan. Find it, steal it, deliver it to a fictional garage
  without wrecking it.
- **Start**: a street point (zone `z_start_order`); the search zone `z_order_search` (90 m) is
  marked on the map. A car tagged `order_sedan_blue` is taken from the parked cars there, or
  spawned if absent.
- **Objectives**: 1. be driving `order_sedan_blue` (the car is then bound to the mission).
  2. bring it to the garage zone `z_garage_fictif` with at least 50 % health.
- **Failure**: the bound car is destroyed; you leave it over 150 m away for 20 s; 600 s ran out;
  arrested or dead. Retry restarts at the search. Police stars from the theft are the normal
  consequence, not a failure.
- **Reward**: 400 €. Repeatable, 60 s cooldown.
- **Depends on**: parked cars and vehicle tags (traffic), wanted (theft crime), economy.

## Activity 3: lose the police (`act_lose_cops`)

- **Pitch**: the police have spotted you. Get away from them, then reach the hideout near the
  Terreaux without being seen.
- **Start**: a street point (zone `z_start_flee`); the framework raises the player to 2 stars at
  the start (`ensure_stars=2`) through the police API.
- **Objectives**: 1. stars back to 0 for 2 s. 2. reach `z_refuge_terreaux` with 0 stars.
  Being seen in the zone does not complete the objective.
- **Failure**: arrested, dead, 300 s ran out. Retry raises the stars again and starts over.
- **Reward**: 250 €. Repeatable, 60 s cooldown.
- **Depends on**: wanted (stars, search state), traffic (cars chasing).

## Scripted scenarios

`-MissionStart=<id> [-MissionStartDelay=<s>]` starts a mission by itself once the game has run that long,
so a scenario of the methods thread can combine it with `-TestDrive=<name> -TestVehicle -SkipTitle` and read
the log: `LogGTPMission: mission: succeeded <id>` or `failed <id>: <reason>`. Example, lose the police:
`-SkipTitle -MissionStart=act_lose_cops` (the framework raises 2 stars, the test drive or the tester gets away).

## Activities 4 and 5 (data only)

- **Time trial** (`act_time_trial`): start place Bellecour, three checkpoints along rue de la République in a
  car, at most one star at the last one, 150 s. 300 €.
- **Courier run** (`act_courier`): start rue Mercière, on foot to the garage spot, then to place des Terreaux,
  240 s. 120 €.

## Done when (0.9)

The three activities played from start to end, three times each, in a packaged build without
a blocker; failure and retry work; the reward is paid and saved; start from the map or from a
point in the street; merge gate held. Cost with no active mission: 0 ms.

## Unreal adapter (`UGTPMissionSubsystem`)

World subsystem, off with `-Missions=0` (nothing is created). It reads the player pawn, the wanted
component (stars, arrest banner) and the driven `AGTPVehicle` (actor tag, health, wreck), feeds the
runner and executes its commands. Tables are read from `Content/Data/Missions/`; the interim save is
`Saved/SaveGames/GTPMissions.txt` (written beside, then moved over) until ADR-0013 carries it.
Tests without the interface: `-MissionAutoStart=1` starts a mission on entering its zone;
console `GTPMission.Start <id>`, `.Retry`, `.Abort`, `.TagVehicle <tag>` (tags the car you drive).
Delegates for the other threads: `OnStartAvailable`, `OnObjective`, `OnFailed`, `OnSucceeded`,
`OnReward(amount, runKey, mission)`; getters `GetWaypoint` and `GetStartMarkers` for the map.
Wired to the real APIs of candidate 9: `OnArrested` and `EnsureMinStars` (police), `OnVehicleWrecked` and
`SpawnParkedVehicle` (traffic), `OnPlayerDied` (combat). Still missing: the interior id accessor (reference
interior, step 7), so the hold-up cannot start its first objective yet; the economy `AddMoney` (the reward
is broadcast on `OnReward`). Zones are real
vertices of named roads of the city data (rue de la République, rue Confort, rue Mercière,
place des Terreaux).

## Next steps

Unreal adapter `UGTPMissionSubsystem` (needs the police, economy and interior APIs on `next`),
start prompt and waypoint with the interface thread, `-Missions=0` switch, a text card for
Retry/Give up, then the machine slot for the A/B measure.
