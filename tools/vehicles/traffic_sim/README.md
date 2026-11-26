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

`traffic_sim.cpp` names no lane when it asks for a junction, so it still lets one car at a time through
(the rule until gate 106).

Pass criteria: distance to the lane p95 < 1.5 m and p99 < 4 m (the worst
1 % is at junction corners, measured against the straight jump between two
lane ends), mean speed above 12 km/h, at most a quarter of the cars stuck
for 20 s, no contact at all. One car at a time crosses a junction (lights
and priorities come in 0.9). Cars reaching a dead end or stuck for 20 s are
taken away and put back on a free lane, as the game does out of sight.

## The traffic around a player who stands still

`standing_sim.cpp` puts 28 cars in the ring of 40 to 250 m around a player who stands on a lane, as the
game does, and lets them slide along the lanes under the game's rules (`GTPDriveLogic.h`,
`GTPTrafficLights.h`, `GTPTrafficHeld.h`, `GTPTrafficGhost.h`). With `his-car` the car he left shuts the
lane he stands on and the one of the other way beside it, with no room to go round. Every 10 s it prints
the counterpart of the game's "traffic cars" line, and at the end the means from t=90 s.

```sh
g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public \
    tools/vehicles/traffic_sim/standing_sim.cpp -o /tmp/standing_sim
/tmp/standing_sim grid                                   # no file: a grid of two-way streets, PASS or FAIL
/tmp/standing_sim lanes.csv 350 100 28 360 his-car            # the rules of now, where he stands (metres)
/tmp/standing_sim lanes.csv 350 100 28 360 his-car gate-106   # the rules until gate 106
```

`gate-106` is `one-at-a-time hold-junction no-keep-off`; each of the three can be given alone.
`junction_test.cpp` checks the rules themselves: who crosses a junction, when a car in a queue gives
it back, which lanes the traffic keeps off. See [vehicles.md](../../../docs/gameplay/vehicles.md), 8.10.
