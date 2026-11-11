# Car draw levers, 2026-11-11 13:53 CET

Scenario street-fixed, 75 s per case, `-FarTraffic=24 -Police=0`.

## d16-base: `-Traffic=16 `

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 12.2 | 24.7 | 12.4 | 24.7 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 16 driven (14 ghosts), 0 pooled, 24 far | drivers 0.067 ms/frame (25.7 us per driver tick), network 0.003 ms/frame, far 0.115 ms/frame
    LogGTPTraffic: traffic budget: 16 driven (15 ghosts), 0 pooled, 24 far | drivers 0.056 ms/frame (23.9 us per driver tick), network 0.002 ms/frame, far 0.090 ms/frame

## d16-cheap: `-Traffic=16 -CarDriverDraw=15 -CarLightDistance=40 -CarRenderStatic=30 -CarNearLOD=1`

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 12.5 | 24.5 | 12.6 | 24.7 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 16 driven (13 ghosts), 0 pooled, 24 far | drivers 0.081 ms/frame (29.4 us per driver tick), network 0.002 ms/frame, far 0.156 ms/frame
    LogGTPTraffic: traffic budget: 16 driven (12 ghosts), 0 pooled, 24 far | drivers 0.060 ms/frame (22.7 us per driver tick), network 0.006 ms/frame, far 0.092 ms/frame

## d20-base: `-Traffic=20 `

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 16.0 | 30.5 | 16.1 | 30.7 | 99 % | 0 |
    LogGTPTraffic: traffic budget: 20 driven (16 ghosts), 0 pooled, 24 far | drivers 0.086 ms/frame (19.7 us per driver tick), network 0.002 ms/frame, far 0.100 ms/frame
    LogGTPTraffic: traffic budget: 20 driven (16 ghosts), 0 pooled, 24 far | drivers 0.087 ms/frame (19.6 us per driver tick), network 0.002 ms/frame, far 0.105 ms/frame

## d20-cheap: `-Traffic=20 -CarDriverDraw=15 -CarLightDistance=40 -CarRenderStatic=30 -CarNearLOD=1`

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 12.1 | 24.7 | 12.2 | 24.9 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 20 driven (18 ghosts), 0 pooled, 24 far | drivers 0.109 ms/frame (25.3 us per driver tick), network 0.012 ms/frame, far 0.117 ms/frame
    LogGTPTraffic: traffic budget: 20 driven (18 ghosts), 0 pooled, 24 far | drivers 0.104 ms/frame (24.4 us per driver tick), network 0.002 ms/frame, far 0.116 ms/frame

## d24-base: `-Traffic=24 `

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 15.0 | 26.9 | 15.1 | 27.2 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 24 driven (22 ghosts), 0 pooled, 24 far | drivers 0.155 ms/frame (26.0 us per driver tick), network 0.009 ms/frame, far 0.138 ms/frame
    LogGTPTraffic: traffic budget: 24 driven (22 ghosts), 0 pooled, 24 far | drivers 0.135 ms/frame (21.5 us per driver tick), network 0.003 ms/frame, far 0.108 ms/frame

## d24-cheap: `-Traffic=24 -CarDriverDraw=15 -CarLightDistance=40 -CarRenderStatic=30 -CarNearLOD=1`

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 12.2 | 24.6 | 12.2 | 24.8 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 24 driven (19 ghosts), 0 pooled, 24 far | drivers 0.112 ms/frame (23.7 us per driver tick), network 0.002 ms/frame, far 0.080 ms/frame
    LogGTPTraffic: traffic budget: 24 driven (20 ghosts), 0 pooled, 24 far | drivers 0.080 ms/frame (17.0 us per driver tick), network 0.002 ms/frame, far 0.093 ms/frame

## d24-driver: `-Traffic=24 -CarDriverDraw=15`

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 14.0 | 26.7 | 14.1 | 26.8 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 24 driven (21 ghosts), 0 pooled, 24 far | drivers 0.117 ms/frame (21.5 us per driver tick), network 0.008 ms/frame, far 0.120 ms/frame
    LogGTPTraffic: traffic budget: 24 driven (22 ghosts), 0 pooled, 24 far | drivers 0.112 ms/frame (20.0 us per driver tick), network 0.002 ms/frame, far 0.116 ms/frame

## d24-lights: `-Traffic=24 -CarLightDistance=40`

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 13.1 | 26.4 | 13.2 | 26.5 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 24 driven (20 ghosts), 0 pooled, 24 far | drivers 0.112 ms/frame (22.9 us per driver tick), network 0.002 ms/frame, far 0.125 ms/frame
    LogGTPTraffic: traffic budget: 23 driven (19 ghosts), 1 pooled, 24 far | drivers 0.077 ms/frame (15.6 us per driver tick), network 0.002 ms/frame, far 0.088 ms/frame

## d24-static: `-Traffic=24 -CarRenderStatic=30`

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 12.3 | 24.9 | 12.4 | 25.1 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 24 driven (19 ghosts), 0 pooled, 24 far | drivers 0.111 ms/frame (22.4 us per driver tick), network 0.003 ms/frame, far 0.094 ms/frame
    LogGTPTraffic: traffic budget: 24 driven (19 ghosts), 0 pooled, 24 far | drivers 0.100 ms/frame (20.4 us per driver tick), network 0.002 ms/frame, far 0.060 ms/frame

## d24-near: `-Traffic=24 -CarNearLOD=1`

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 12.8 | 25.4 | 13.0 | 25.5 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 24 driven (19 ghosts), 0 pooled, 24 far | drivers 0.110 ms/frame (23.7 us per driver tick), network 0.002 ms/frame, far 0.123 ms/frame
    LogGTPTraffic: traffic budget: 24 driven (20 ghosts), 0 pooled, 24 far | drivers 0.095 ms/frame (20.5 us per driver tick), network 0.002 ms/frame, far 0.094 ms/frame

## d24-shadow: `-Traffic=24 -CarShadows=0`

exit status 0
| Scenario | Variant | GPU p50 | GPU p95 | Frame p50 | Frame p95 | % <= 33.3 ms | Hitches > 100 ms |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| street-fixed | base | 13.9 | 29.7 | 14.0 | 29.9 | 100 % | 0 |
    LogGTPTraffic: traffic budget: 24 driven (20 ghosts), 0 pooled, 24 far | drivers 0.110 ms/frame (20.8 us per driver tick), network 0.002 ms/frame, far 0.112 ms/frame
    LogGTPTraffic: traffic budget: 24 driven (21 ghosts), 0 pooled, 24 far | drivers 0.085 ms/frame (16.3 us per driver tick), network 0.002 ms/frame, far 0.070 ms/frame


Note: the bench carries about 2 ms of p50 variance between runs of the same build; each case here is a single run, so gaps under 2 ms are within noise.
Build: 23f322f Shipping, MacBook M1, 2026-11-11.
