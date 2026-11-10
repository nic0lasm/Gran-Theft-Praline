#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPRiverRecovery.generated.h"

class AActor;
class AGTPVehicle;
class APawn;

/**
 * What happens in the Rhône (version 0.3). The city generation places, on the
 * water, trigger boxes tagged GTP_RiverRecovery, and on both embankments, at
 * street level and facing land, points tagged GTP_RiverExit
 * (tools/city/riverside.py). A riverbed with collision 2.5 m down keeps
 * anything from falling through the world.
 *
 * - Player on foot: swims (floats) for SwimSeconds, then the screen fades and
 *   the player stands at the nearest exit point.
 * - Car: it sinks and is drowned (wrecked); the player at the wheel is taken
 *   out and brought to the nearest exit the same way; a car nobody drives is
 *   removed after RemoveCarSeconds.
 * No water plugin, no physics volume: plain overlap events, cheap on the M1.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPRiverRecovery : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Nearest exit point to Location; false when the map has none. */
	bool FindExit(const FVector& Location, FTransform& OutExit) const;

protected:
	UPROPERTY(Config)
	float SwimSeconds = 4.f;

	/** Time a car takes to sink before the driver is brought ashore (s). */
	UPROPERTY(Config)
	float SinkSeconds = 2.5f;

	UPROPERTY(Config)
	float FadeSeconds = 0.6f;

	UPROPERTY(Config)
	float RemoveCarSeconds = 12.f;

private:
	UFUNCTION()
	void OnEnterWater(AActor* Water, AActor* Other);

	void BringAshore(TWeakObjectPtr<APawn> Pawn);
	void RemoveCar(TWeakObjectPtr<AGTPVehicle> Car);

	TArray<FTransform> Exits;
	/** Actors already handled, so overlaps of several boxes count once. */
	TSet<TWeakObjectPtr<AActor>> InWater;
};
