# Police chase demo, 2026-11-13

Five frames from the scripted chase (`-PoliceChaseDemo -TestVehicle -Traffic=6 -TrafficLights=0`), Windows, 1280x720.
Car badges, number plates, signs and logos are blurred.

Source run: chase code of `feat/police-hunt-2` fc71466 (same code as `feat/police-hunt-3` f48c06d, replayed on `next` b28b614), editor `-game`, old city map.

| Frame | Video second | What it shows |
|---|---|---|
| `01-car-taken-2-stars.jpg` | 1 | The player drives the car taken on the quai Jean Moulin, 2 stars, 29 km/h |
| `02-3-stars-police-car-across.jpg` | 24 | 3 stars, a car across the road ahead, the player slows to 11 km/h |
| `03-flight-40kmh.jpg` | 52 | The player has lost the police (seen=0) and drives off at 40 km/h |
| `04-search-stars-greyed.jpg` | 70 | Search phase: stars greyed, the player at 48 km/h |
| `05-level-lost.jpg` | 96 | `wanted: level lost` at 94 s: no star left |

Log timeline of that run: 2 stars at 2 s, police in sight from 7 s, 3 stars at 22 s, held up then free at 37 s, police lost at 52 s, search phase at 72 s, level lost at 94 s; police units drove at 44 to 72 km/h.

Known limits: top speed 46 km/h (the planned 70 to 85 km/h is not reached), the player still hits obstacles three or four times.
The packaged build of f48c06d (Development) runs 120 s without a crash with pedestrians, traffic and police on; its chase run keeps 3 stars for the whole capture (no level lost).
