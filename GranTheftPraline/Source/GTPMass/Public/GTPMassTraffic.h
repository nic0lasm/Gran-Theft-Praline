#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "GTPMassTraffic.generated.h"

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
	 * Once, before the spawners: the car lanes from which every way ends at a dead end (the edge of
	 * the focus area) are taken out of the cars' choice. MassTraffic's lane choice does not look
	 * ahead: a car takes a lane that leads to a dead end, then stops for good before its last lane
	 * (ShouldStopAtLaneExit, "NONEXT"), inside the junction when that last lane leaves one, and the
	 * junction never opens again for anyone. The links to those lanes are removed from MassTraffic's
	 * own lane data (FZoneGraphTrafficLaneData::NextLanes): every other lane keeps a way on. Cars
	 * spawned inside such a branch are recycled (RecycleAtDeadEnds). -NoDeadEndCut keeps the links.
	 */
	void CutDeadEndBranches();
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
	bool bDeadEndsCut = false;
	int32 Recycled = 0;
	/** Recycled cars to which Epic's damage fragment had to be added (its recycling needs one). */
	int32 DamageAdded = 0;
	/** ZoneGraph lanes (indices in the data of index DeadEndBranchData) of the cut branches. */
	TSet<int32> DeadEndBranch;
	int32 DeadEndBranchData = INDEX_NONE;
	FTimerHandle RecycleTimer;
	FTimerHandle CountTimer;
};
