#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GTPTrafficFar.h"
#include "UObject/SoftObjectPath.h"
#include "GTPFarTraffic.generated.h"

class AGTPTrafficNetwork;
class UInstancedStaticMeshComponent;

class AGTPVehicle;

/**
 * Far traffic (docs/performance/vehicle-cost.md section 6): cars past the
 * ghost distance, with no physics and no driver at work. Two ways to draw them:
 * - puppets (default): cars of the traffic classes, spawned once, kept at
 *   their lowest LOD with no shadow, no pose update and no collision, moved
 *   by this actor. A handover swaps the roles of two cars (the puppet is
 *   driven, the driven car becomes a puppet), so nothing changes in view;
 * - instances (-FarInstances=1): moving instances of the parked car meshes,
 *   one draw per model, no actor at all. They follow the lanes of the network, keep their
 * distance on a lane and stop at red lights (GTPTrafficFar.h). When one comes
 * within HandoverDistance of the player, it swaps places with the farthest
 * driven car (a ghost by then): the driven car takes its lane, position and
 * speed, the far car takes the driven car's. Out of sight, a far car that
 * comes too close, goes too far or reaches a dead end starts again elsewhere.
 *
 * Spawned by AGTPTrafficNetwork with -FarTraffic=N (or NumFarCars). Ticks
 * every frame; the network ticks twice a second.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPFarTraffic : public AActor
{
	GENERATED_BODY()

public:
	AGTPFarTraffic();

	/** Creates Count far cars on the lanes of Network; returns the number placed. */
	int32 Build(AGTPTrafficNetwork* InNetwork, int32 Count);

	virtual void Tick(float DeltaSeconds) override;

	int32 Num() const { return Cars.Num(); }

	/** Time spent in Tick (seconds) since the last read, for the CPU budget line of the network. */
	double TakeTickSeconds();

protected:
	/** Instances of the parked car meshes instead of puppet cars (-FarInstances=1). */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	bool bInstances = false;

	/** Height of a puppet car's origin above the ground (cm), as a driven car at rest. */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	float PuppetRideHeight = 40.f;

	/** Static meshes, one per model (same as the parked cars: tools/vehicles/unreal/import_parked_static.py). */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	TArray<FSoftObjectPath> Meshes;

	/** Body colours (linear RGB), to PerInstanceCustomData[0..2] like the parked cars. */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	TArray<FLinearColor> Colours;

	/** A far car closer than this to the player (cm) swaps with a driven car. */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	float HandoverDistance = 9000.f;

	/** Out of sight and closer than this (cm) with no driven car to swap with: start again elsewhere. */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	float HideDistance = 5000.f;

	/** New far cars start between these distances from the player (cm), out of sight. */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	float SpawnMinDistance = 12000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	float MaxDistance = 25000.f;

	/** Instances fade out between these distances (cm), like the driven cars (DrawDistance). */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	int32 CullStart = 12000;

	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	int32 CullEnd = 15000;

	/** Distance kept to the car ahead (m) and braking (m/s2). */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	float MinGap = 8.f;

	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	float Decel = 3.f;

	/** Ride height of the mesh origin above the ground (cm): parked meshes sit on the ground. */
	UPROPERTY(Config, EditAnywhere, Category = "Far traffic")
	float GroundOffset = 0.f;

private:
	struct FFarCar
	{
		int32 Lane = INDEX_NONE;
		int Index = 0;
		float Along = 0.f;
		float Speed = 0.f;
		int32 Model = 0;
		int32 Instance = INDEX_NONE;
		/** Puppet mode: the car drawn for this far car (it changes at each handover). */
		TWeakObjectPtr<AGTPVehicle> Puppet;
		float ToPlayer = 0.f;
	};

	const GTPFar::FLanePath& LanePath(int32 Lane);
	float GroundZ(int32 Lane, int32 Point);
	FVector Where(const FFarCar& Car, float& OutYawDegrees);
	bool Place(FFarCar& Car, bool bOutOfSight);
	bool Promote(FFarCar& Car);
	void Show(FFarCar& Car);
	/** Puppet mode: Vehicle stops being a driven car (no physics, no driver at work, lowest LOD, no collision). */
	void MakePuppet(AGTPVehicle* Vehicle);
	/** Puppet mode: Vehicle is a driven car again, as a ghost at Location on Lane, rolling at SpeedMs. */
	void Wake(AGTPVehicle* Vehicle, int32 Lane, int32 Point, const FVector& Location, float YawDegrees, float SpeedMs);
	bool bPuppets = false;

	UPROPERTY(Transient)
	TObjectPtr<AGTPTrafficNetwork> Network;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Models;

	TArray<FFarCar> Cars;
	TMap<int32, GTPFar::FLanePath> Paths;
	/** Ground height at each lane point (cm), traced once when first needed. */
	TMap<int32, TArray<float>> Ground;
	FRandomStream Random;
	double TickSeconds = 0.0;
	int32 Promoted = 0;
	int32 Relocated = 0;
	float StatsAge = 0.f;
};
