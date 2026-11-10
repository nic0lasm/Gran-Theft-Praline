#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "GTPDriveLogic.h"
#include "GTPTraffic.generated.h"

class ACharacter;
class AGTPVehicle;
class AGTPTrafficNetwork;

/** One driving lane, in world space (cm), from tools/city/traffic.py. */
USTRUCT(BlueprintType)
struct FGTPTrafficLane
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TArray<FVector> Points;

	/** Lanes this one leads to; empty = dead end (the car is taken away out of sight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TArray<int32> Next;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float SpeedKmh = 30.f;

	/** Junction at the end of the lane (one car at a time crosses it), -1 for a plain join. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	int32 ToJunction = -1;

	float LengthCm() const;
};

/**
 * Drives a car along the lanes of the traffic network (GTPDriveLogic.h, the
 * same decisions as tools/vehicles/traffic_sim). Stops for anything ahead,
 * the player on foot included. Switches itself off when the AI driver
 * leaves the seat (car stolen by the player).
 */
UCLASS(ClassGroup = (Traffic), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPAIDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPAIDriverComponent();

	/** Starts on Lane at its point StartPoint. */
	void Start(AGTPTrafficNetwork* InNetwork, int32 Lane, int32 StartPoint);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True when the car stands at the end of a dead-end lane. */
	bool IsAtDeadEnd() const { return bAtDeadEnd; }

	int32 GetLane() const { return Route.Num() > 0 ? Route[0] : INDEX_NONE; }

	/** Seconds the car has been standing still. */
	float GetStoppedTime() const { return StoppedTime; }

	/** Gives back the junctions this car holds (car taken away or stolen). */
	void ReleaseJunctions();

	/** Length of the obstacle probe ahead (cm). */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float ProbeLength = 4000.f;

	/** Half width of the obstacle probe (cm): a lane is about 3 m. */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float ProbeRadius = 90.f;

private:
	void PlanRoute();
	float FindObstacleMetres(const AGTPVehicle* Vehicle) const;

	UPROPERTY(Transient)
	TObjectPtr<AGTPTrafficNetwork> Network;

	TArray<int32> Route;
	std::vector<FGTPDrivePoint> Path;
	int32 CurrentLaneLength = 0;
	int Closest = 0;
	bool bAtDeadEnd = false;
	float StoppedTime = 0.f;
	FGTPDriveSettings Settings;
	FGTPJunctionClaims Junctions;
};

/**
 * The driving lanes of a map and the cars that drive on them (version 0.3:
 * a few cars with a non-player driver the player can pull out).
 *
 * Lanes come from Content/City/Traffic/<MapName>.lanes.csv (written by
 * tools/city/traffic.py, staged with the game) unless set on the actor. The
 * game mode spawns one network per map when that file exists. Car count:
 * -Traffic=N on the command line, else NumCars; -Traffic=0 turns it off.
 * Cars are spawned and taken away only out of sight, at least
 * MinSpawnDistance from the player.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPTrafficNetwork : public AActor
{
	GENERATED_BODY()

public:
	AGTPTrafficNetwork();

	virtual void Tick(float DeltaSeconds) override;

	const TArray<FGTPTrafficLane>& GetLanes() const { return Lanes; }

	/** Reads a lanes CSV (traffic.py format); returns the number of lanes. */
	int32 LoadLanesFromCsv(const FString& Path);

	/** Path of the lanes file for a map name, under the project content folder. */
	static FString LanesFileFor(const FString& MapName);

	/** Picks a random lane among Lane's next lanes, INDEX_NONE at a dead end. */
	int32 PickNext(int32 Lane);

	FGTPJunctionTable& GetJunctionTable() { return JunctionTable; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TArray<FGTPTrafficLane> Lanes;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	int32 NumCars = 6;

	/** Cars to pick from (Blueprint children of AGTPVehicle); empty = the game mode's test car. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic", meta = (MetaClass = "/Script/GranTheftPraline.GTPVehicle"))
	TArray<FSoftClassPath> VehicleClasses;

	/** Character put at the wheel; empty = the player's character class. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic", meta = (MetaClass = "/Script/Engine.Character"))
	FSoftClassPath DriverClass;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	float MinSpawnDistance = 4000.f;

	/** Cars farther than this from the player, out of sight, are moved closer. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	float MaxCarDistance = 25000.f;

	/** A car standing still this long (gridlock), out of sight, is taken away (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	float StuckSeconds = 20.f;

private:
	bool SpawnCar();
	bool FindSpawnPoint(int32& OutLane, int32& OutPoint, FTransform& OutTransform) const;
	bool IsSeenByPlayer(const FVector& Location) const;
	void Recycle(AGTPVehicle* Car);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGTPVehicle>> Cars;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AGTPVehicle>> LoadedVehicleClasses;

	UPROPERTY(Transient)
	TSubclassOf<ACharacter> LoadedDriverClass;

	FGTPJunctionTable JunctionTable;
	int32 WantedCars = 0;
	float NextSpawnTime = 0.f;
	FRandomStream Random;
};
