# Missions on the interface

*French version: [missions-hud.fr.md](missions-hud.fr.md)*

Mission rules and data are the mission thread's (ADR-0014, `GTPMissionSubsystem.h`). The interface
draws what the subsystem exposes and holds no mission logic (`GTPHudMissions.cpp`). The file compiles
to nothing when `GTPMissionSubsystem.h` is absent and does nothing with `-Missions=0`.

- **Maps**: start markers as diamonds on the mini-map and the full map (yellow available, grey not),
  refreshed twice a second (`GetStartMarkers` allocates); the current waypoint in light blue (on the
  mini-map ring when out of range), and the **GPS route to it**, set when it changes and cleared with it
  (a destination the player chose is left alone until a waypoint replaces it).
- **Objective hint** at the top centre while a mission runs (key from the mission tables, text in
  `GTPStringTable.h`).
- **Start prompt** near an available start marker: "Start: <title>" and "E / D-pad right: start".
- **Result card**: success shows the reward ("Mission complete: +150 EUR"), Enter or A continues;
  failure shows the reason (`mission.fail.<reason>`), Enter or A retries, Backspace or B gives up.
- Texts: the rows of `Content/Data/Missions/strings.tsv` are in the string table (a test checks both
  languages); new mission texts go through the interface thread in one pass.

Money display (HUD) waits for the economy thread's API (a getter for the current amount).
