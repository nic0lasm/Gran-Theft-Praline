#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/SoftObjectPath.h"
#include "GTPDriveLogic.h"
#include "GTPTrafficLights.h"
#include "GTPTraffic.generated.h"

class ACharacter;
class AGTPVehicle;
class AGTPTrafficNetwork;
class AGTPFarTraffic;

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
 * Follows lanes of a network for a driver that is not an AI car of that
 * network (the demo route driving the player's car): same decisions as the
 * AI drivers, no junction claims. Plain C++, owned by its user.
 */
struct GRANTHEFTPRALINE_API FGTPLaneFollower
{
	/** Starts on the lane closest to Location heading like Yaw (degrees); false if none within MaxDistanceCm. */
	bool StartNear(AGTPTrafficNetwork* InNetwork, const FVector& Location, float YawDegrees, float MaxDistanceCm);

	/** One decision; false when the route ended (dead end) or there is none. */
	bool Step(const FVector& Location, float YawDegrees, float SpeedKmh, float MaxSpeedKmh, float ObstacleMetres, FGTPDriveCommand& Out);

	bool IsActive() const { return Network.IsValid() && Route.Num() > 0; }

	/** From now on, junction choices lead towards Goal (world cm) instead of being random. */
	void SetGoal(const FVector& InGoal) { Goal = InGoal; bHasGoal = true; }
	void ClearGoal() { bHasGoal = false; }

private:
	void Plan();

	FVector Goal = FVector::ZeroVector;
	bool bHasGoal = false;

	TWeakObjectPtr<AGTPTrafficNetwork> Network;
	TArray<int32> Route;
	std::vector<FGTPDrivePoint> Path;
	int32 CurrentLaneLength = 0;
	int Closest = 0;
	FGTPDriveSettings Settings;
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

	/** Lane after the current one on the planned route, INDEX_NONE if none. */
	int32 GetNextLane() const { return Route.Num() > 1 ? Route[1] : INDEX_NONE; }

	/** Index of the lane point the car is nearest to, on its current lane. */
	int32 GetLanePoint() const { return Closest; }

	/** Free distance in front of Vehicle (m) to the nearest car or person, negative if none within LengthCm. */
	static float ProbeAhead(const AGTPVehicle* Vehicle, float LengthCm, float RadiusCm);

	/** Seconds the car has been standing still. */
	float GetStoppedTime() const { return StoppedTime; }

	/**
	 * Seconds between two decisions (0 = every frame). The pedals keep their
	 * last value in between, so far cars can decide 4 to 10 times a second
	 * (-VehicleLod=1, docs/performance/vehicle-cost.md).
	 */
	void SetDecisionInterval(float Seconds);

	/** Gives back the junctions this car holds (car taken away or stolen). */
	void ReleaseJunctions();

	/**
	 * Ghost mode (-VehicleGhost=1, docs/performance/vehicle-cost.md): physics
	 * and the Chaos vehicle off, the car slides along its path under the same
	 * stops as a driven car (GTPTrafficGhost.h). Off again: physics back on at
	 * the ghost's speed.
	 */
	void SetGhost(bool bInGhost);
	bool IsGhost() const { return bGhost; }

	/** Ghost speed (m/s). */
	float GetGhostSpeed() const { return GhostSpeed; }

	/**
	 * Moves the car, as a ghost, to point Point of Lane at Location (cm) facing
	 * YawDegrees, rolling at SpeedMs: a far car handed over to this driven car
	 * (-FarTraffic, GTPFarTraffic.h).
	 */
	void PlaceGhost(int32 Lane, int32 Point, const FVector& Location, float YawDegrees, float SpeedMs);

	/** Time spent in the ticks of all AI drivers (seconds) and their number, since the last read. */
	static double TickSeconds;
	static int32 TickCount;

	/** Length of the obstacle probe ahead (cm). */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float ProbeLength = 4000.f;

	/** Half width of the obstacle probe (cm): a lane is about 3 m. */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float ProbeRadius = 90.f;

private:
	void PlanRoute();
	float FindObstacleMetres(const AGTPVehicle* Vehicle) const;
	/** Ground height under Location (cm), false if nothing within 10 m below. */
	bool FindGround(const AGTPVehicle* Vehicle, const FVector& Location, float& OutZ) const;

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

	bool bGhost = false;
	float GhostSpeed = 0.f;      // m/s
	float GhostAlong = 0.f;      // m past path point Closest
	float GhostRideHeight = 40.f; // cm from the ground to the actor origin
	float GhostGroundZ = 0.f;    // last ground height found (cm)
	float GhostGroundAge = 1.f;  // seconds since that ground trace
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

	/** Next lane of Lane whose end is closest to Target (world cm); INDEX_NONE at a dead end. */
	int32 PickNextToward(int32 Lane, const FVector& Target) const;

	FGTPJunctionTable& GetJunctionTable() { return JunctionTable; }

	/** Traffic lights of the crossings (version 0.4); empty with -TrafficLights=0. */
	const FGTPTrafficLights& GetTrafficLights() const { return Lights; }

	/** Clock of the lights (seconds). */
	float GetLightTime() const;

	/**
	 * Pedestrian crossings: may someone walk across the road at Location now,
	 * heading WalkDirection? True away from any signalled junction (JunctionRadiusCm),
	 * else true only while the roads being crossed have a red light.
	 */
	bool IsWalkAllowed(const FVector& Location, const FVector& WalkDirection, float JunctionRadiusCm = 2500.f) const;

	/**
	 * Free distance (m) along the lanes from Driver to the next AI car on its
	 * current or next lane, negative if none within MaxMetres. Sees queues on
	 * bends that a straight probe misses.
	 */
	float GapAlongLanes(const UGTPAIDriverComponent* Driver, float MaxMetres) const;

	/** Lane and point closest to Location whose direction is within 60 degrees of YawDegrees. */
	bool FindNearestLane(const FVector& Location, float YawDegrees, float MaxDistanceCm, int32& OutLane, int32& OutPoint) const;

	/**
	 * A point to drive to in order to join the nearest lane, whatever its
	 * direction: AheadCm further along that lane, so the car meets it at an
	 * angle and can then follow it. False when no lane is within MaxDistanceCm.
	 */
	bool FindJoinPoint(const FVector& Location, float MaxDistanceCm, float AheadCm, FVector& OutPoint) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TArray<FGTPTrafficLane> Lanes;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	int32 NumCars = 3;

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

	/**
	 * Cheaper AI cars (frame budget, ADR-0002): beyond DetailDistance a car casts no
	 * shadow (moving shadow casters redraw virtual shadow map pages every frame),
	 * its lights cast no shadow at all, and it is drawn from FarLOD (0-based).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float DetailDistance = 2500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	int32 FarLOD = 2;

	/** Beyond this distance an AI car is not drawn at all (cm, 0 = always drawn). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float DrawDistance = 15000.f;

	/**
	 * Distance tiers for the cost of AI cars, only with -VehicleLod=1 (or
	 * bVehicleLod): the driver decides every frame under LodNearDistance, 10
	 * times a second under LodMidDistance, 4 times a second beyond; the body
	 * pose is only refreshed while the car is drawn.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bVehicleLod = false;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float LodNearDistance = 3500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float LodMidDistance = 9000.f;

	/**
	 * Ghost mode of far AI cars, only with -VehicleGhost=1 (or bVehicleGhost):
	 * a car turns into a ghost past GhostDistance from the player (cm) and is
	 * driven again inside GhostWakeDistance. Ghosts move 20 times a second.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bVehicleGhost = false;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float GhostDistance = 9000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float GhostWakeDistance = 7000.f;

	/** Reuse the cars taken away instead of destroying and spawning new ones (-TrafficPool=1): no spawn hitch. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bTrafficPool = false;

	/** Parked cars along the kerbs as static instances (-ParkedCars=1, AGTPParkedCars, tools/city/parking.py). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bParkedCars = false;

	/**
	 * Far traffic (-FarTraffic=N, AGTPFarTraffic): N more cars past the ghost
	 * distance, drawn as moving instances with no actor. 0 = none. Needs the
	 * ghost mode, turned on with it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	int32 NumFarCars = 0;

	/** A car standing still this long (gridlock), out of sight, is taken away (seconds); longer than a red light and its queue. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	float StuckSeconds = 35.f;

	/** Traffic lights at the crossings (off: -TrafficLights=0). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	bool bTrafficLights = true;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float GreenSeconds = 12.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float AmberSeconds = 3.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float AllRedSeconds = 2.f;

	/**
	 * Stand-in signal heads until the street furniture model exists: one small
	 * coloured sphere on a thin pole at each stop line, no shadow, no collision.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	bool bShowSignalHeads = true;

	/** Signal heads farther than this from the camera are hidden (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalDrawDistance = 6000.f;

private:
	friend class AGTPPoliceDirector;
	friend class AGTPFarTraffic;
	bool SpawnCar();
	/** A new car with its AI driver (registered, not started) at Transform; nullptr if it could not spawn. */
	AGTPVehicle* CreateCar(const FTransform& Transform, ESpawnActorCollisionHandlingMethod Collision);
	bool FindSpawnPoint(int32& OutLane, int32& OutPoint, FTransform& OutTransform) const;
	bool IsSeenByPlayer(const FVector& Location) const;
	void Recycle(AGTPVehicle* Car);
	void MakeCheap(AGTPVehicle* Car, ACharacter* Driver) const;
	void UpdateDetail(AGTPVehicle* Car, float DistanceToView) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGTPVehicle>> Cars;

	/** Cars taken away and kept hidden for the next spawn (-TrafficPool=1): no spawn, no destroy. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGTPVehicle>> Pool;

	/** Puts Car away into Pool; false if it cannot be reused (wrecked, no driver). */
	bool PutAway(AGTPVehicle* Car);
	/** A pooled car at Transform, ready to drive; nullptr if the pool is empty. */
	AGTPVehicle* TakeFromPool(const FTransform& Transform);

	UPROPERTY(Transient)
	TArray<TSubclassOf<AGTPVehicle>> LoadedVehicleClasses;

	UPROPERTY(Transient)
	TSubclassOf<ACharacter> LoadedDriverClass;

	void BuildLights();
	void UpdateSignalHeads();

	struct FSignalHead
	{
		int32 Lane = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		EGTPLight Shown = EGTPLight::None;
		TWeakObjectPtr<UStaticMeshComponent> Lamp;
		TWeakObjectPtr<UStaticMeshComponent> Pole;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
	};

	FGTPJunctionTable JunctionTable;
	FGTPTrafficLights Lights;
	TArray<FSignalHead> SignalHeads;
	/** AI drivers of the cars spawned here (stolen ones stop ticking), for GapAlongLanes. */
	TArray<TWeakObjectPtr<UGTPAIDriverComponent>> AIDrivers;
	/** Centre of each signalled junction (mean of its stop lines), by junction id. */
	TMap<int32, FVector> JunctionCentres;
	/** Heading (radians) of group 0 at each signalled junction. */
	TMap<int32, float> JunctionAxis;
	int32 WantedCars = 0;
	float NextSpawnTime = 0.f;
	/** Far traffic drawn as instances (-FarTraffic=N), spawned by BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<AGTPFarTraffic> FarTraffic;
	/** CPU budget log: time spent in Tick (seconds) and ticks since the last line. */
	double NetworkTickSeconds = 0.0;
	int32 BudgetTicks = 0;
	uint64 BudgetFrame = 0;
	void LogBudget();
	FRandomStream Random;
};
