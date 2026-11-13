#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GTPTheftDemo.generated.h"

class ACharacter;
class AGTPVehicle;

/**
 * Car thefts for scripted runs (-GTARun of the police thread, playtest scripts). The player goes
 * through the real game code: the enter key (UGTPVehicleInteractionComponent::HandleEnterExit),
 * the window break, alarm, hot-wiring, flag-down and carjack of AGTPVehicle. Only the walk to the
 * door is skipped.
 *
 * Parked car with an alarm:
 *   Car = PlaceAlarmCar(World, PlayerLocation, 3000);   // log "street parked ... alarm"
 *   PlaceAtDriverDoor(Player, Car); PressEnter(World);  // log "entering (BreakWindow ...)", "alarm"
 *   wait until IsDriving(Player, Car) (about 3 s: 1.6 s window, 1.4 s hot-wire), drive, PressEnter to get out.
 * Traffic car whose driver is pulled out:
 *   Car = FindTrafficCar(World, PlayerLocation, 6000); StopTrafficCar(Car);
 *   wait until IsStopped(Car) (2 s at most), PlaceAtDriverDoor(Player, Car), PressEnter(World);
 *   log "entering (Carjack ...)" then "carjack: ... pulled out of ..., flee|resist|cower";
 *   wait until IsDriving(Player, Car) (about 2.2 s).
 * Every step logs "theft demo:" lines for the report of the run.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPTheftDemoLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A locked street parked car with an alarm at the kerb within RadiusCm of Near; null if no spot. */
	UFUNCTION(BlueprintCallable, Category = "GTP|Demo", meta = (WorldContext = "WorldContext"))
	static AGTPVehicle* PlaceAlarmCar(UObject* WorldContext, FVector Near, float RadiusCm = 3000.f);

	/** The nearest traffic car with its driver (not police, not wrecked) within MaxDistanceCm of Near. */
	UFUNCTION(BlueprintCallable, Category = "GTP|Demo", meta = (WorldContext = "WorldContext"))
	static AGTPVehicle* FindTrafficCar(UObject* WorldContext, FVector Near, float MaxDistanceCm = 6000.f);

	/** The traffic driver brakes and waits (as when the player flags the car down). */
	UFUNCTION(BlueprintCallable, Category = "GTP|Demo")
	static void StopTrafficCar(AGTPVehicle* Car, float Seconds = 6.f);

	UFUNCTION(BlueprintPure, Category = "GTP|Demo")
	static bool IsStopped(const AGTPVehicle* Car);

	/** Puts the character beside the driver door, facing it, out of the car's way. */
	UFUNCTION(BlueprintCallable, Category = "GTP|Demo")
	static bool PlaceAtDriverDoor(ACharacter* Character, AGTPVehicle* Car);

	/** The enter / exit key of the first player. */
	UFUNCTION(BlueprintCallable, Category = "GTP|Demo", meta = (WorldContext = "WorldContext"))
	static bool PressEnter(UObject* WorldContext);

	/** The character is at the wheel of Car (the entry sequence is over). */
	UFUNCTION(BlueprintPure, Category = "GTP|Demo")
	static bool IsDriving(const ACharacter* Character, const AGTPVehicle* Car);
};
