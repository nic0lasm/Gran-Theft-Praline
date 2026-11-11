# Night slot (compare) - cost of the night (20261111-055119)

Host macbook-pro-de-nico1; 5:51  up 14:10, 2 users, load averages: 2.80 1.95 2.32
Clone HEAD: c29f2fa048da19fe91e91c66f1c31bae975fc8b1
Map md5: 1c92de0f813b89cb40aeb000cd92a1de (expected 1c92de0f813b89cb40aeb000cd92a1de: OK)
Build: GTAL-build/Builds/perf-cumul/0.3

GPU = p50 / p95 in ms over the whole run (frame log of the game); fixed camera unless 'demo'.
Mean = mean luma 0-255 of the still. Budget = what the game applied (status file after 12 s).
Code of the tools: 5134859

## Runs

| Run | Arguments | GPU p50 | GPU p95 | Mean | Budget applied | Lights |
| --- | --- | --- | --- | --- | --- | --- |
| golden-full | `-Ambience=golden -NightClouds=full` | 12.9 | 26.6 | 65 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| golden-default | `-Ambience=golden` | 12.9 | 26.6 | 65 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 0/499 on, 0 shadowed |
| dusk-full | `-Ambience=dusk -NightClouds=full` | 14.6 | 28.5 | 31 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 39/499 on, 4 shadowed |
| dusk-default | `-Ambience=dusk` | 12.9 | 21.5 | 31 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 39/499 on, 4 shadowed |
| late-dusk-full | `-Date=2026-10-15 -TimeOfDay=19.6 -TimeScale=0 -Weather=Cloudy -NightClouds=full` | 16.9 | 39.6 | 16 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 39/499 on, 4 shadowed |
| late-dusk-default | `-Date=2026-10-15 -TimeOfDay=19.6 -TimeScale=0 -Weather=Cloudy` | 13.6 | 27.9 | 16 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 39/499 on, 4 shadowed |
| night-full | `-Ambience=night -NightClouds=full` | 16.0 | 35.5 | 36 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 39/499 on, 4 shadowed |
| night-default | `-Ambience=night` | 13.3 | 25.5 | 36 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 39/499 on, 4 shadowed |
| night-demo-full | `-Ambience=night -DemoRoute -NightClouds=full` | 19.4 | 38.1 | 27 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 43/499 on, 4 shadowed |
| night-demo-default | `-Ambience=night -DemoRoute` | 17.0 | 26.7 | 27 | 64 lights, 4 shadowed, 9000 cm, 4 lit cars | lights 43/499 on, 4 shadowed |

## Cost against noon (fixed camera), budget: night <= 3 ms GPU over the day

| Run | GPU p50 - noon | GPU p95 - noon | Within 3 ms (p50) |
| --- | --- | --- | --- |
| golden-full | nan | nan | ? |
| golden-default | nan | nan | ? |
| dusk-full | nan | nan | ? |
| dusk-default | nan | nan | ? |
| late-dusk-full | nan | nan | ? |
| late-dusk-default | nan | nan | ? |
| night-full | nan | nan | ? |
| night-default | nan | nan | ? |

## Clone after the pass
- Map md5 before 1c92de0f813b89cb40aeb000cd92a1de, after 1c92de0f813b89cb40aeb000cd92a1de: unchanged
- HEAD before c29f2fa048da19fe91e91c66f1c31bae975fc8b1, after c29f2fa048da19fe91e91c66f1c31bae975fc8b1: unchanged
- Tracked files: unchanged
- Duration: 10 min
