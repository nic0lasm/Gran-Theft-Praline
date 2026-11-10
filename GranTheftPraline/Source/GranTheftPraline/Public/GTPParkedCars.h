#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "GTPParkedCars.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/**
 * Parked cars along the kerbs, drawn as instances of one static mesh per model
 * (docs/performance/vehicle-cost.md section 5): no physics, no tick, one draw
 * per model and LOD. Only with -ParkedCars=1 (or bParkedCars on
 * AGTPTrafficNetwork), which spawns it.
 *
 * Slots come from Content/City/Traffic/<MapName>.parking.csv (written by
 * tools/city/parking.py): x_cm, y_cm, yaw_deg, road_id. The model and the
 * body colour of each slot are picked from its position, so they stay the
 * same at every run; the colour goes to PerInstanceCustomData[0..2]
 * (M_Parked_Body multiplies its base colour by it).
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPParkedCars : public AActor
{
	GENERATED_BODY()

public:
	AGTPParkedCars();

	/** Path of the parking file for a map name, under the project content folder. */
	static FString ParkingFileFor(const FString& MapName);

	/** Reads the slots and fills the instances; returns the number of cars placed. */
	int32 Build(const FString& File);

protected:
	/** Static meshes of the parked cars, one per model (tools/vehicles/unreal/import_parked_static.py). */
	UPROPERTY(Config, EditAnywhere, Category = "Parked cars")
	TArray<FSoftObjectPath> Meshes;

	/** Body colours picked from (linear RGB). */
	UPROPERTY(Config, EditAnywhere, Category = "Parked cars")
	TArray<FLinearColor> Colours;

	/** Instances fade out between these distances (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Parked cars")
	int32 CullStart = 12000;

	UPROPERTY(Config, EditAnywhere, Category = "Parked cars")
	int32 CullEnd = 15000;

	/** Shadows only matter close by: none for parked cars by default on M1. */
	UPROPERTY(Config, EditAnywhere, Category = "Parked cars")
	bool bCastShadows = false;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Models;
};
