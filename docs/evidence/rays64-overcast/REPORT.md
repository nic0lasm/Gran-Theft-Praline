# Night slot (rays64) - cost of the night (20261111-105252)

Host macbook-pro-de-nico1; 10:52  up 19:11, 2 users, load averages: 2.47 3.28 3.36
Clone HEAD: d27b6719e7d422a620d3871869ef125428c418d7
Map md5: 1c92de0f813b89cb40aeb000cd92a1de (expected 1c92de0f813b89cb40aeb000cd92a1de: OK)
Build: GTAL-build/Builds/perf-cumul/0.3

GPU = p50 / p95 in ms over the whole run (frame log of the game); fixed camera unless 'demo'.
Mean = mean luma 0-255 of the still. Budget = what the game applied (status file after 12 s).
Code of the tools: 0501a87

## Runs

| Run | Arguments | GPU p50 | GPU p95 | Mean | Budget applied | Lights |
| --- | --- | --- | --- | --- | --- | --- |
| Cloudy-base | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Cloudy` | 12.9 | 24.2 | 91 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| Cloudy-rays64 | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Cloudy -GfxCVars=r.VolumetricCloud.ViewRaySampleMaxCount=64` | 12.4 | 23.5 | 91 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| Overcast-base | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Overcast` | 13.2 | 24.3 | 71 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| Overcast-rays64 | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Overcast -GfxCVars=r.VolumetricCloud.ViewRaySampleMaxCount=64` | 12.8 | 23.9 | 71 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| Rain-base | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Rain` | 13.6 | 24.5 | 59 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| Rain-rays64 | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Rain -GfxCVars=r.VolumetricCloud.ViewRaySampleMaxCount=64` | 12.9 | 23.8 | 59 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| Overcast-demo-base | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Overcast -DemoRoute` | 18.3 | 27.0 | 71 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| Overcast-demo-rays64 | `-Date=2026-06-21 -TimeOfDay=13.5 -TimeScale=0 -Weather=Overcast -DemoRoute -GfxCVars=r.VolumetricCloud.ViewRaySampleMaxCount=64` | 15.3 | 24.7 | 52 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |

## Gain of the tested setting (default, or rays64) against the reference (full, or base)

| Pair | GPU p50 full -> default | GPU p95 full -> default |
| --- | --- | --- |
| Cloudy | 12.9 -> 12.4 (-0.5) | 24.2 -> 23.5 (-0.7) |
| Overcast | 13.2 -> 12.8 (-0.4) | 24.3 -> 23.9 (-0.4) |
| Rain | 13.6 -> 12.9 (-0.7) | 24.5 -> 23.8 (-0.7) |
| Overcast-demo | 18.3 -> 15.3 (-3.0) | 27.0 -> 24.7 (-2.3) |

## Cost against noon (fixed camera), budget: night <= 3 ms GPU over the day

| Run | GPU p50 - noon | GPU p95 - noon | Within 3 ms (p50) |
| --- | --- | --- | --- |
| Cloudy-base | nan | nan | ? |
| Cloudy-rays64 | nan | nan | ? |
| Overcast-base | nan | nan | ? |
| Overcast-rays64 | nan | nan | ? |
| Rain-base | nan | nan | ? |
| Rain-rays64 | nan | nan | ? |

## Clone after the pass
- Map md5 before 1c92de0f813b89cb40aeb000cd92a1de, after 1c92de0f813b89cb40aeb000cd92a1de: unchanged
- HEAD before d27b6719e7d422a620d3871869ef125428c418d7, after d27b6719e7d422a620d3871869ef125428c418d7: unchanged
- Tracked files: unchanged
- Duration: 8 min

## Notes on the evidence

- The 88d47e3 build reads -CaptureFps=0.1 as 1 frame/s: each demo folder had 80 frame_*.jpg (one per second, each capture costing a > 100 ms frame). Kept here: one every 10 s at the same instants in both folders (frame_00000 ... frame_00070).
- The two demo runs follow the same route only for the first ~30 s; from about 60 s the camera and the place differ (frame_00060, frame_00079: different street and heading). The demo GPU gain (-3.0 ms p50) therefore mixes the setting and a different path; the fixed-camera pairs (-0.4 to -0.7 ms) are the clean comparison.
