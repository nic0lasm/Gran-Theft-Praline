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

So each row carries the line driven on its lane: one closed curve over the lap, a point every 3 m, straights straightened, corners kept where they are and rounded. Since gate 86 the line is then eased among the walls of the town (`tools/city/line_room.py`, `flight_circuit.py write --town <processed data of the zone>`). The long take of gate 86 showed why: the corners rounded "a little" had a radius of 2 to 4 m on five right-hand corners, which no car turns (the stolen car arrived at 74 km/h, turned at full lock at 15 km/h for four seconds, ran wide and was put back on a lane, six times in ninety seconds); the line ran 1.6 m from a building on the place de l'Hôpital; and it ran through the carrousel of the place de la République, a stand-in whose estimated place is on the carriageway of the rue Jean de Tournes. The eased line has the least bending that keeps each of its points within 4 m of the first line at a bend (0.3 m along a straight), 1.2 m inside the measured kerbs (`carriageway.geojson`) and 2.8 m from every wall (buildings, monuments, water): a bend is entered from its outside and cut at its inside, as wide as the room allows. What stands on the line is gone round on the side where nothing else stands: at gate 90 the carrousel, by the north, over the place, 12 m from the lane. Since gate 91 the carrousel itself stands 12 m north, on the place (its place was an estimate; `lane_clear.LANDMARK_MOVES` moves its actor in the base map, and `line_room.py` reads it there whatever the processed data say), and the line runs straight down the rue Jean de Tournes again. The tightest bend of the lap now has a radius of 9.3 m (rue Herriot into the rue Jean de Tournes), the turn back of the approach 8.8 m, and no wall is nearer than 2.8 m. With the game's own driver (`flight_circuit_test.cpp`) a lap takes 109 s instead of 122, and the first minute is driven at 58 km/h instead of 52. Not driven in the game yet.

The line stays within 8 m of its lane, and starts and ends within 16 m of the lane's ends (`GTPFlightCircuit::LineEndsMetres`, 12 m until gate 86). A row whose line goes round something is marked `round` in the last column and may be 16 m from its lane; the game reads only `end` there. `flight_circuit.py check` measures the room of the line when it finds the town next to `--carriageway` (or with `--town`): the nearest wall, the tightest bend; a line that runs through a wall fails the check.

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

The script still puts the car back on a lane when it is held up or off its lane for good, but only on a lane of the lap, and each time counts in `DemoChase ... relocations=N`. A film with a relocation is not a valid one. It never puts it into another car, nor, since gate 83, onto anything else that stops a car (a bike rack, a low wall, a tree): the place is passed over, and the line of the relocation says how many were (`N place(s) passed over with something standing there`). Since gate 86 a place where a person or a car stands is passed over too, whatever they answer to a car's collision channel: on the rue Grolée the car stood on its nose two seconds after it was put down, a passer-by under it. On the lap the car is put on the line it drives there, facing along it, not on the lane's own points (the trace ends with `on its driving line`): put on the lane's points, metres from its line, it started at full lock. And the ground is never an obstacle: the probe that tells what holds the car up touched the road at gate 86, two tiles of the terrain were read as obstacles "at 0 to 2 m" and their lanes closed; it now looks level ahead from 55 cm above the car's origin and passes over a face that looks up (`the ground: not an obstacle`).

Gate 88's long take showed what none of these tests could see: a car of the far traffic. It is drawn and has no collision, so no overlap finds it on the place; with the player suddenly next to it, the far traffic handed it over as a driven car, collision and all, inside the stolen car held on its new lane, and the stolen car stood on its tail 0.3 s after its physics came back (the grey car is in the frames; no line of that log names it). Three rules since: the far traffic never hands a car over within 7 m of the player; the far cars within 15 m of the place are sent away when the car is put there and at every step of the hold (`N far traffic car(s) sent away from the place`); and the physics do not come back while a car is in the held one: a car of the traffic is taken off the road, any other is waited for, 3 s at most (`... is in the car held on its new lane`). The log also names what the car touches in its first two seconds there (`it touches ... of ...`) and who is in it (`who is in the car`).

Gate 91's film ends at 70 s with the flight standing since 62 s behind three cars in the rue Bellecordière, a police car across the street beside them and officers on foot: arrested short of the place Bellecour. Its log names the first one, a Clio 2.5 m to the right of the middle of the lane, which is where the traffic parks a car (`ParkOffset`), and no driver got out of a car before the player did: a row of parked cars, 2.4 m from the driving line. The flight stood 1.6 m to the right of its line when it stopped, four seconds after the right turn into the street (it cuts its corners, and its steering does not bring it back at once), its nose on the first car: brake to a stop, `held up, pushing through` into it, an officer in front, arrested. Two rules since. A car is no longer parked within 3.4 m of the driving line of the circuit (`AGTPTrafficNetwork::ParkFlightClearance`: two half widths and what the flight strays; `N beside the flight's line` in the lines of the parked cars), so the right-hand kerb of the circuit's narrow streets has none. And a traffic car left in the street by a driver who ran is known to the traffic and taken out of the flight's way like the others (`GTPLeftCars.h`; read in the code, seen in no gate so far). Why the flight strays 1.6 m at 50 km/h is not settled: the lines of gate 93 carry its distance to the line each second (`off=`). Not compiled nor played when written.

The police car across that street was a roadblock, 1.4 m from the driving line and 13 m past the parked car: without the parked cars the flight would have stopped there instead. While the script drives, no roadblock is put within 8 m of the circuit's driving lines ([wanted level](wanted.md), roadblocks and the filmed flight).

## At speed on the quay

Gate 89's long take lost the car at the same place in both its flights, as gate 83 had: on the quai Jules Courmont, 99 km/h, the car 1.4 m left of its line, 4.8 m two seconds later, through twelve people of the promenade and into a tree. Three things were found, and each has its rule since gate 93.

The lanes file puts the left lane of the quay 1.5 m from the kerb of the promenade, and the approach drove it. Where the flight is asked 90 km/h or more its line now keeps 4.25 m from the kerbs, a lane of 3 m between the car and the kerb, where the carriageway is that wide and in its middle where it is not (`line_room.off_kerbs`; the kerbs are those met across the line, the mouth of a side street is none). On the quay the approach now runs where the lap does, in the second lane. The row keeps its lane: the line is 3 m beside it.

At speed the car gives its wheels a share of the steering asked, under half at 100 km/h (`GTPSteeringCurve.h`, the keys the vehicle is built with, made for a keyboard). The lane follower reckons a wheel angle; the script now asks the car for more above 50 km/h, the whole of what the car cuts from 80 km/h, so that the wheels turn as reckoned (`GTPFlightPedals::SteeringSent`, `-DemoSteerAlone` undoes it). By the one measure there is (3 degrees a second for 0.12 asked at 99 km/h) the car still turns less than its wheels say: the next log tells.

The next log told. Gate 93 held the quay at 99 km/h, but 2.2 m from its line for three seconds, and the car was 4.4 m from its line at 71 km/h in the rue du Président Carnot. Its `DemoDrive` lines give, once a second, where the car is, where it points and the steering it is sent. Over the 82 seconds driven above 25 km/h, the turn the car made in each second is the one of a car whose front wheels get **the square of the steering sent**, times the share of its speed, to 1.2 degrees a second; a car whose wheels got the steering sent would have turned four times more. The vehicle squares its steering input and keeps its sign (the input curve of a Chaos vehicle's steering, left as the engine sets it, made for a thumb on a stick): half of the stick is a quarter of the lock, a tenth of it a hundredth. The 0.07 of lock the lane follower asked on the quay, sent as 0.15, was 0.010 at the wheels.

The same seconds are replayed outside the game (`tools/vehicles/wanted_sim/steering_input_test.cpp`, in CI): a bicycle model put where the log says the car was, driven at the log's speeds by the game's own lane follower on the line of that gate. With wheels that get the square of the steering sent it is where the game's car was to 0.6 m, 24 seconds on the quay and 10 into the rue du Président Carnot, 2.3 and 4.4 m off the line at most; with wheels that get the steering sent it never leaves its line by more than 0.8 m. Not read in the engine's code: measured.

The script now undoes both the square and the share of the speed, at every speed, so that the wheels get the angle the lane follower reckoned (`GTPFlightPedals::SteeringSent`; `-DemoSteerSquared` sends the steering of gates 93 to 99, `-DemoSteerAlone` the steering as asked). In the replay the car then comes back in a second from where each stretch starts (1.7 and 0.8 m off the line) and stays within 0.4 m of its line, corners included: 0.1 m on the quay at 99 km/h where it was 2.2 m, 0.2 m in the rue du Président Carnot where it was 4.4 m. The `DemoDrive` line says `wheels` after `sent`: the share of the lock the front wheels get. The model has no tyres: whether the game's car holds the quay at 99 km/h with wheels that answer seven times more is for the gate to say.

The police units and the traffic are driven by the same lane follower and had the same square: for them the square is undone where the car takes its driver's steering (`AGTPVehicle::SetAIDriveInput`, `GTPSteeringCurve::InputFor`), the share of the speed left as it is; `-AISteerSquared` gives the steering as before. A street corner asked as 0.3 of the lock was 0.09 at the wheels. The player's own stick keeps the square.

And the pull of the dents. A car beaten on one side pulls to that side: the driving aid of the player's car added the pull to the player's stick and gave the sum to the car at every tick. With a script at the wheel the stick is at 0, so the script's steering was replaced by the pull alone from the first dent on. The aid now leaves the steering alone while a script holds the wheel (`AGTPVehicle::NoteScriptedSteering`; the log says once `its dents pull the steering by ...; a script holds the wheel`). Read in the code: no line of gate 89's log shows a dent.

The `DemoDrive` line, one a second, now carries what was missing to tell these apart: `off` (metres from the line driven), `xy` and `yaw`, `pull` (what the dents would add), and `sent` beside `steer` (given to the car, and asked by the lane follower).

The same take held the second flight's van up four times at one place of the place Bellecour, and each `held up` line named the player's own character "1.5 m ahead", where a van's seat is. The probe no longer sees the car's driver, and before a car held up twice goes elsewhere the log says who and what is in it and round it (`who is in the car (held up twice in the same place)`).

## People in the road

The script's probe is 9 m long. At 98 km/h that is a third of a second: at gate 86 the script gave the whole brake for a person at 0 m (`kmh=98 brake=1.00 limit=obstacle obstacle=0 m`), the car hit them at 86 km/h, went sideways and stopped against a tree 4.4 m from its line. Since then the flight brakes for a person only when it can still stop before them (7 m/s², 1 m of room: under 43 km/h for a person at the end of the probe) or when it is at walking pace; otherwise it keeps its line and its speed (`GTPFlightPedals::StopsForPerson`). The `DemoDrive` line says so: `not braked for`. People getting out of the way of a car is not done yet.

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

`-DemoStopsForPeople`: the flight brakes for any person in its probe, as until gate 86.

## Also changed for every driver

The speed limit at the end of the known path counted the straight distance to that end. Where a path comes back beside itself (a lap, a quay driven up one side and down the other), the end lies a few metres from a car that still has 200 m to drive. The distance is now counted along the path when that is longer (`GTPDriveLogic.h`).
