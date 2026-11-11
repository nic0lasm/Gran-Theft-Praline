# slot6d, near crowd on M1 (30edea5, Shipping aa020c2, street-demo 90 s, 2026-11-11)

Per-frame times from the game's frame log (frametimes.csv: frame_ms, game_ms, render_ms, gpu_ms). No Unreal CSV profile was recorded for these runs, so draw calls and primitives are not available.

| Case | Frames | Frame p50 | Frame p95 | GameThread p50 | GameThread p95 | RenderThread p50 | RenderThread p95 | GPU p50 | GPU p95 |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| n0 | 3372 | 24.3 | 34.0 | 0.87 | 0.97 | 1.54 | 1.79 | 24.5 | 33.9 |
| n20 | 3408 | 24.0 | 33.8 | 0.91 | 1.02 | 1.55 | 1.79 | 23.9 | 33.5 |
| n50 | 3375 | 24.3 | 33.8 | 0.97 | 1.09 | 1.61 | 1.85 | 24.1 | 33.7 |
| n100 | 3371 | 24.2 | 33.8 | 1.03 | 1.19 | 1.64 | 1.91 | 24.0 | 33.7 |
| n50-nocheap | 3354 | 24.4 | 34.0 | 0.97 | 1.10 | 1.62 | 1.89 | 24.2 | 33.8 |
| n50-noanimrate | 3440 | 23.9 | 33.7 | 1.01 | 1.14 | 1.63 | 1.88 | 23.8 | 33.4 |
| n100-nospread | 3413 | 24.0 | 33.7 | 1.08 | 1.20 | 1.62 | 1.86 | 23.9 | 33.5 |
| n50-far80 | 3375 | 24.3 | 33.9 | 0.97 | 1.09 | 1.61 | 1.86 | 24.4 | 33.6 |

Pedestrians present (game log): n0 off; n50 active=50 drawn 23-30; n100 active=100 drawn 50-74. GameThread n0 -> n100: +0.16 ms p50, +0.22 ms p95.
