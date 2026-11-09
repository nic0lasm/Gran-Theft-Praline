#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPVehicleInteractionComponent.generated.h"

class ACharacter;
class AGTPVehicle;
class APlayerController;

/**
 * Enter / exit key handling, owned by the player controller.
 *
 * On foot: picks the best vehicle around the character (closest driver door
 * within MaxDoorDistance, favouring the one the camera looks at) and starts
 * its entry sequence. Driving: asks the vehicle to let the driver out.
 */
UCLASS(Config = Game, ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPVehicleInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Bound to the enter / exit action. */
	void HandleEnterExit();

	/** Best vehicle the controlled character could enter now, or null. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	AGTPVehicle* FindBestVehicle() const;

protected:
	/** Farthest a driver door may be from the character (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle")
	float MaxDoorDistance = 350.f;

	/** Weight of the camera direction in the choice, in cm per unit of misalignment. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle")
	float ViewAlignmentWeight = 150.f;

private:
	APlayerController* GetPlayerController() const;
};
