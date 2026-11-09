# Player character tools

Scripts that install and check the playable character, built on Epic's Game
Animation Sample (GASP). Plan and results:
[docs/gameplay/player-character.md](../../docs/gameplay/player-character.md).

| Script | What it does |
| ---- | ---- |
| `install_gasp_content.sh [sample]` | Lists the packages the character needs with a headless, read-only run in the sample project (`gasp_dependency_closure.py`), then copies them into `GranTheftPraline/Content` with the same `/Game` paths. The copied folders are Epic content and are ignored by Git. |
| `gasp_dependency_closure.py` | Asset registry walk (hard and soft references) from the sample character, camera and input; soft references to the sample's alternative characters are not followed. |
| `create_player_assets.py` | Headless, in our project: creates `BP_PlayerCharacter` (child of `SandboxCharacter_CMC`), `IA_ToggleCameraMode` and the `IMC_OnFoot_*` mapping contexts. Idempotent. |
| `playtest_character.sh [out]` | Plays `L_RueRepublique` in `-game` mode with `playtest_character.py`: run, walk, sprint, jump, crouch, first person toggle, respawn in first person. Writes `playtest.json`, 1920x1080 screenshots, frame times and the log to `.scratch/character/`. |

Order on a fresh clone: get the sample (Epic launcher, Fab, "Game Animation
Sample", project in `../Samples/GameAnimationSample`), run
`install_gasp_content.sh`, build the editor, run `playtest_character.sh`.
`create_player_assets.py` is only needed to rebuild our own assets.

Headless command for the Python scripts:

```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" GranTheftPraline/GranTheftPraline.uproject -run=pythonscript -script="$PWD/tools/character/create_player_assets.py" -unattended -nosplash -nullrhi
```
