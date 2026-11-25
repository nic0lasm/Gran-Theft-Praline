# The circuit of the filmed flight

[Français](flight-circuit.fr.md)

The automatic run (`-GTARun`, `-PoliceChaseDemo`) steals a car on the quai Jean Moulin and flees. Until gate 78 the flight was free on the lanes of the map: at each junction the exit with the best way on. Any wall, post or pit of the whole centre could hold it up, and the script put the car back on a lane 4 to 8 times in four filmed minutes. From now on the flight drives one circuit, chosen outside the game and kept in Git. The police pursue freely: they choose their own way, and on a lane of the circuit a unit drives the circuit's line, as the flight does (at gate 78 the units averaged 14 to 21 km/h on the lanes' own points; `-PoliceCircuitLines=0` to compare).

## The file

`GranTheftPraline/Content/Test/Circuits/<Map>.circuit.csv`, one row a lane: where the lane starts and ends (Unreal cm), its name, and the line driven on it. The lane's number in the lanes file is only a hint: the game finds each row's lane by its two ends, within 3 m, so the circuit survives a lanes file made again with other numbers. The rows under `approach` are driven once, from the theft to the lap; the rows under `lap` are driven again and again.

The game checks the file when the map starts (`GTPFlightCircuit.h`): each lane leads to the next, the last of the lap to the first, the last of the approach to a lane of the lap. A file that does not fit the lanes read is not used, the flight is free as before, and the log says why (`traffic: flight circuit NOT USED: ...`).

## The lap of L_City_RueDeLaRepublique

The lap goes through the place Bellecour, the shop window of the town: 1793 m, 37 lanes. South on the quai Jules Courmont (346 m), the rue Childebert, the place de la République and its basin, the rue Marcel-Gabriel Rivière, the place de l'Hôpital, the rue Bellecordière, the rue de la Barre, the north-east corner of the place Bellecour, north on the rue du Président Édouard Herriot past the place des Jacobins, the rue Jean de Tournes, the rue Stella, the rue Grolée, the rue du Président Carnot, and right onto the quay again.

The theft is on the quai Jean Moulin, northbound, 400 m north of the lap. The approach (488 m) turns back through the junction of the pont Lafayette and runs down the quay into the rue Childebert.

Until gate 80 the lanes file could not hold this lap, for two reasons, both in `tools/city/traffic.py`:

- a road segment of 2.7 m of the rue Herriot at the place des Jacobins was dropped as too short, which cut the street. A whole road segment is now kept from 0.6 m;
- lanes were only made within 60 m of a generated building, which cut 610 m out of this lap (201 m of the quay north of the rue Childebert, 118 m of the rue Grolée, 96 m of the rue Herriot north of the Jacobins, 82 m of the rue Childebert, 81 m of the rue Stella, 32 m of the rue Jean de Tournes). The roads of the circuit are now listed in `tools/city/through_roads/<focus>.txt` and kept whole within the extent of the map. The lanes file goes from 229 to 255 lanes.

The lap chosen first was 817 m round the place des Cordeliers: the longest closed way of the lanes file as it was. It did not show Bellecour.

What this lap is not:

- a way through the rue de la République itself: that street is pedestrian and has no lane. The lap crosses it at the place de la République and runs beside it;
- a lap round the place Bellecour: the ground of the map ends 65 m into the square, only its north-east corner is there;
- 70 % of main axes by the lanes file, which says 41 %: it counts a one-way street by its one lane of 3 m. Between the kerbs measured on the ground (`carriageway.geojson`), 77 % of the lap is 7 m wide or more. `check --carriageway` counts that way.

## The driving line

The lanes are road centre lines pushed aside. Their points wander by a metre, and a lane starts a metre or two beside the end of the one before. The drivers read each such step as a bend: a step of 15 degrees is worth 42 km/h. On the lanes' own points, three laps driven with the game's decisions average 40 km/h and never pass 74, whatever speed is asked.

So each row carries the line driven on its lane: one closed curve over the lap, a point every 3 m, straights straightened, corners kept where they are and rounded (0.9 m cut inside a right-hand corner, whose inside is the kerb; 2.5 m inside a left-hand one). The line stays within 4.5 m of its lane, and starts and ends within 12 m of the lane's ends.

Each row also carries the speed the flight asks on it (column `kmh`), from 4 s after the theft: 100 km/h on the quays, 75 on the rue de la Barre, the rue Herriot and the rue Carnot, 60 on Bellecour and in the rue Childebert, Jean de Tournes, Stella and Grolée, 50 in the rue Rivière and the rue Bellecordière, 45 on the place de la République, 40 on the place de l'Hôpital. A row without a speed goes by the width the lanes file gives: 100 km/h on a carriageway of 12 m and more, 75 on another main axis, 60 in a middle street.

What the simulation says (`flight_circuit_test.cpp`: the game's decisions, a bicycle model of a car, not the game's physics): on the line a lap takes about two minutes, the first minute averages 52 to 53 km/h, the top speed is 100 km/h. As the film starts, from a standstill at the kerb and through the turn back: 50 to 52 km/h over the first minute, the place de la République at about 38 s, Bellecour at about 70 s, the place des Jacobins at about 85 s. Only a gate on the PC says what the game's car does.

## The pedals in the corners

Gate 83: the stolen car left its lane at three corners (quay to rue Childebert, rue Childebert to the place de la République, rue Herriot to rue Jean de Tournes) and was put back on a lane 6 times in one pass, 3 in the other. Its lines, one a second, show why. The lane follower drops the points of a lane the moment the car's closest point is on the next one, and a corner at the very start of a path is not counted as a bend: the speed wanted goes from 24 to 57 km/h in the middle of the corner, and the car gets the whole throttle at full lock (`lane=45 kmh=36 throttle=1.00 steer=1.00`, then `kmh=52`, then 27 m from its lane).

Since then the script's pedals follow its steering (`GTPFlightPedals.h`): with more than a third of lock asked, no throttle and some brake while the car is faster than that lock holds at 6 m/s² (15 km/h at full lock, 28 at a third). Under a third of lock nothing changes: on the quay the car asks 0.14 of lock at 98 km/h, and in the lines of gate 83 it never asks a third above 45 km/h. The steering is left as asked. `DemoDrive` lines show the pedals the car gets.

The cause itself is gone too, for the flight and for the police units, which drive with the same lane follower: a lane left keeps its last 12 m at the head of the driver's path (`GTPDrive::KeptBehind`), so the corner is still measured with the path before it. `lane_change_corner_test.cpp` drives the three corners three ways: with the lane dropped whole the speed wanted jumps, with its end kept the driver decides frame for frame as one whose path is never cut. The cars of the traffic have their own copy of that code and still drop the lane whole.

Not proven on the game's car at the time of writing. Both tests drive the corners on the circuit's own lines with the game's decisions, but the car of their model holds the corners either way: they show the decisions, not the 13 to 27 m by which the game's car left its lane.

## Where the film ends

In `-GTARun` the player gives up after the pursuit step: the car stops and the police come to it. By the clock alone that fell 100 to 200 m short of the place Bellecour. The circuit file marks the lanes the flight is to end on (column `mark`, value `end`): the last 65 m of the rue de la Barre and the 30 m of the place Bellecour, along the north side of the square. Once its 40 s are over, the pursuit step goes on until the car is on one of them, 20 s more at most; a car that crossed them before the 40 s were over stops where it is. The step says which: `ended after N s on the place the circuit ends on`, `past` it or `short of` it.

## When the car is held up

The script still puts the car back on a lane when it is held up or off its lane for good, but only on a lane of the lap, and each time counts in `DemoChase ... relocations=N`. A film with a relocation is not a valid one. It never puts it into another car, nor, since gate 83, onto anything else that stops a car (a bike rack, a low wall, a tree): the place is passed over, and the line of the relocation says how many were (`N place(s) passed over with something standing there`).

## Choosing and checking a circuit

`tools/city/flight_circuit.py`, plain Python:

- `find <lanes.csv>`: every closed way with no lane taken twice, the longest first, with its share of main axes;
- `write <lanes.csv> --lanes 381,367,... --approach 397,161,... --speeds "QU JULES COURMONT=100,..." --out <Map>.circuit.csv --roads-out tools/city/through_roads/<focus>.txt`: the circuit file with its driving lines and speeds, and the list of its roads for `traffic.py`;
- `check <circuit.csv> <lanes.csv> --carriageway carriageway.geojson --report lane_clear_report.json --png lap.png`: the circuit against the lanes file the game reads, and against the report of the prep step `lanes`: nothing that step only reported (facade modules, signs) may stand on a lane of the circuit. Exit 1 when the circuit does not hold. To run before each gate, on the lanes file of that gate.

When the roads of the circuit change: `write` on a lanes file that has them (made with `traffic.py` on a through-roads file that lists them), then `traffic.py` again, the prep step `lanes`, and `check`.

## Switch

`-FlightCircuit=0`: the flight is free on the lanes, as until gate 78.

`-DemoPedalsAlone`: the pedals no longer follow the steering, as until gate 83.

`-LaneKeptBehind=0`: a lane left is dropped whole, as until gate 83.

## Also changed for every driver

The speed limit at the end of the known path counted the straight distance to that end. Where a path comes back beside itself (a lap, a quay driven up one side and down the other), the end lies a few metres from a car that still has 200 m to drive. The distance is now counted along the path when that is longer (`GTPDriveLogic.h`).
