# AI driver simulation

Checks the driving decisions of non-player drivers (`GTPDriveLogic.h`, the
same header the game compiles) on the real lanes of the focus street, made by
`tools/city/traffic.py`, without opening Unreal. Cars follow a kinematic
bicycle model close to the city car tuning (2.6 m wheelbase, 35° lock,
2.6 m/s² acceleration, 8 m/s² braking).

```sh
python3 tools/city/traffic.py --zone presqu-ile --focus rue-de-la-republique
g++ -O2 -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public \
    tools/vehicles/traffic_sim/traffic_sim.cpp -o /tmp/traffic_sim
/tmp/traffic_sim assets/geo/processed/presqu-ile/gen/rue-de-la-republique/traffic/lanes.csv 25 300
```

Pass criteria: distance to the lane p95 < 1.5 m and p99 < 4 m (the worst
1 % is at junction corners, measured against the straight jump between two
lane ends), mean speed above 12 km/h, at most a quarter of the cars stuck
for 20 s, no contact at all. One car at a time crosses a junction (lights
and priorities come in 0.9). Cars reaching a dead end or stuck for 20 s are
taken away and put back on a free lane, as the game does out of sight.
