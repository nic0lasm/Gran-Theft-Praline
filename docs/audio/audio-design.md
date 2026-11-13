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
| Cries of passers-by: hurt, fear, surprise | banks `Voices/<Scream|Hurt|Gasp>` (`make_voices_ui.py`) | `UGTPStreetReactions` (listens to `OnPedestrianHit` / `OnPedestrianScared`) |
| Police radio while wanted: alert for each star, end of pursuit, chatter | banks `Radio/<Chatter|Alert|Clear>` | same |
| Interface: menus, map, stars | banks `UI/<Name>` | `UGTPAudioMix::PlayUI(World, EGTPUISound::Confirm)` |

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
| open square, 30 m from every building and off the road (place Bellecour) | `SW_Amb_Square`: wind, far traffic all around, pigeons | 35 % |
| night (`CityLights` of MPC_Environment, ADR-0008) | `SW_Amb_Night`: muffled city, rare cars | × 30 % |
| rain (`RainIntensity`) | `SW_Amb_Rain`, or `SW_Amb_RainCar` (roof) in a car | — |
| visible pedestrians within 25 m | `SW_Amb_Crowd`, full at 10 pedestrians | — |

Every loop has the same level (RMS −24 dBFS); the weights in
`DefaultGame.ini` are the mix. Without the zone grid or the weather
parameters, only the street plays.

## Engines per model

The 0.3 engine synth gets a profile per car model (`EngineProfiles` in
`DefaultGame.ini`, chosen by a word of the class name): cylinders, rpm range,
resonance, diesel clatter, turbo whistle and hiss. The Chaos rpm range is
mapped onto the real engine's, and the load is cut during each gear change.

| Model | Engine | Character |
|---|---|---|
| Clio V | TCe 90, 1.0 3-cylinder turbo petrol | three-cylinder thrum, light whistle |
| 208 II | PureTech 100, 1.2 3-cylinder turbo petrol | rougher, lower at idle |
| Kangoo II | dCi 90, 1.5 4-cylinder turbo diesel | clatter at idle, revs lower (4,500 rpm) |
| others | 1.6 4-cylinder petrol | neutral |

## Street reactions

- A person hit by a car cries out (`Voices/Hurt`). A frightened person screams or gasps half the time, with at least 0.35 s between two screams, so a fleeing crowd does not become a wall of screams. Each voice has its own pitch (×0.82 to ×1.28).
- A medium or heavy crash broadcasts `UGTPAudioWorld::OnLoudSound(location, radius, cause)`, with a radius of 8 m (medium) or 20 m (heavy). Pedestrians bind `ScareAround` to it through a lambda.
- Police radio, 2D and without words: alert tones then a call for each star gained, a closing call when the pursuit ends. In between, chatter every 7 to 16 s, closer together with more stars. The HUD star sounds (`StarUp`, `StarDown`) play at the same time.

## Combat (0.7)

Spec: [combat.md](../gameplay/combat.md), "Sounds". Banks made by `make_weapons.py`
(`Weapons/`, `BulletImpacts/`, `Melee/`), played by `UGTPCombatAudio`. The audio world
listens to the combat system through `UGTPCombatSounds`, so the combat code plays nothing itself:
`OnShotFired` gives the shot and, from its hit, the bullet impact; `OnDamageDealt` with a melee
blow gives a light or heavy hit (a bullet in a body already sounded as a Flesh impact); the
player's `UGTPWeaponComponent::OnWeaponEvent` gives dry fire, magazine out at the start of a
reload and in at 55 % of it, and the slide when it ends. Only blocks and swishes of the fists
are called directly by the combat code. Nothing is bound under `-Combat=0`.

- **Shot:** within 25 m `ShotClose` (dry, 90 % of its energy in under 80 ms); farther
  `ShotFar` (crack, dull boom, rolling tail), late by the speed of sound (170 m: 0.5 s).
- **Street echo:** a `Tail` after the shot, delay and level by ambience zone: pedestrian
  street 60 ms and 0.6, street 90 ms and 0.5, quay 250 ms and 0.3, bridge 400 ms and 0.15.
- **Whizz:** a bullet line passing within 2.5 m of the listener, not for the shooter.
- **Ducking:** each shot lowers the ambience and its layers to 0.4 and the traffic to 0.6
  for 0.5 s (`SM_GTP_Gunshot`), then they come back in 0.35 s.
- **Panic:** `OnLoudSound` with a 30 m radius, so pedestrians run.
- **Bullet impacts:** Dust (stone, asphalt), Sparks (metal, sometimes a ricochet), Car,
  Glass (material name), Wood, Dirt (grass, gravel), Water, Flesh (`ImpactKindOf(Hit)`).
- **Melee:** Light, Heavy (15 damage and more), Block, Swish (dodge, missed blow).
- **Handling:** DryFire, MagOut, MagIn, Slide at the holder.
- **Budget:** `Weapons` class, 6 voices; impacts and blows share the 6 impact voices.
- **Per weapon** (`WeaponVoices`, key sent with each shot): the pistol (`Weapon.Pistol9`) and the
  carbine (`Weapon.Carbine`, 600 rounds a minute) each have a recorded close shot and dry fire
  (Lyra sample content, migrated on the PC, not in Git). When they do not load, the synthesised
  sounds play, pitched 0.86 and 15 % louder for the carbine. The far shot and the street tail stay
  synthesised. Automatic fire gets one tail and one panic broadcast per 0.3 s burst. The player's
  own shots never whizz, on foot or through the window of the car they drive.

## Interiors and missions (0.9)

`tools/audio/make_places.py` (seed 17, 32 files, no recording) writes `Interior/` (seamless 12 s
loops: RoomTone, Fryer, Grill, Hood, Cold, Shake, Drinks), `Shop/` (till open and close, coins,
notes, receipt printer, card beep, fryer timer, basket drop, order and door chimes, back-door
alarm buzzer, safe) and `Mission/` (available, start, objective, checkpoint, success, fail,
reward, countdown). `UGTPPlaceAudio`, owned by the audio world, plays them:

- when a streamed level whose name contains `Interior` becomes visible (ADR-0012), its
  components are scanned once. Loop emitters come from a tag `GTP_AudioLoop=<Loop>`, or else
  from a name with fryer, grill, hood, fridge/freezer/cold, shake or drink in it; the same loop
  within 1.5 m counts once. Rooms come from components tagged `GTP_AudioRoom`, or else the level
  bounds. Till points come from a tag `GTP_AudioTill`, or else till, register, kiosk or pos in a
  name. Actor tags count once per actor (its root component); `GTP_AudioLoop=None` marks a
  component that must not be guessed from its name (instanced groups);
- a pool of at most 6 looping voices (`MaxRoomVoices`) follows the nearest emitters within
  25 m. The pool is created once, so streaming an interior in and out never creates or
  destroys an audio object;
- inside a room, the indoor mix `SM_GTP_Indoor` lowers the street loop and the bed to 0.3, the
  traffic to 0.35 (sirens 0.7) and low-passes them at 1.5 kHz; the room tone fades in and the
  door chime plays on the way in;
- until the staff loops call `UGTPPlaceAudio::PlayShop`, a light shop life plays card beeps,
  receipts, order chimes, coins and fryer timers at the tills and fryers every 3 to 9 s
  (`-ShopLife=0` turns it off);
- `UGTPPlaceAudio::PlayMission` plays a sting in 2D through the Interface class. `UGTPMissionSounds`
  listens to `UGTPMissionSubsystem`: available on entering a start zone, start on the first
  objective of a run, then objective, success followed by reward when it pays, fail.

Settings: `[/Script/GranTheftPraline.GTPPlaceAudio]` in `DefaultGame.ini`. Cost: at most 6
looping voices plus one room tone, an estimated 0.06 % of a core per sample voice, under 0.5 % in all.

## Crowd budget (50 pedestrians, 25 cars)

Every 0.25 s `UGTPAudioWorld` ranks the running engines and the walkers by
distance to the listener. Only the 6 nearest engines within their silent
distance (45 m) are rendered; the others fade out and stop. Only the 8 nearest
visible walkers within 25 m look for steps; the crowd layer of the bed covers
the others. The player's car and the player always count first. Bed layers
that are silent are stopped instead of playing at zero. Far cars check their
tyres four times a second instead of twenty.

| Item | Without the budget | With it |
|---|---|---|
| Engine synths, audio thread (measured outside Unreal, 2.1 GHz x86 core) | 25 × 0.44 % = 11 % of a core | 6 × 0.44 % = 2.6 % |
| Walkers looking for steps, game thread | up to 50 | 8 |
| Bed loops decoded and mixed | 8 | the street and the layers in use (2 to 4) |
| Voices at most | ~70 | 42: steps 10, impacts 6, sirens 3, skids 4, voices 4, radio 1, interface 3, engines 6, bed 5 |

Estimated game-thread cost at 30 fps with that crowd: about 0.15 ms
(budget 0.3 ms), to be confirmed on the Mac with `-AudioStats`; the log line
gives `heard=` (walkers) and `engines=audible/total`. Settings
`MaxEngineVoices`, `MaxStepWalkers` and `BudgetInterval` in `DefaultGame.ini`.

## Footstep surfaces and ambience zones per map

`UGTPAudioMix::UseGridsForMap` picks the grids made for the map being played:
`L_City_Quarter` reads `Sound/SurfaceMap/quarter.gtpsurf` and `zones-quarter.gtpsurf`,
`L_City_RueDeLaRepublique` the `rue-de-la-republique` ones. A map without its own grids keeps
those of `UGTPAudioSettings`. `mac_audio_pass.sh prepare` makes them for every map of `FOCUSES`
(default `rue-de-la-republique quarter`), so the whole 0.9 quarter has its pedestrian streets,
quays of both rivers and bridges, and its surfaces under the feet.

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
| Our syntheses (`make_sfx.py`, `make_voices_ui.py`, `make_ambiences.py`, `make_street_ambience.py`) | MIT, original | working base of every bank | generated, out of Git |
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
- `-Audio=0`: nothing of ADR-0009 (A/B measurement). The car engine synths stay.
- `-SynthAudio=0`: no procedural wave at all (engines, sirens, skids). With `-Audio=0`, no game sound object is created or destroyed during play (crash hunt).
- `-AudioBed=0`: no ambience bed, whose layers stop and restart with the zone, the crowd, rain and night.
- `-RecordAudio[=s]` (`-RecordAudioDir=`): records the game output to a WAV, at the same time as `-CaptureVideo`; `-TestSiren`: the nearest traffic car sounds a siren (two-tone 12 s, yelp 4 s).
- `-DebugFeet` (or `gtp.Audio.DebugFeet 1`): foot heights of the player and every step with its surface (to calibrate `FootPlantHeight`).
- Outside the engine, in CI: `tools/audio/audio_logic_test.cpp` + `check_synths.py`
  (footstep rules, sirens, squeal), `tools/audio/tests` (banks, grid,
  recorded picks).
