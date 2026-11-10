# ADR-0007: Interface texts, settings and mini-map

*French version: [adr-0007-interface-settings-minimap.fr.md](adr-0007-interface-settings-minimap.fr.md)*

## Status

Proposed on 2026-11-10; applied by default until the project lead decides otherwise.

## Date

2026-11-10

## Decision Makers

nic0lasm (project lead)

## Summary

How the game holds its texts in French and English, saves the player's settings, and
draws a mini-map, with the least coupling to the systems being built in parallel
(HUD, traffic, police, pedestrians, rendering profiles).

## Decisions

1. **Texts: one compiled table**, `GTPStringTable.h` (key, French, English), read by
   `GTPLoc`. Not Unreal's localisation dashboard and `.po` files: for a few hundred
   short strings and two languages, a table in code is reviewable in a diff, needs no
   editor step on the Mac and is checked in CI by a plain compiler. The table has no
   Unreal types so it can move to data files later without touching callers. French is
   the reference text.
2. **Settings: a small text file** (`Saved/Config/GTPSettings.ini`, `key=value`), values
   and file format in `GTPSettingsCore.h`, loaded by `UGTPUserSettings`. Not
   `UGameUserSettings`: it carries engine scalability groups that would compete with the
   ADR-0002 profiles. Graphics settings **call** `UGTPGraphicsProfile::SetProfile` with a
   profile name; they never set console variables themselves, which stay owned by the
   optimisation work.
3. **Controls: AZERTY / QWERTY / automatic, sensitivity, vertical inversion** now; per-key
   remap later through Enhanced Input user settings.
4. **Mini-map: drawn on the canvas** from polylines generated from the city tables
   (`tools/ui/minimap.py`), in the zone's local coordinates shared with the traffic lanes.
   One call added to `AGTPHUD::DrawHUD`; everything else lives in its own subsystem.
   Other systems add blips through `AddBlip`.

## Consequences

- `UGTPGraphicsProfile` gains `SetProfile` and remembers the cvar values it replaces, so a
  switch back to Ultra restores them (a small change to a file the optimisation work owns,
  reported to them).
- Menu texts of the title and pause screens now come from the table.
- Prompts of other systems migrate to `GTPLoc` when those systems are touched.
- The mini-map data is derived from BD TOPO and stays out of Git, like the traffic lanes.

## Alternatives considered

- Unreal localisation (string tables assets, `.po`): heavier workflow, editor steps, no CI check.
- `UGameUserSettings`: conflicts with the profile mechanism.
- Render-target mini-map (a second scene capture): costs a full extra view on an M1 Pro
  already GPU-bound; the canvas lines cost a fraction of a millisecond. A pre-rendered
  texture is the fallback if the canvas cost does not fit its budget.

## Validation

`tools/ui/tests/run_cpp_tests.sh` (CI job "Interface core tests"); in-game evidence on
the Mac under `docs/ui/test-results/`.
