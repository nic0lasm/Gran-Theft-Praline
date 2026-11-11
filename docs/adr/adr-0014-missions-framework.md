# ADR-0014: Missions and activities framework, data in tables

- Status: Proposed (2026-11-11); accepted once the three 0.9 activities run on M1 and PC through the merge gate
- Context owners: missions thread; consumes police (stars), shops/economy/save (money, SaveGame, interiors), combat (threat), traffic and vehicles
- Related: ADR-0004 (wanted), ADR-0010 (cells), ADR-0012 (interiors), ADR-0013 (SaveGame), docs/gameplay/missions.md
- Numbering: 0011 combat, 0012 interiors, 0013 SaveGame are taken by other threads; this one takes 0014.

## Context

The 0.9 test session needs a goal: a minimal mission framework and three activities (rob the
shop, steal a car to order, lose the police). Full framework and six activities come in 0.12.
No story and no character yet: the story is written later with Nico.

Constraints: no engine-wide cost (the merge gate allows no regression), every system can be
switched off (`-Missions=0`), texts only through string tables (FR/EN), and the logic must be
testable outside Unreal. The missions thread must read police, economy, combat and traffic
without modifying them.

## Options

**A. Blueprint or script per mission.** Fast to start, but missions become code, cannot be
checked by CI, and cannot be saved or resumed in a uniform way.

**B. Quest plugin or Unreal assets (UDataAsset graph).** Heavy editor work, binary files that
cannot be diffed in Git, and not testable without the engine.

**C. Tables (CSV) read by a small engine-free state machine, with an adapter in Unreal.**
Missions and objectives are rows; the runner is plain C++ that takes a context each tick and
events as they happen, and returns commands. Diffable, checkable in CI, and every other system
is reached through the adapter only.

## Decision (proposed)

**C.**

1. **Tables** in `Content/Data/Missions/`: `missions.csv` (id, kind, title and brief keys, start
   zone, repeatable, cooldown, money reward, time limit, retry policy, start effects,
   prerequisites), `objectives.csv` (ordered rows per mission: type, target, parameters, hint
   key, checkpoint flag, time limit, failure events), `zones.csv` (city local metres and radius, snapped on
   real street vertices), `strings.tsv` (FR/EN texts, to be folded
   into `GTPStringTable.h` by the interface thread). The parser rejects every inconsistency
   (unknown zone, gap in indexes, bad number, unknown type) and CI runs it on the shipped tables.
2. **Objective types**: GoTo, EnterInterior, WaitEvent, EnterVehicle, DeliverVehicle,
   LoseWanted. A new activity is new rows; a new type is one `case` and one test.
3. **State machine** (`GTPMissionRunner.h`): Idle → Active → Succeeded | Failed; a failure waits
   for Retry (resume at the last checkpoint, or restart, or none, per mission) or Abort.
   Objective 0 is always a resume point. Always failing: `player.arrested`, `player.died`.
   One active mission at a time.
4. **No coupling.** The runner never calls another system. It reads a context (position, vehicle,
   stars, interior) and events (`holdup.cash_taken`, `player.arrested`, `vehicle.destroyed`…) and
   emits commands (show objective, waypoint, ensure stars, spawn vehicle, checkpoint, reward,
   result). The Unreal adapter (`UGTPMissionSubsystem`, written when the interfaces are on `next`)
   translates both ways: it subscribes to the delegates of the other threads and executes the
   commands through their public APIs.
5. **Reward and save.** A success increments the completion count and emits `GiveReward` with a
   run key `<mission>#<count>` in the same step, and marks the save dirty. Economy refuses a
   run key it has already paid; SaveGame (ADR-0013, atomic write) stores `FSaveData`
   (completions, cooldowns, running mission and checkpoint). Loading a save resumes the running
   mission at its checkpoint. Nothing finer is stored (no mid-objective state).
6. **Switch**: `-Missions=0` creates no subsystem. Cost when idle: one table lookup per tick of
   an active mission, nothing when none is active (target 0 ms).

## Consequences

- Positive: new activities are data; the whole logic is tested in CI in milliseconds; save and
  retry are uniform; other threads are never modified.
- Negative: complex branching missions (0.12) will need conditions in tables or a second
  runner level; that is deliberately left out of 0.9.
- Dependencies to confirm with their owners: the event names and the `ReportCrime`-based way to
  raise stars at the start of the flee activity (police), the idempotent `AddMoney(amount,
  reason, runKey)` (shops/economy), the `holdup.*` events (reference interior), vehicle
  lookup by tag near a zone (traffic/parked cars).
