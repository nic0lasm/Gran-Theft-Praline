# Settings and texts

*French version: [settings.fr.md](settings.fr.md)*

Roadmap: texts in French and English (0.4), saved settings (0.5). Decision record: [ADR-0006](../adr/adr-0006-interface-settings-minimap.md).

## What the player sees

The title screen and the pause menu have a **Settings** button. Each line of the
screen is `label : value`; a click (or Enter / gamepad A) moves to the next value,
applies it at once and saves it. Escape, gamepad B or **Back** returns to the menu
that opened the screen.

| Line | Values | Applied by |
| ---- | ------ | ---------- |
| Language | Français, English | `GTPSetLanguage`: every text read through `GTPLoc` |
| Graphics | Smooth (M1), Fidelity, Ultra | `UGTPGraphicsProfile::SetProfile`, the profiles of [ADR-0002](../adr/adr-0002-performance-budget.md) |
| Volume | 0 to 100 %, steps of 10 | `FApp::SetVolumeMultiplier` (all game audio) |
| Look sensitivity | 25 to 200 %, steps of 25 | `AGTPPlayerController::AddYawInput / AddPitchInput` |
| Invert vertical look | on / off | `AddPitchInput` |
| Keyboard | Automatic, AZERTY, QWERTY | `UsesQwerty()`, mapping contexts re-applied at once |
| Mini-map | on / off | [mini-map](minimap.md) |
| Mini-map zoom | near 120 m, medium 220 m, far 400 m | [mini-map](minimap.md) |

The file is `Saved/Config/GTPSettings.ini` (one `key=value` per line, `#` comments);
a damaged or hand-edited file never breaks the game (values are clamped, bad lines
ignored). `-GfxProfile=` and `-Qwerty` on the command line still win at start-up.

Not in this first version, on purpose: remapping individual keys (the AZERTY /
QWERTY pair covers the two layouts of the player base; a full remap goes through
Enhanced Input user settings and comes with the controls pass), resolution and
window mode, subtitles. They are listed in the roadmap.

## Texts

All menu texts live in one table, `Source/GranTheftPraline/Public/GTPStringTable.h`:
a row per text with a dotted key, the French text and the English text. In code:

```cpp
GTPLoc("menu.play");                                          // "Jouer" / "Play"
GTPLocFormat("settings.volume", {FText::AsNumber(40)});       // "Volume : 40 %" / "Volume: 40%"
```

An unknown key is shown as itself, so a typo is visible. Adding a text is one row
in the table; adding a language is one column. `tools/ui/tests/run_cpp_tests.sh`
checks that every key has both texts, that `{0}`-style placeholders match in both
languages, and that no key is duplicated; it runs in CI.

Prompts of other systems ("E", "F Monter", police, pedestrians) still use their
own strings; `prompt.enter_vehicle` and `prompt.exit_vehicle` are already in the
table so they can switch to `GTPLoc` when those systems are next touched.

## Tests

- `tools/ui/tests/run_cpp_tests.sh`: string table, settings round trip and damaged
  files, mini-map maths (no Unreal needed, compiler only).
- In the game (Mac, Shipping build): open Settings, change each line, quit, start
  again, check the values are back; see the evidence in
  `docs/ui/test-results/` once recorded.
