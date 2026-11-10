# Game audio

*Version française : [audio-design.fr.md](audio-design.fr.md). Decision: [ADR-0009](../adr/adr-0009-audio-mix.md).*

## What you hear in 0.3

| Sound | Source | Code |
|---|---|---|
| Street ambience (120 s loop) | offline synthesis, `tools/audio/make_street_ambience.py` | `AGTPPlayerController` |
| Car engines | real-time synthesis (rpm, load) | `UGTPEngineSoundComponent` (ADR-0003) |
| Footsteps of the player and pedestrians, per ground | banks `Footsteps/<Surface>` | `UGTPFootstepComponent` |
| Landing after a jump or a fall | banks `Land/<Surface>` | same |
| Car impacts (car, pole, wall, person, furniture) and glass | banks `Impacts/...` | `UGTPVehicleAudioComponent` |
| Tyre squeal | real-time synthesis (slide) | same |
| Police siren « deux tons » and yelp, with Doppler | real-time synthesis | `UGTPSirenComponent` |

| Ambience bed: pedestrian street, quai du Rhône, pont Lafayette, night, rain (street and car roof), crowd | loops `make_ambiences.py`, zone grid `zone_map.py` | `UGTPAmbienceBed` |

At the wheel, the ambience goes down (60 %) and loses its highs (6 kHz): you
are inside the car.

## Mix

Categories (sound classes created at start-up): Master > Effects >
Footsteps, Vehicles, Impacts, Sirens; Master > Ambience, Music, Voice,
Interface. Volumes, voice limits, attenuation distances, impact thresholds,
foot plant height: section `[/Script/GranTheftPraline.GTPAudioSettings]` of
`Config/DefaultGame.ini`. Far sounds lose their highs (distance low-pass), as
in a street.

## Ambience bed

The day street loop always plays; other loops fade in over it in 2 s from
where the listener is and what the world does:

| Condition | Layer | Street loop |
|---|---|---|
| "pedestrian" zone (rue de la République, mask G) | `SW_Amb_Pedestrian`: voices, steps, sparrows | 45 % |
| within 45 m of the Rhône | `SW_Amb_Quay`: river, wind, quay traffic, gulls | 35 % |
| above the water (pont Lafayette) | `SW_Amb_Bridge`: wind, close cars, expansion joints | 20 % |
| night (`CityLights` of MPC_Environment, ADR-0008) | `SW_Amb_Night`: muffled city, rare cars | × 30 % |
| rain (`RainIntensity`) | `SW_Amb_Rain`, or `SW_Amb_RainCar` (roof) in a car | — |
| visible pedestrians within 25 m | `SW_Amb_Crowd`, full at 10 pedestrians | — |

Every loop has the same level (RMS −24 dBFS); the weights in
`DefaultGame.ini` are the mix. Without the zone grid or the weather
parameters, only the street plays.

## Surfaces

Order of decision at the hit point: physical material (named in the project
settings or by its name, `PM_Gravel`), then keywords of the material
(`Wood`, `Metal`, `Steel`, `Cobble`, `Grass`, `Gravel`…), then, on the city
ground, the grid `Content/Sound/SurfaceMap/<street>.gtpsurf`, then stone.

The grid follows the ground mask (`tools/city/ground_mask.py`): carriageway =
asphalt; sidewalks, kerbs and pedestrian streets = stone. Rule: **the ground
sounds like it looks**. The red gravel of Bellecour, the grass of the squares
(`--vegetation`) or cobbles (`--areas`) are added once the ground material
shows them.

## Sound sources, free first

| Source | Licence | Planned use | Where |
|---|---|---|---|
| Our syntheses (`make_sfx.py`, `make_street_ambience.py`) | MIT, original | working base of every bank | generated, out of Git |
| Sonniss "GDC Game Audio Bundle" (≈ 7.5 GB) | royalty-free, commercial use allowed, no attribution, raw sounds may not be resold (re-read on download) | footsteps, car crashes, glass, tyres, city ambiences | Mac, `GTAL/assets/audio/sonniss/`, out of Git |
| CC0 packs (Kenney "Impact Sounds", OpenGameArt, Freesound filtered on CC0) | CC0 | occasional additions | Mac, out of Git |
| Field recordings in Lyon (0.16) | ours | per-street ambiences | later |

Every recorded sound that enters a release is listed with its licence in
`assets/SOURCES.md`. The repository holds no sound.

## Pipeline on the Mac

Unattended, in two steps of the grouped pass:

```
tools/audio/mac_audio_pass.sh prepare        # editor closed, before packaging
tools/audio/mac_audio_pass.sh check 0.4      # after packaging
```

`prepare` generates the banks and the grid, imports them headless and tells
whether GASP already plays footsteps (`.scratch/audio/gasp_foley.json`; if so,
put its folder in `MutedSoundFolders`). `check` makes a filmed run with sound
(`-DemoRoute -TestSiren -DebugFeet -RecordAudio`, then
`capture-with-sound.mov`), two A/B runs (`-Audio=0` and with audio), and
writes `audio-report.md` (step rate, suggested plant height, game thread cost
against the 0.3 ms budget).

Steps in detail:

```
python3 -I tools/audio/make_sfx.py .scratch/audio/sfx            # generated banks
python3 -I tools/audio/surface_map.py --ground <gen/rue-de-la-republique/ground>
python3 -I tools/audio/stage_recorded.py picks.csv .scratch/audio/sfx   # chosen recordings (optional)
UnrealEditor-Cmd GranTheftPraline.uproject -run=pythonscript \
    -script="tools/audio/unreal/import_sounds.py .scratch/audio/sfx"
```

`tools/audio/pick_sonniss.py <library> picks.csv` builds the list from the
folder and file names (footstep + surface, car + crash, glass + break…) and
cuts walk recordings into single steps; a report `picks.csv.txt` says what
each bank gets, to listen to before importing. `picks.csv` maps a recorded file to a bank
(`/path/Gravel_Walk_03.wav,Footsteps/Gravel`). To switch a whole bank to
recordings, delete its folder in the editor first.

## Measuring and tuning

- `-AudioStats`: one line `[Audio] steps=… impacts=… sirens_on=… skids_on=… walkers=…` per second.
- `-Audio=0`: nothing of ADR-0009 (A/B measurement).
- `-RecordAudio[=s]` (`-RecordAudioDir=`): records the game output to a WAV, at the same time as `-CaptureVideo`; `-TestSiren`: the nearest traffic car sounds a siren (two-tone 12 s, yelp 4 s).
- `-DebugFeet` (or `gtp.Audio.DebugFeet 1`): foot heights of the player and every step with its surface (to calibrate `FootPlantHeight`).
- Outside the engine, in CI: `tools/audio/audio_logic_test.cpp` + `check_synths.py`
  (footstep rules, sirens, squeal), `tools/audio/tests` (banks, grid,
  recorded picks).
