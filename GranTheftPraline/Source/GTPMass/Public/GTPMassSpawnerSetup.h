#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GTPMassSpawnerSetup.generated.h"

/**
 * Editor help for tools/city/unreal/place_masstraffic.py: gives a placed Mass spawner its spawn
 * data generator (the object that says where entities go, such as MassTraffic's vehicle and
 * intersection generators). City Sample's spawner Blueprints come with none (their generators were
 * set on the spawners of City Sample's own map), and the array holds instanced objects that Python
 * cannot create for a class it does not expose.
 */
UCLASS()
class GTPMASS_API UGTPMassSpawnerSetup : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Replaces the spawn data generators of Spawner (an AMassSpawner placed in a level) by one new
	 * generator of the class at GeneratorClassPath ("/Script/MassTraffic.MassTrafficVehicleSpawnDataGenerator"),
	 * proportion 1. Returns the generator's object path, for UGTPAssetReflection::SetPropertyText,
	 * or "error: ..." when nothing was changed.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Mass")
	static FString SetSpawnDataGenerator(AActor* Spawner, const FString& GeneratorClassPath);
};
