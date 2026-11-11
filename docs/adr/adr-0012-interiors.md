# ADR-0012: Interiors without loading, the fast-food restaurant as reference

- Status: Proposed (2026-11-11); accepted once step 3 of the build order is measured on M1 and PC
- Context owners: reference interior thread; optimisation (budgets, measures); city v2 (cells, shells); realism (meshes, materials)
- Related: ADR-0002 (budget, M1 capped at 40 fps), ADR-0005 (pedestrians), ADR-0006 (ragdoll), ADR-0010 (cells), docs/gameplay/reference-interior.md
- Note: the 0.9 plan reserved number 0012 for "interiors and SaveGame". This ADR takes the interiors part; the versioned SaveGame becomes **ADR-0013**, for the Shops, economy and save thread.

## Context

Nico's goal (2026-11-11): photorealism at a massive level of detail, with a very large number
of objects. The first proof is a building the player walks into: the fast-food restaurant on
place Bellecour at the corner of rue de la République, with several employees at work who can
be held up, fryers, burgers being made, customers and a realistic till float.

Constraints:

1. **M1 Pro 16 GB without Nanite**, capped at 40 fps (25.0 ms), no frame over 100 ms; the
   street is already at the edge of the budget. **PC** (RTX 3070 Ti, DX12/SM6) has Nanite.
2. **No loading screen** between street and interior: the glass fronts on place Bellecour and
   rue de la République show the room from outside and the street from inside.
3. **Everything regenerable from data**, headless, like the city (ADR-0010).
4. **Branding is local**: real names as lettering at most, no logo or trade dress in Git or in
   packages (QA thread interim rule). No real person.

## Options

**A. Interior placed in the cell level.** Simple, but the cell carries thousands of interior
objects even when the player is 200 m away, and regenerating the interior rewrites the cell.

**B. One streaming level per interior, loaded by distance to its door** (the ADR-0010
mechanism, one level deeper). The cell keeps the shell and a shop-window view of the room
(`M_City_ShopInterior` interior mapping, already used by city v2); the interior level
`L_Interior_<id>` is generated from the layout CSV and loaded asynchronously when the player
comes within the preload radius, unloaded with hysteresis.

**C. Data layers in World Partition.** Rejected for the same reasons as in ADR-0010.

## Decision (proposed)

**B**, with these rules:

1. **Streaming.** Preload radius 40 m from the door (`-InteriorPreload=`), unload at 70 m.
   Instances serialised in the level (never built on the game thread in one block), PSOs
   precached, so entering at a run gives **no frame over 100 ms**. Outside the radius the
   room is the interior mapping card of the shop window: zero objects.
2. **Instancing by class and level.** One instanced static mesh component per mesh class per
   floor (`ism_group` column of the CSV): thousands of objects for a few hundred draw calls.
3. **Kits.** Stacks and boxes (cups, lids, wrappers, bags, fry cartons, napkins) are one mesh
   showing N items. A kit becomes single items only when the player touches it.
4. **Promotion.** Objects are static instances until something acts on them (an employee's
   work loop, a hit, the player's hand): then the instance is hidden and replaced by an actor,
   and goes back to an instance once at rest. Same principle as the far traffic ghosts.
   Caps: 64 simultaneous physics bodies on M1, 256 on PC.
5. **Cost by object size** (tools/interiors/catalog.json): micro objects (under 5 cm) cull at
   4 m and never cast dynamic shadows; small ones cull at 12 m, no shadows on M1; every mesh
   has LODs, which are the M1 fallback since Nanite is inactive there. No distance field for
   micro and small objects.
6. **Budgets inside the interior** (proposed to the optimisation thread, which owns them):

   | While inside | M1 (no Nanite, 40 fps) | PC (Nanite, SM6) |
   |---|---|---|
   | Interior GPU | ≤ 5 ms of the 25 ms | ≤ 4 ms at Ultra |
   | Rendered instances | ≤ 12 000 | ≤ 40 000 |
   | Unique meshes | ≤ 260 | ≤ 600 |
   | Draw calls (main / shadow) | ≤ 1 400 / 450 | ≤ 3 500 / 1 500 |
   | Triangles after LOD | ≤ 3 M | Nanite (source ≤ 25 M) |
   | Characters (staff + customers) | ≤ 20, animation rate by distance | ≤ 35 |
   | Shadowed lights | ≤ 6 | ≤ 16 |
   | Memory of the interior level | ≤ 600 MB, 2K textures max | ≤ 1.5 GB |

   The layout generator checks the countable ones in CI. The first layout gives, at the
   default densities: M1 9 505 perceived objects, 3 523 instances, 92 meshes, 184 draw calls;
   PC 9 880 objects for 196 draw calls (lower bounds: one material per section; one component per class, floor and density level).
7. **The city while inside.** Lumen and the street through the glass stay; cells behind the
   building can drop to their far proxy while the player is inside (the optimisation thread
   decides it from the measure).
8. **Switches.** Every step can be turned off from the command line (`-Interiors=0`,
   `-InteriorDensity=0..3`, `-InteriorStaff=`, `-InteriorCustomers=`, `-InteriorCooking=`,
   `-HoldUp=`, `-InteriorPhysics=`, `-Branding=`) for A/B and fallback.

## Consequences

- Every interior that follows (shops for 0.8, the safehouse) uses the same pipeline: a JSON
  spec, the shared catalogue, the generator, a streaming level.
- Gameplay rules (till, hold-up, work loops) are plain C++ headers tested with g++ in CI, the
  game wraps them in components: same pattern as wanted level and far traffic.
- Branding, logos and any real trade dress stay in a local, git-ignored layer.

## Acceptance

Accepted when step 3 of docs/gameplay/reference-interior.md is measured by the optimisation
thread: M1 packaged, capped at 40, entering and leaving at a run, ≥ 99 % of frames at
25.0 ms ±1, 0 frame over 100 ms, interior GPU ≤ 5 ms at density 2; PC compiled and measured
at density 3.
