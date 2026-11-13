#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPMassTraffic.generated.h"

/**
 * Switch to the MassTraffic plugin of City Sample (ADR-0015, docs/gameplay/traffic-mass.md).
 *
 * -Traffic=mass: the legacy network keeps its lights, street parked cars and police, but drives
 * no car of its own (AGTPTrafficNetwork::IsMassTrafficWanted); the Mass spawners placed by
 * tools/city/unreal/place_masstraffic.py (tag GTPMassTraffic, no spawn on begin play) spawn
 * the traffic and the parked cars on our ZoneGraph lanes. Without the plugin (not installed,
 * WITH_GTP_MASSTRAFFIC=0) nothing happens here and the legacy traffic drives as before.
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
	int32 StartedSpawners = 0;
};
