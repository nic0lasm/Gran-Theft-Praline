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

	int32 StartedSpawners = 0;
	int32 CountLogs = 0;
	bool bViewPlaced = false;
	FTimerHandle CountTimer;
};
