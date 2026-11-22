#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UWorld;

/** One driving MassTraffic car, where it is shown (its Mass transform: the instance the player sees). */
struct FGTPMassTrafficCar
{
	FVector Location = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	/** Along its lane, cm/s. */
	float SpeedCmS = 0.f;
	/** MassTraffic's agent radius (cm): what it keeps from the car ahead, about half its length. */
	float RadiusCm = 0.f;
	/** The Mass entity (index and serial number), the same from one call to the next while the car lives. */
	uint64 Id = 0;
};

/**
 * The MassTraffic cars near a point, for the code of the game module, which cannot see Mass (the module GTPMass
 * depends on this one, not the other way). UGTPMassTrafficSubsystem (GTPMass) sets the query when it starts the cars.
 * Read by the crash step of -GTARun (GTPGtaRun.h).
 */
namespace GTPMassTrafficCars
{
	using FQuery = TFunction<void(UWorld& World, const FVector& Near, float RadiusCm, TArray<FGTPMassTrafficCar>& Out)>;

	GRANTHEFTPRALINE_API void SetQuery(FQuery Query);

	/** The cars within RadiusCm of Near in Out; false when no query is set (-Traffic=mass off, MassTraffic not installed). */
	GRANTHEFTPRALINE_API bool Find(UWorld& World, const FVector& Near, float RadiusCm, TArray<FGTPMassTrafficCar>& Out);
}
