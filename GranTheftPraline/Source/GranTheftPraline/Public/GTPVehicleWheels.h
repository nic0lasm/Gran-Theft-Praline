#pragma once

#include "CoreMinimal.h"
#include "ChaosVehicleWheel.h"
#include "GTPVehicleWheels.generated.h"

/**
 * Front wheel of a French city car (195/55 R16 class): steers, drives
 * (front-wheel drive), brakes, no handbrake.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPWheelFront : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UGTPWheelFront();
};

/**
 * Rear wheel of a French city car: handbrake (for slides), no steering,
 * no drive; a little less grip than the front so the handbrake turn works.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPWheelRear : public UChaosVehicleWheel
{
	GENERATED_BODY()

public:
	UGTPWheelRear();
};
