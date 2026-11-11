# TSR / TAA A/B, day variants (2026-11-11, Shipping 88d47e3, no demo stills)

Fixed camera 45 s, demo route 95 s, frame times in ms, `-Sky=lightfunction`. Only the frame columns are valid: the per-pass GPU columns of this series came from a stale profile (the Shipping build writes no CSV profile) and are left out.

| Variant | cvars | Fixed p50 | Fixed p95 | Demo p50 | Demo p95 | Demo <= 33 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| warmup | | 13.4 | 25.9 | 19.5 | 29.1 | 98% | 0 |
| base | | 13.4 | 26.0 | 17.6 | 28.7 | 98% | 0 |
| tsr-hist75 | r.TSR.History.ScreenPercentage=75 | 13.4 | 26.2 | 17.4 | 28.6 | 99% | 0 |
| tsr-aa-medium | sg.AntiAliasingQuality=1 | 13.4 | 26.2 | 19.2 | 29.0 | 98% | 0 |
| tsr-aa-medium-hist75 | sg.AntiAliasingQuality=1, r.TSR.History.ScreenPercentage=75 | 13.5 | 26.2 | 19.0 | 28.8 | 98% | 0 |
| taa | r.AntiAliasingMethod=2 | 10.6 | 23.1 | 12.9 | 24.6 | 100% | 0 |

Stills, fixed camera 30 s in: `base-fixed.jpg`, `taa-fixed.jpg` (1728 x 1084), and the same centred 600 x 400 crop of both (`base-crop.jpg`, `taa-crop.jpg`) to compare sharpness.
