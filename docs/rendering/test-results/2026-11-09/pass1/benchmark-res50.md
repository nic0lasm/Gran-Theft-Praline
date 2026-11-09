Profile: res50 (r.VSync 0, t.MaxFPS 0, r.ScreenPercentage 50, sg.ViewDistanceQuality 3, sg.ShadowQuality 2, sg.GlobalIlluminationQuality 2, sg.ReflectionQuality 2, sg.PostProcessQuality 3, sg.TextureQuality 2, sg.EffectsQuality 2, sg.FoliageQuality 2)

| State | Frames | Frame p50 (ms) | Frame p95 (ms) | GPU p50 (ms) | GPU p95 (ms) | Max frame (ms) | Hitches > 100 ms | Result |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Noon | 1770 | 30.2 | 39.5 | 29.6 | 38.7 | 71.6 | 0 | FAIL |
| GoldenHour | 1770 | 26.8 | 39.1 | 26.3 | 38.0 | 77.2 | 0 | FAIL |
| Rain | 1770 | 32.2 | 43.5 | 31.6 | 42.6 | 113.4 | 1 | FAIL |
| Night | 1770 | 26.8 | 45.1 | 26.0 | 44.1 | 73.2 | 0 | FAIL |

Budget: GPU p95 <= 33.3 ms (30 fps), no frame above 100 ms (ADR-0002).
