# Slot 3 - light function cloud shadow in every weather (20261111-013722)

Host macbook-pro-de-nico1; 1:37  up  9:56, 2 users, load averages: 4.82 4.29 4.26
Clone HEAD: 00c6b95f25cb83c37856e5108c3857bdae276ab6
Map md5: 1c92de0f813b89cb40aeb000cd92a1de (expected 1c92de0f813b89cb40aeb000cd92a1de: OK)
Build: GTAL-build/Builds/perf-cumul/0.3

Luminance = mean Rec. 601 luma 0-255 of the still at 20 s; ground = lower half;
patch = std of 16x6 block means of the lower half (cloud spots), compared with the volumetric reference.
Code of the tools: 3572bec
Control: 1127 frames, light function loaded

## Clear (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Clear-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Clear` | 86 | 91 | 10.1 | 26.1 | 36.3 | off | +0 |
| Clear-preset | lightfunction | `preset` | 86 | 91 | 10.1 | 16.7 | 28.3 | loaded | +0 |
| Clear-c60s50m55 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.6,gtp.CloudShadowLF.Strength=0.5,gtp.CloudShadowLF.Mean=0.55` | 96 | 103 | 10.5 | 17.3 | 28.2 | loaded | +10 |
| Clear-c60s50m65 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.6,gtp.CloudShadowLF.Strength=0.5,gtp.CloudShadowLF.Mean=0.65` | 102 | 110 | 10.9 | 17.0 | 28.0 | loaded | +16 |
| Clear-c80s50m60 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.8,gtp.CloudShadowLF.Strength=0.5,gtp.CloudShadowLF.Mean=0.6` | 87 | 92 | 10.1 | 16.6 | 27.8 | loaded | +1 |
| Clear-c80s50m70 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.8,gtp.CloudShadowLF.Strength=0.5,gtp.CloudShadowLF.Mean=0.7` | 92 | 98 | 10.3 | 17.2 | 27.8 | loaded | +6 |
| Clear-c60s70m70 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.6,gtp.CloudShadowLF.Strength=0.7,gtp.CloudShadowLF.Mean=0.7` | 104 | 112 | 11.0 | 17.2 | 28.0 | loaded | +18 |

## Cloudy (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Cloudy-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Cloudy` | 92 | 97 | 10.3 | 25.2 | 35.7 | off | +0 |
| Cloudy-preset | lightfunction | `preset` | 98 | 104 | 10.6 | 17.2 | 27.9 | loaded | +6 |
| Cloudy-s35m40 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Strength=0.35,gtp.CloudShadowLF.Mean=0.4` | 91 | 96 | 10.3 | 17.2 | 27.9 | loaded | -1 |
| Cloudy-s35m60 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Strength=0.35,gtp.CloudShadowLF.Mean=0.6` | 104 | 111 | 10.9 | 17.0 | 27.7 | loaded | +12 |
| Cloudy-s60m55 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Strength=0.6,gtp.CloudShadowLF.Mean=0.55` | 99 | 105 | 10.7 | 17.2 | 27.9 | loaded | +7 |
| Cloudy-s60m75 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Strength=0.6,gtp.CloudShadowLF.Mean=0.75` | 110 | 118 | 11.3 | 17.2 | 27.9 | loaded | +18 |

## Overcast (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Overcast-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Overcast` | 69 | 73 | 9.8 | 26.0 | 36.0 | off | +0 |
| Overcast-preset | lightfunction | `preset` | 75 | 81 | 9.8 | 16.9 | 28.1 | loaded | +6 |
| Overcast-m70 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=0.7` | 66 | 70 | 10.0 | 17.5 | 28.3 | loaded | -3 |
| Overcast-m85 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=0.85` | 71 | 76 | 9.8 | 16.9 | 28.0 | loaded | +2 |
| Overcast-m120 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=1.2` | 81 | 88 | 10.0 | 17.0 | 28.1 | loaded | +12 |

## Storm (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Storm-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Storm` | 53 | 58 | 10.3 | 25.4 | 36.3 | off | +0 |
| Storm-preset | lightfunction | `preset` | 59 | 64 | 10.0 | 17.5 | 28.2 | loaded | +6 |
| Storm-m70 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=0.7` | 51 | 55 | 10.5 | 17.0 | 28.3 | loaded | -2 |
| Storm-m85 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=0.85` | 55 | 60 | 10.2 | 16.8 | 28.0 | loaded | +2 |
| Storm-m120 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=1.2` | 64 | 71 | 9.8 | 16.9 | 28.0 | loaded | +11 |

## Rain (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Rain-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Rain` | 58 | 62 | 10.1 | 26.1 | 36.0 | off | +0 |
| Rain-preset | lightfunction | `preset` | 64 | 70 | 9.9 | 16.9 | 28.2 | loaded | +6 |

## Fog (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Fog-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Fog` | 84 | 89 | 9.5 | 26.1 | 36.1 | off | +0 |
| Fog-preset | lightfunction | `preset` | 111 | 122 | 11.3 | 16.9 | 28.2 | loaded | +27 |
| Fog-m80 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=0.8` | 102 | 112 | 10.6 | 17.0 | 28.2 | loaded | +18 |

## Verdict per weather (closest mean; spots visible = patch at least 70 % of the reference)

| Weather | Reference mean | Reference patch | Best run | Gap | Patch | Within ±3 | Spots |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Clear | 86 | 10.1 | Clear-preset | +0 | 10.1 | yes | yes |
| Cloudy | 92 | 10.3 | Cloudy-s35m40 | -1 | 10.3 | yes | yes |
| Overcast | 69 | 9.8 | Overcast-m85 | +2 | 9.8 | yes | yes |
| Storm | 53 | 10.3 | Storm-m70 | -2 | 10.5 | yes | yes |
| Rain | 58 | 10.1 | Rain-preset | +6 | 9.9 | no | yes |
| Fog | 84 | 9.5 | Fog-m80 | +18 | 10.6 | no | yes |

## Clone after the pass
- Map md5 before 1c92de0f813b89cb40aeb000cd92a1de, after 1c92de0f813b89cb40aeb000cd92a1de: unchanged
- HEAD before 00c6b95f25cb83c37856e5108c3857bdae276ab6, after 00c6b95f25cb83c37856e5108c3857bdae276ab6: unchanged
- Tracked files: unchanged
- Duration: 13 min
