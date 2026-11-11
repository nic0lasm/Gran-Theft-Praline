# Streamed quarter, started 17:48 Paris time, commit 5e470d4
- cells: cell-p0000-m0250 cell-p0000-p0000 cell-p0000-p0250 cell-p0000-p0500 cell-p0000-m0500
- 17:50 bench on: exit 0 in 2 min (bench-on.log)
- **on**: 3163 frames, p50 25.7 ms, p95 37.0 ms, p99 38.8 ms, max 50.5 ms, 90.5 % <= 33.3 ms, 0 frames > 100 ms
    [2026.11.11-16.49.18:555][  0]CityStreaming: 29 cells, on, load 60 m, unload 90 m, max 4
    [2026.11.11-16.49.18:609][  0]CityStreaming: load L_City_CellP0000M0250 t=0.00
    [2026.11.11-16.49.18:898][  8]CityStreaming: load L_City_CellP0000M0500 t=0.28
    [2026.11.11-16.49.19:073][ 13]CityStreaming: visible L_City_CellP0000M0250 t=0.45
    [2026.11.11-16.49.19:468][ 23]CityStreaming: visible L_City_CellP0000M0500 t=0.86
    [2026.11.11-16.49.41:957][872]CityStreaming: unload L_City_CellP0000M0500 t=23.35
    [2026.11.11-16.49.54:763][346]CityStreaming: load L_City_CellP0000P0000 t=36.15
    [2026.11.11-16.49.55:053][355]CityStreaming: visible L_City_CellP0000P0000 t=36.44
    [2026.11.11-16.50.10:387][921]CityStreaming: unload L_City_CellP0000M0250 t=51.78
    [2026.11.11-16.50.20:079][286]CityStreaming: load L_City_CellP0000P0250 t=61.47
    [2026.11.11-16.50.20:351][295]CityStreaming: visible L_City_CellP0000P0250 t=61.74
- 17:52 bench off: exit 0 in 1 min (bench-off.log)
- **off**: 3173 frames, p50 25.5 ms, p95 36.7 ms, p99 39.5 ms, max 43.0 ms, 91.2 % <= 33.3 ms, 0 frames > 100 ms
    [2026.11.11-16.50.53:881][  0]CityStreaming: 29 cells, off (-CityCells=0), load 60 m, unload 90 m, max 4
- 17:52 done in 3 min
- 17:52 benches only: the project content was not touched
