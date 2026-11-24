# The circuit of the filmed flight

[Français](flight-circuit.fr.md)

The automatic run (`-GTARun`, `-PoliceChaseDemo`) steals a car on the quai Jean Moulin and flees. Until gate 78 the flight was free on the lanes of the map: at each junction the exit with the best way on. Any wall, post or pit of the whole centre could hold it up, and the script put the car back on a lane 4 to 8 times in four filmed minutes. From now on the flight drives one circuit, chosen outside the game and kept in Git. The police pursue freely.

## The file

`GranTheftPraline/Content/Test/Circuits/<Map>.circuit.csv`, one row a lane: where the lane starts and ends (Unreal cm), its name, and the line driven on it. The lane's number in the lanes file is only a hint: the game finds each row's lane by its two ends, within 3 m, so the circuit survives a lanes file made again with other numbers. The rows under `approach` are driven once, from the theft to the lap; the rows under `lap` are driven again and again.

The game checks the file when the map starts (`GTPFlightCircuit.h`): each lane leads to the next, the last of the lap to the first, the last of the approach to a lane of the lap. A file that does not fit the lanes read is not used, the flight is free as before, and the log says why (`traffic: flight circuit NOT USED: ...`).

## The lap of L_City_RueDeLaRepublique

The lanes of this map are only made within 60 m of a generated building. The largest set of lanes from which one can come back to the theft is 33 lanes round the place des Cordeliers; its closed ways are 214 to 817 m long. The lap kept is the longest: 817 m, 21 lanes, 75 % on main axes (carriageway of 7 m and more), no narrow street. North on the quai Jean Moulin, round at the rue Gentil, south on the quays, the rue Saint-Bonaventure, north on the rue Grolée, east across the place des Cordeliers. Two laps are 1.6 km, three 2.4 km.

The approach is the middle lane of the quai Jean Moulin from the theft, 189 m straight on, into the lap at the turn of the rue Gentil. Without it the shortest way to the lap was a U-turn at the first junction.

What this lap is not: one loop of 1.5 to 2.5 km, and a way through the rue de la République. That street is pedestrian and has no lane, and the lanes round the place Bellecour are cut off from the quays in today's lanes file. A longer loop needs another lanes file.

## The driving line

The lanes are road centre lines pushed aside. Their points wander by a metre, and a lane starts a metre or two beside the end of the one before. The drivers read each such step as a bend: a step of 15 degrees is worth 42 km/h. On the lanes' own points, three laps driven with the game's decisions average 40 km/h and never pass 74, whatever speed is asked.

So each row carries the line driven on its lane: one closed curve over the lap, a point every 3 m, straights straightened, corners kept where they are and rounded (0.9 m cut inside a right-hand corner, whose inside is the kerb; 2.5 m inside a left-hand one). The line stays within 4.5 m of its lane. On the circuit the flight asks for 100 km/h on a quay or a boulevard (carriageway of 12 m and more), 75 on another main axis, 60 in a middle street, from 4 s after the theft.

What the simulation says (`flight_circuit_test.cpp`: the game's decisions, a bicycle model of a car, not the game's physics): on the line a lap takes 51 to 55 s, the first minute averages 52 to 56 km/h, the top speed is 100 km/h. As the film starts, from a standstill at the kerb: 49 to 53 km/h over the first minute. Only a gate on the PC says what the game's car does.

## When the car is held up

The script still puts the car back on a lane when it is held up or off its lane for good, but only on a lane of the lap, and each time counts in `DemoChase ... relocations=N`. A film with a relocation is not a valid one.

## Choosing and checking a circuit

`tools/city/flight_circuit.py`, plain Python:

- `find <lanes.csv>`: every closed way with no lane taken twice, the longest first, with its share of main axes;
- `write <lanes.csv> --lanes 214,7,... --approach 188,215,... --out <Map>.circuit.csv`: the circuit file with its driving lines;
- `check <circuit.csv> <lanes.csv> --report lane_clear_report.json --png lap.png`: the circuit against the lanes file the game reads, and against the report of the prep step `lanes`: nothing that step only reported (facade modules, signs) may stand on a lane of the circuit. Exit 1 when the circuit does not hold. To run before each gate, on the lanes file of that gate.

## Switch

`-FlightCircuit=0`: the flight is free on the lanes, as until gate 78.

## Also changed for every driver

The speed limit at the end of the known path counted the straight distance to that end. Where a path comes back beside itself (a lap, a quay driven up one side and down the other), the end lies a few metres from a car that still has 200 m to drive. The distance is now counted along the path when that is longer (`GTPDriveLogic.h`).
