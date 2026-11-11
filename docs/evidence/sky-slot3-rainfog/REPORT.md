# Slot 3 (rainfog) - light function cloud shadow (20261111-021826)

Host macbook-pro-de-nico1; 2:18  up 10:37, 2 users, load averages: 4.08 4.51 4.13
Clone HEAD: c278de890a912e37ca9dce3016168b908c34002b
Map md5: 1c92de0f813b89cb40aeb000cd92a1de (expected 1c92de0f813b89cb40aeb000cd92a1de: OK)
Build: GTAL-build/Builds/perf-cumul/0.3

Luminance = mean Rec. 601 luma 0-255 of the still at 20 s; ground = lower half;
patch = std of 16x6 block means of the lower half (cloud spots), compared with the volumetric reference.
Code of the tools: 8821188
Control: 1127 frames, light function loaded

## Clear (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | Spots % | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Clear-flat | no cloud shadow | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Clear -CloudShadows=0` | 113 | 124 | 11.6 | nan | 17.2 | 28.1 | off | +0 |
| Clear-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Clear` | 86 | 91 | 10.1 | 5.6 | 25.4 | 36.0 | off | +0 |
| Clear-m38 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.45,gtp.CloudShadowLF.Strength=0.2,gtp.CloudShadowLF.Mean=0.38` | 86 | 91 | 10.1 | 5.7 | 17.2 | 27.8 | loaded | +0 |
| Clear-c80s50m60 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.8,gtp.CloudShadowLF.Strength=0.5,gtp.CloudShadowLF.Mean=0.6` | 87 | 92 | 10.1 | 5.4 | 17.0 | 27.9 | loaded | +1 |

## Rain (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | Spots % | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Rain-flat | no cloud shadow | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Rain -CloudShadows=0` | 86 | 95 | 10.2 | nan | 16.9 | 28.0 | off | +0 |
| Rain-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Rain` | 58 | 62 | 10.1 | 9.1 | 26.1 | 36.1 | off | +0 |
| Rain-m70 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=0.7` | 56 | 60 | 10.3 | 10.3 | 17.3 | 28.1 | loaded | -2 |
| Rain-m80 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=0.8` | 58 | 63 | 10.1 | 8.8 | 16.9 | 28.2 | loaded | +0 |
| Rain-m90 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Mean=0.9` | 61 | 66 | 10.0 | 7.5 | 17.5 | 28.3 | loaded | +3 |

## Fog (13:30, 21 June)

| Run | Sky | Arguments | Mean | Ground | Patch | Spots % | GPU p50 | GPU p95 | Light function | Gap |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Fog-flat | no cloud shadow | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Fog -CloudShadows=0` | 113 | 125 | 11.6 | nan | 17.3 | 28.2 | off | +0 |
| Fog-vol | volumetric | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Fog` | 84 | 89 | 9.5 | 6.2 | 26.1 | 36.1 | off | +0 |
| Fog-m35 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.7,gtp.CloudShadowLF.Strength=0.35,gtp.CloudShadowLF.Mean=0.35` | 78 | 81 | 9.3 | 8.2 | 17.5 | 28.4 | loaded | -6 |
| Fog-m45 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.7,gtp.CloudShadowLF.Strength=0.35,gtp.CloudShadowLF.Mean=0.45` | 84 | 89 | 9.5 | 6.3 | 16.9 | 28.1 | loaded | +0 |
| Fog-m55 | lightfunction | `-GfxCVars=gtp.CloudShadowLF.Coverage=0.7,gtp.CloudShadowLF.Strength=0.35,gtp.CloudShadowLF.Mean=0.55` | 90 | 96 | 9.8 | 4.7 | 16.9 | 28.2 | loaded | +6 |

## Verdict per weather (closest mean; spots visible = spots at least 70 % of the volumetric reference, both against the flat still)

| Weather | Reference mean | Reference spots % | Best run | Gap | Spots % | Within ±3 | Spots visible |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Clear | 86 | 5.6 | Clear-m38 | +0 | 5.7 | yes | yes |
| Rain | 58 | 9.1 | Rain-m80 | +0 | 8.8 | yes | yes |
| Fog | 84 | 6.2 | Fog-m45 | +0 | 6.3 | yes | yes |

## Clone after the pass
- Map md5 before 1c92de0f813b89cb40aeb000cd92a1de, after 1c92de0f813b89cb40aeb000cd92a1de: unchanged
- HEAD before c278de890a912e37ca9dce3016168b908c34002b, after c278de890a912e37ca9dce3016168b908c34002b: unchanged
- Tracked files: unchanged
- Duration: 7 min
