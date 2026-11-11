# TSR and clouds A/B (20261111-032938)

Build: <GTAL-build>/Builds/perf-cumul-test/0.3 (commit f1744ddd29dfeca9e0dc53c60e8d959b8b227f15); tools: c433164
Map md5: 1c92de0f813b89cb40aeb000cd92a1de; host macbook-pro-de-nico1; load averages: 3.27 2.98 3.53
Base arguments: -Sky=lightfunction ; fixed 45 s (still at 30 s), demo route 95 s; frame ms p50/p95, GPU per pass median ms

| Variant | Fixed p50 | Fixed p95 | Demo p50 | Demo p95 | Demo ≤33 ms | Hitches | TSR | Clouds | Lumen | Basepass |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| base | 17.7 | 30.4 | 21.3 | 31.9 | 97% | 0 | nan | 5.04 | 2.08 | 1.13 |
| tsr-hist75 | 14.3 | 26.6 | 19.8 | 29.7 | 98% | 0 | nan | 4.57 | 2.16 | 1.16 |
| tsr-rejaa1 | 17.4 | 30.0 | 16.1 | 28.3 | 100% | 0 | nan | 0.14 | 1.83 | 1.17 |
| tsr-noflicker | 16.6 | 29.4 | 20.3 | 31.3 | 97% | 0 | nan | 5.38 | 1.88 | 1.13 |
| tsr-aa-medium | 13.0 | 25.6 | 17.3 | 28.6 | 98% | 0 | nan | 5.08 | 2.20 | 1.13 |
| tsr-aa-medium-hist75 | 13.6 | 26.2 | 18.5 | 28.2 | 99% | 0 | nan | 4.52 | 2.05 | 1.15 |
| taa | 10.5 | 23.1 | 15.7 | 25.7 | 100% | 0 | nan | 4.03 | 1.87 | 1.15 |
| cloud-rays128 | 13.0 | 25.7 | 17.6 | 27.9 | 99% | 0 | nan | 4.35 | 2.18 | 1.19 |
| cloud-rays64 | 12.8 | 24.9 | 16.1 | 25.7 | 100% | 0 | nan | 3.27 | 1.90 | 1.16 |
| cloud-rtmode2 | 17.5 | 30.0 | 19.1 | 30.6 | 100% | 0 | nan | 8.06 | 1.96 | 1.13 |
| sky-cheap | 13.4 | 25.7 | 18.1 | 27.8 | 100% | 0 | nan | 4.90 | 1.87 | 1.17 |
| night | 15.7 | 35.8 | 20.0 | 38.6 | 91% | 0 | nan | 4.88 | 2.18 | 1.20 |
| night-sky-cheap | 15.7 | 35.4 | 18.4 | 37.4 | 92% | 0 | nan | 4.14 | 1.80 | 1.19 |
| night-noclouds | 11.5 | 12.2 | 13.2 | 14.8 | 100% | 0 | nan | nan | 1.79 | 1.16 |

GPU columns: demo route, sum of the median of the matching passes. Full pass tables: <variant>-demo/profile.csv with gpu_passes.py.
Duration: 40 min
