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
	/** Bound to the enter / exit action; callable from scripts for playtests. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
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

	/**
	 * Taking a car that still rolls (traffic): within this distance of a door and under this
	 * speed, its driver is made to stop and the player gets in as soon as it does (cm, km/h).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle")
	float FlagDownDistance = 650.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle")
	float FlagDownMaxSpeedKmh = 45.f;

private:
	APlayerController* GetPlayerController() const;

	/** Moving AI car near the character, or null. */
	AGTPVehicle* FindRollingCar() const;
	void TryPendingEntry();

	TWeakObjectPtr<AGTPVehicle> PendingCar;
	FTimerHandle PendingTimer;
	float PendingLeft = 0.f;
};
