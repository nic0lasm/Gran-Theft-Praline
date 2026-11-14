#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "GTPMassTraffic.generated.h"

struct FZoneGraphTrafficLaneData;

/**
 * Switch to the MassTraffic plugin of City Sample (ADR-0015, docs/gameplay/traffic-mass.md).
 *
 * -Traffic=mass: the legacy network keeps its lights, street parked cars and police, but drives
 * no car of its own (AGTPTrafficNetwork::IsMassTrafficWanted, which also asks for spawners in the map); the Mass spawners placed by
 * tools/city/unreal/place_masstraffic.py (tag GTPMassTraffic, no spawn on begin play) spawn
 * the traffic and the parked cars on our ZoneGraph lanes. Without the plugin (not installed,
 * WITH_GTP_MASSTRAFFIC=0) nothing happens here and the legacy traffic drives as before.
 *
 * Proof in the log, every 5 s for the first minute then every 30 s ("MassTraffic count"): ZoneGraph
 * data in the map (lanes, zones: 0 lanes means the ZoneGraph was never built, so no car can
 * spawn), cars drawn by Mass as instances (MassVisualizer) and as actors (City Sample's
 * CitySampleMassVehicleBase), and the nearest one to the player.
 * -MassTrafficView: the player is put 12 m from the first Mass car seen (once), for a capture.
 */
UCLASS()
class GTPMASS_API UGTPMassTrafficSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static const FName SpawnerTag;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Spawners started at begin play (0 when MassTraffic is off or absent). */
	int32 GetStartedSpawners() const { return StartedSpawners; }

private:
	void LogCounts();
	/** "MassTraffic cars": the simulated cars (Mass entities), moving or not, near the player or not. */
	void LogSimulatedCars(UWorld& World, const FVector& PlayerLocation);
	/**
	 * With -TrafficDeadEnds=cut, once before the spawners: the car lanes from which every way ends at
	 * a dead end (the edge of the focus area) are taken out of the cars' choice. MassTraffic's lane choice does not look
	 * ahead: a car takes a lane that leads to a dead end, then stops for good before its last lane
	 * (ShouldStopAtLaneExit, "NONEXT"), inside the junction when that last lane leaves one, and the
	 * junction never opens again for anyone. The links to those lanes are removed from MassTraffic's
	 * own lane data (FZoneGraphTrafficLaneData::NextLanes): every other lane keeps a way on. A junction
	 * lane into a lane too short for a car and its gap counts as a dead end too: MassTraffic never lets
	 * a car into it (no room after the junction) and the car waits at the line for good. Cars spawned
	 * inside such a branch are recycled (RecycleAtDeadEnds). On the PC (f8fc873) this took 307 of 577
	 * car lanes out of the traffic, most of the map away from the player.
	 */
	void CutDeadEndBranches();
	/**
	 * Once, before the spawners (default, -TrafficDeadEnds=link): the network is closed instead of
	 * cut, as a GTA city has no end. Each lane with no lane after it (a road leaving the map) leads
	 * on, in MassTraffic's own lane data (FZoneGraphTrafficLaneData::NextLanes), to an entry: a lane
	 * no lane leads into (a road coming into the map), the least used then the nearest. MassTraffic
	 * moves a car onto its next lane by lane and distance only (MoveVehicleToNextLane): the car leaves
	 * the map at one end and comes back at another, and no car stops for good before a dead end
	 * (ShouldStopAtLaneExit, "NONEXT"). A junction lane into a lane too short for a car and its gap
	 * (MassTraffic never lets a car into it) is taken out of the cars' choice first. Every lane keeps
	 * its traffic: nothing is closed. -TrafficDeadEnds=cut cuts the branches as before
	 * (CutDeadEndBranches), -TrafficDeadEnds=none (or -NoDeadEndCut) leaves the lanes as built.
	 */
	void LinkDeadEndsToEntries();
	/** The dead-end handling of -TrafficDeadEnds (link, cut or none), once the lane data is there. */
	void HandleDeadEnds();
	/**
	 * Every 0.25 s while dead ends are linked: no car leaves or comes into the map in the player's
	 * sight, as in GTA. An entry lane is closed (bIsOpen) while the player may see one of its two ends
	 * (the dead end a car leaves from, the entry's own start): within 300 m (-TrafficLoopSight=<m>),
	 * in the camera's field (plus 15 degrees) or within 30 m, and not hidden by a building (a
	 * visibility line trace). MassTraffic stops a car before a closed lane (ShouldStopAtLaneExit), so
	 * the cars wait at the dead end and go on once out of sight. Counts the cars that looped, and those
	 * that did it in sight (a car that could no longer stop): "MassTraffic loops". An entry is also
	 * held while a car MassTraffic simulates with its simple physics (its High/Medium LOD, 200 m by default) is
	 * on one of its dead ends: such a car would be pulled onto the entry in one frame and take that
	 * move as its speed. RecycleAtDeadEnds hands it to MassTraffic's recycling once stopped there out
	 * of sight. -NoLoopGuard leaves the entries open.
	 */
	void GuardLoops();

	/** A dead end linked to an entry: indices in GetMutableTrafficZoneGraphData() and its lane array. */
	struct FLoop
	{
		int32 Slot = INDEX_NONE;
		int32 DataIndex = INDEX_NONE;
		int32 From = INDEX_NONE;
		int32 To = INDEX_NONE;
		int32 FromLane = INDEX_NONE;
		int32 ToLane = INDEX_NONE;
		FVector Exit = FVector::ZeroVector;
		FVector Entry = FVector::ZeroVector;
	};
	TArray<FLoop> Loops;
	int32 LoopEntries = 0;
	/** Entries held closed at the last check (ZoneGraph data index << 32 | lane index). */
	TSet<int64> EntriesInSight;
	/** ZoneGraph lane of each car at the last check, to see it loop. */
	TMap<uint64, int32> LaneOfCar;
	int32 LoopJumps = 0;
	int32 LoopJumpsInSight = 0;
	/** Loops made by a car under MassTraffic's simple physics (should stay 0, see GuardLoops). */
	int32 LoopJumpsInPhysics = 0;
	int32 MostEntriesHeld = 0;
	/** Entries held closed because a car under MassTraffic's physics is on one of their dead ends. */
	int32 EntriesHeldForPhysics = 0;
	int32 MostHeldForPhysics = 0;
	/** The dead ends LinkDeadEndsToEntries linked (lane data in MassTraffic's arrays, never resized). */
	TSet<const FZoneGraphTrafficLaneData*> LoopExitLanes;
	FTimerHandle LoopTimer;
	/**
	 * Every second until MassTraffic's junction entities are there: our junctions have no traffic
	 * lights, so MassTraffic runs them as stop signs, one car per side per turn (the lane closes as the
	 * first car enters it) and each turn waits for the junction to be clear. They are given Epic's
	 * traffic-light behaviour instead (bHasTrafficLights, as at 106 of Small City's 172 junctions):
	 * a side stays open for 10 s (-TrafficJunctionGoSeconds=<s>) and lets every car through, and the
	 * turn ends early once the junction is clear with no car waiting. No light is drawn: the periods
	 * are those MassTraffic built, one per side. -TrafficStopSigns keeps the stop signs.
	 */
	void LightJunctions();
	bool bJunctionsLit = false;
	int32 LightTries = 0;
	FTimerHandle LightTimer;
	/**
	 * Every second: a car in a branch that only leads to dead ends, or standing at the end of a lane
	 * with no lane after it, out of the player's sight, is handed to MassTraffic's own recycling
	 * (FMassTrafficRecyclableVehicleTag, the tag a repaired wreck gets), which moves it to a free
	 * space on one of the least busy lanes away from the player (UMassTrafficOverseerProcessor).
	 * MassTraffic itself never takes a car off a dead end.
	 */
	void RecycleAtDeadEnds();
	/** True for a ZoneGraph lane of a branch cut off by CutDeadEndBranches. */
	bool IsInDeadEndBranch(int32 DataIndex, int32 LaneIndex) const { return DataIndex == DeadEndBranchData && DeadEndBranch.Contains(LaneIndex); }

	int32 StartedSpawners = 0;
	int32 CountLogs = 0;
	bool bViewPlaced = false;
	bool bNetworkLogged = false;
	/** HandleDeadEnds found the lane data and did its work (or none was asked). */
	bool bDeadEndsHandled = false;
	int32 Recycled = 0;
	/** Recycled cars to which Epic's damage fragment had to be added (its recycling needs one). */
	int32 DamageAdded = 0;
	/** ZoneGraph lanes (indices in the data of index DeadEndBranchData) of the cut branches. */
	TSet<int32> DeadEndBranch;
	int32 DeadEndBranchData = INDEX_NONE;
	FTimerHandle RecycleTimer;
	FTimerHandle CountTimer;
};
