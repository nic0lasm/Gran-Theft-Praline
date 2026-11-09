#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GTPVehicleSetupLibrary.generated.h"

class UPhysicsAsset;
class USkeletalMesh;

/**
 * Editor-time helpers for the car import (tools/vehicles/unreal/import_vehicle.py).
 *
 * Chaos Vehicles simulate the wheels as raycasts: the physics asset must hold
 * one body on the root bone only. The importer's generated physics asset has
 * a body per wheel too, which fights the suspension, so it is rebuilt here
 * as two boxes (lower body and cabin) measured from the wheel bones.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPVehicleSetupLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Replaces every body and constraint of PhysicsAsset with a chassis made
	 * of two boxes on RootBone, sized from Mesh's bounds and wheel bones
	 * (bones whose name starts with WheelBonePrefix). Returns a short report,
	 * empty on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Vehicle Setup")
	static FString BuildChassisPhysicsAsset(USkeletalMesh* Mesh, UPhysicsAsset* PhysicsAsset,
		FName RootBone = TEXT("root"), FString WheelBonePrefix = TEXT("Phys_Wheel"));
};
