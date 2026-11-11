# Reference interior 1: the fast-food restaurant on place Bellecour

Goal set by Nico on 2026-11-11: walk into the fast-food restaurant at the corner of place
Bellecour and rue de la République and find the game at the expected level inside:
photorealism with a massive number of objects, several employees at work who can be held up,
fryers, burgers being made, customers, a realistic till float. This interior is the yardstick
for every building that follows. Architecture: [ADR-0012](../adr/adr-0012-interiors.md).

Brand rule: the restaurant is generic in Git and in packages (real name as lettering at most,
QA thread rule). Logos, colours and menu names of the real chain live in a local, git-ignored
branding layer (`-Branding=local|generic`). No real person: staff and customers are generic
characters.

## 1. The building, from public data

| Source | What it gives |
|---|---|
| OSM node 35694312 (45.75820 N, 4.83423 E) | fast food, burgers, indoor seating, terrace on the pedestrian zone, cash and cards accepted, open 9:00 to 1:00 (2:00 on Friday and Saturday), wheelchair access |
| BD TOPO building `T24GFDBJTRSJ` | footprint 329 m², 7 levels, 28.3 m high, use "commercial and services", ground at 166.9 m |
| Geo cache (city v2) | the shop sign already generated for this node (`T_Sign_node_35694312`) |

The footprint is an irregular polygon of about 22 × 21 m, with the long glass front facing
place Bellecour (south) and a side on rue de la République (east). The room split below is a
working hypothesis from street knowledge, written in `tools/interiors/fastfood_bellecour.json`
and easy to correct: one edit, one regeneration.

## 2. Floor plan (hypothesis)

Working rectangle 19 × 16 m aligned on the Bellecour front, 3.6 m storeys.

| Zone | Level | Size | Content |
|---|---|---|---|
| Ground dining | 0 | 12 × 7.5 m | 10 tables for 2, 4 for 4, 3 high tables, terrace side on the glass |
| Kiosks | 0 | 7 × 3.5 m | 6 order kiosks near the République door |
| Queue | 0 | 7 × 4 m | queue lanes, pickup screens |
| Counter | 0 | 15 × 1.5 m | 4 tills, 2 drink towers, coffee corner, 2 pickup points |
| Kitchen | 0 | 15 × 4.5 m | 4 fryers, fry dump, 2 grills, 2 toasters, 2 assembly lines, 2 holding cabinets, 4 bun racks, shake machine, wash area |
| Storage, walk-in | 0 | 15 × 2.5 m | 6 dry shelves, 4 cold racks |
| Stairs | 0-1 | 4 × 8.5 m | 2 flights |
| Upper dining | 1 | 19 × 9 m | 12 tables for 2, 8 for 4, 5 booths |
| Crew room, office | 1 | 9 × 7 m | lockers, desk, safe, monitors |
| Toilets | 1 | 10 × 7 m | 4 toilets, 4 basins |

## 3. Object inventory by class

Mesh classes and their cost rules are in `tools/interiors/catalog.json` (94 classes); the
generator places them and counts them. At the default densities:

| Family | Examples | Perceived objects (M1 / PC) |
|---|---|---|
| Kitchen equipment | fryers, baskets, grills, toasters, holding cabinets, drink towers, coffee, shake | ~90 |
| Food | buns (racks of 96), patties, pastries, fries, fry bags, frozen cases | ~1 100 |
| Packaging (kits) | cups, lids, wrappers, boxes, bags, napkins, sauce packets, straws | ~6 000 shown by ~200 kit meshes |
| Furniture | tables, chairs, stools, booths, kiosks, lockers, shelving | ~260 |
| Table leftovers | trays, wrappers, cups, fries, receipts (60 % of tables used) | ~400 to 700 |
| Building fixtures | ceiling tiles, lights, sprinklers, vents, sockets, drains, extinguishers | ~1 300 |
| **Total** | | **9 575 / 9 880 objects for 3 593 / 3 898 instances and ~155 draw calls** |

Density levels (`-InteriorDensity=`): 0 equipment and furniture only (2 170 objects),
1 half stocked (5 893), 2 fully stocked with leftovers (M1 default), 3 everything including
loose fries on tables (PC default). Raising the count further is a data change: the
catalogue and budgets tell straight away whether it still fits.

## 4. Employees, stations and work loops

Staff is a type of its own (`AGTPStaffCharacter`, persistent in the interior level), with the
pedestrians' skeleton and looks. Default crew in the evening: 8 (M1) to 10 (PC).

| Station | Staff | Loop (each step moves a real object) |
|---|---|---|
| Fries | 1 | bag of frozen fries → basket → lower into oil (timer, bubbles) → lift, drain → dump → salt → scoop into cartons → carton to the holding chute |
| Grill | 1 | patties from the freezer drawer → clamshell grill → cook → holding cabinet tray |
| Assembly | 2 | toast bun → read the kitchen screen → bun, sauce gun, condiments, patty, cheese → wrap or box → slide to the counter |
| Counter and drinks | 2 | take the order at the till → cash or card → cup → drink tower → lid → tray or bag |
| Pickup | 1 | call the number, hand over the bag or tray |
| Manager | 1 | walks the line, skims the tills to the safe, restocks change, office |

A loop is a list of station slots and object actions (pick, place, transform). Objects of a
loop are instances that the employee promotes to an actor while handling them (ADR-0012 §4);
a burger is a stack of instances that changes state (bun → bun + patty → wrapped). Orders
come from the customers, so the kitchen works at the pace of the room.

## 5. Customer flow

Customers are pedestrians of the crowd pool (pedestrians thread) that go in from the street:
door → kiosk or queue → till → wait (screen shows their number) → pickup → table or exit →
eat (time by meal) → tray return → exit. Target occupancy by hour (lunch peak, quiet
afternoon, late evening). M1 up to 12 customers inside, PC 25.

## 6. Hold-up of several employees

Rules in `GTPHoldUpModel.h` (tested in CI):

- Fear per person. The generic reaction (Calm → Alarmed → HandsUp → Complying / Backing /
  Fleeing / Down), fear and fear contagion belong to the ragdoll thread
  (`UGTPThreatResponseComponent`); the hold-up script listens to its states.
- The player covers one employee at a time. The covered cashier, ordered to empty the till,
  opens it (2.5 s) and hands over the notes (4 s); coins only if the player takes the time.
- Employees nobody covers act after a few seconds if they are brave: the manager presses the
  silent alarm under the counter (crime reported 20 s later by the monitoring centre), a crew
  member slips out of the back door (crime reported as soon as they reach the street).
  Shouting raises everyone's fear and restarts their time out of cover: it keeps the room under control.
- Customers react through the pedestrians' `OnPedestrianScared`; a hit person goes to the
  ragdoll thread's hit reactions.
- Crime reported through `UGTPWantedComponent::ReportCrime`; the police thread does the rest.
- The safe (office, upstairs) is a later step: it needs the manager and a time lock.

## 7. The till float

Rules in `GTPCashDrawerModel.h` (tested in CI). Every till opens with a 150.00 euro float:
1 × 20, 4 × 10, 6 × 5, 10 × 2, 20 × 1, 20 × 0.50, 25 × 0.20, 30 × 0.10, 20 × 0.05,
25 × 0.02, 50 × 0.01. The shift is simulated from the opening hour: 45 customers an hour per
till, 18 % pay cash, the change comes out of the drawer coin by coin, the manager restocks
change from the safe and skims notes above 400 euros down to 100. A robbed till therefore
holds between 100 and about 550 euros, depending on the hour; one example after 3 hours:
275.00 euros in notes and 26.90 in coins. The notes the player gets are real notes (count per
denomination), shown as such in the bag and on the HUD.

## 8. Inside and outside

The interior is its own streaming level, loaded 40 m from the door and unloaded at 70 m
(ADR-0012). From further away the shop window shows the interior mapping card. Doors are open
passages, no loading. Light: the room is lit by its own fixtures (6 shadowed lights on M1),
the street stays visible through the glass.

## 9. Budgets

See ADR-0012 §6. The generator report (`interior_layout.py --report`) gives the countable
part on every push; GPU, memory and hitches are measured by the optimisation thread at each
step.

## 10. Build order, in thin testable steps

Each step is behind a switch, off by default until measured, and passes the merge gate of
the 0.9 plan (CI, PC compile, M1 capped at 40: ≥ 99 % at 25.0 ms ±1, no frame over 100 ms,
GPU p95 ≤ 24 ms).

| Step | What | Switch | Where | Proof |
|---|---|---|---|---|
| 1 | Rules off-machine: till float and shift, multi-employee hold-up, layout generator and object budget in CI | none (tools and headers) | cloud | CI green (this branch) |
| 2 | Shell and streaming: `L_Interior_fastfood_bellecour` generated from the CSV at density 0 with grey boxes sized per class, glass front, door, lights; streaming by distance | `-Interiors=0/1`, `-InteriorPreload=` | PC compile, Mac bench | entering at a run: 0 frame over 100 ms; interior GPU at density 0 |
| 3 | Density: kits and instances up to density 3, free assets swapped class by class (`catalog` → asset map) | `-InteriorDensity=0..3` | PC (assets, compile), Mac bench | budget table of ADR-0012 measured; **ADR-0012 accepted** |
| 4 | Staff work loops (stations, object promotion) | `-InteriorStaff=N` | cloud (loop rules in C++ tested), PC, Mac | video of a full service; CPU cost of N employees |
| 5 | Cooking visuals: oil, steam, burger states, carton filling | `-InteriorCooking=0/1` | PC, Mac | stills against reference photos; effect budget |
| 6 | Customers from the street | `-InteriorCustomers=N` | with the pedestrians thread | lunch peak at the M1 cap |
| 7 | Hold-up wired to the threat states and the tills, alarm, ReportCrime | `-HoldUp=0/1` | with ragdoll, police, then combat | video of a hold-up of 3 employees and the escape |
| 8 | Physics: knocked objects, spills, promotions under cap | `-InteriorPhysics=0/1` | with ragdoll | no frame over 100 ms during a brawl |
| 9 | Local branding layer, menu boards, lettering | `-Branding=local/generic` | Mac and PC, local only | build without the layer stays clean |
| 10 | Photoreal pass: scanned materials, wear, grease, light | none | realism, PC | side-by-side with street-level references; gate held |

## 11. Who does what

| Thread | Part |
|---|---|
| Reference interior (this one) | spec, generator, interior level, staff and loops, till and hold-up script, switches |
| Realism | interior meshes and materials (reuses `gtp_surfaces.py`, `surfaces.json`, `lean_small()`, atlas merging) |
| City v2 | building shell, door openings, shop window card, cell link |
| Pedestrians | customers (pool), `OnPedestrianScared` |
| Ragdoll | threat states, fear, hit reactions |
| Police | `ReportCrime` and response |
| Optimisation | budgets, measures, merge gate |
| PC Windows | free assets download (Fab, Megascans), compile check, PC measure |
| Shops, economy, save (to open) | money kept, buying, SaveGame (ADR-0013), other shops on this pipeline |
| Combat (to open) | the weapon threat that replaces `-ThreatDemo` |
| Missions (to open) | the "rob the shop" activity uses this restaurant |
