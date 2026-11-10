#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "GTPDriveLogic.h"
#include "GTPTraffic.h"
#include "GTPWantedLogic.h"
#include "GTPWanted.generated.h"

class AGTPVehicle;
class AGTPTrafficNetwork;
class UPointLightComponent;

/** Tag put on every police car, so the player's collisions and the HUD can tell them from traffic. */
namespace GTPPolice
{
	extern const FName PoliceTag;
}

/**
 * Wanted level of the player (docs/gameplay/wanted.md, ADR-0004). Lives on
 * the player controller. Crimes come in through ReportCrime (car theft and
 * collisions are reported by AGTPVehicle); the police director reads the
 * level and sends units. Each tick decides whether police can see the
 * player, which keeps or loses the level, and whether the player is arrested.
 */
UCLASS(ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPWantedComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPWantedComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Command line (docs/gameplay/wanted.md): -WantedStars=N sets level N
	 * five seconds after start (-WantedDelay=S changes the delay), -WantedHold
	 * keeps it (the police never lose the player), -Police=0 (or -NoPolice)
	 * turns the whole system off. While wanted, one "wanted:" line every 2 s gives the state
	 * for the soak logs.
	 */
	virtual void BeginPlay() override;

	/** False with -Police=0 or -NoPolice: no director, no stars, no hooks. */
	static bool IsPoliceEnabled();

	/** The wanted component of the first local player, null if there is none. */
	static UGTPWantedComponent* Find(const UObject* WorldContext);

	/** Reports a crime at Location. The police learn of it only if a witness could see: always true for now (every crime counts). */
	UFUNCTION(BlueprintCallable, Category = "Wanted")
	void ReportCrime(EGTPCrimeKind Kind, const FVector& Location);

	UFUNCTION(BlueprintPure, Category = "Wanted")
	int32 GetStars() const { return State.GetStars(); }

	/** 0 = clear, 1 = in pursuit, 2 = searching (stars flash). */
	UFUNCTION(BlueprintPure, Category = "Wanted")
	int32 GetPhaseIndex() const { return static_cast<int32>(State.GetPhase()); }

	UFUNCTION(BlueprintPure, Category = "Wanted")
	float GetSearchProgress() const { return State.GetSearchProgress(); }

	UFUNCTION(BlueprintPure, Category = "Wanted")
	float GetArrestProgress() const { return State.GetArrestProgress(); }

	/** Seconds left of the "arrested" banner, 0 when not shown. */
	UFUNCTION(BlueprintPure, Category = "Wanted")
	float GetBustedBannerTime() const { return BustedBannerLeft; }

	/** Where police last saw the player (the search zone centre). */
	const FVector& GetLastKnownLocation() const { return LastKnownLocation; }
	bool IsSeenNow() const { return bSeenNow; }
	EGTPWantedPhase GetPhase() const { return State.GetPhase(); }

	/** Sets the level directly (command line, tests). */
	void ForceStars(int32 Stars);

	/** Clears the level (arrest, debug). */
	UFUNCTION(BlueprintCallable, Category = "Wanted")
	void ClearWanted();

	/** True when a director can send units (a lanes network and a police car class exist). */
	void SetPoliceAvailable(bool bAvailable) { bPoliceAvailable = bAvailable; }

	/** Police units currently alive, registered by the director. */
	void RegisterPolice(AGTPVehicle* Car);
	void UnregisterPolice(AGTPVehicle* Car);
	const TArray<TWeakObjectPtr<AGTPVehicle>>& GetPolice() const { return Police; }

	/** Distance from which police see the player when nothing is in the way (cm). */
	UPROPERTY(EditAnywhere, Category = "Wanted")
	float SightRange = 6500.f;

	/** Police this close notice the player even behind cover (cm). */
	UPROPERTY(EditAnywhere, Category = "Wanted")
	float NoticeRange = 1200.f;

	/** Seconds the "arrested" banner stays and the player cannot move. */
	UPROPERTY(EditAnywhere, Category = "Wanted")
	float BustedSeconds = 3.f;

private:
	bool CanSeePlayer(const AGTPVehicle* Car, const FVector& PlayerLocation) const;
	void OnBusted();
	void EndBusted();

	FGTPWantedState State;
	TArray<TWeakObjectPtr<AGTPVehicle>> Police;
	FVector LastKnownLocation = FVector::ZeroVector;
	int32 ScriptedStars = 0;
	float ScriptedTime = -1.f;
	bool bHoldLevel = false;
	float NextTraceTime = 0.f;
	bool bSeenNow = false;
	bool bPoliceAvailable = false;
	float BustedBannerLeft = 0.f;
	FTimerHandle BustedTimer;
};

/**
 * Sends and drives the police (version 0.3 stage one). Spawns police cars
 * on lanes of the traffic network, out of sight and a few hundred metres at
 * most from the player, up to GTPWanted::UnitsForStars(stars, MaxUnits).
 * Cars chase straight at the player when they see them (stopping behind
 * whatever is ahead, with the same probe as traffic), otherwise follow the
 * lanes towards the last known position, then roam the lanes there. With the
 * level cleared, police leave the lanes and are taken away out of sight.
 *
 * Police car class: PoliceVehicleClass (a Blueprint child of AGTPVehicle with
 * a generic livery); empty = the traffic cars. A roof light bar of two
 * alternating lights is added in code, so every car reads as police.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPPoliceDirector : public AActor
{
	GENERATED_BODY()

public:
	AGTPPoliceDirector();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Police car (Blueprint child of AGTPVehicle); empty = the cars of the traffic network. */
	UPROPERTY(Config, EditAnywhere, Category = "Police", meta = (MetaClass = "/Script/GranTheftPraline.GTPVehicle"))
	FSoftClassPath PoliceVehicleClass;

	/** Most police cars alive at once, 2 until the 0.3 traffic cost is measured (frame budget on M1, ADR-0002 and ADR-0004). -PoliceMax=N overrides. */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	int32 MaxUnits = 2;

	/** Spawn ring around the player (cm): far enough to be out of sight, close enough to arrive. */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float MinSpawnDistance = 5000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float MaxSpawnDistance = 12000.f;

	/** Beyond this a car is moved to a new spawn point when out of sight (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float LeashDistance = 30000.f;

	/** Seconds between two spawns. */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float SpawnInterval = 2.f;

	/** Light bar position on the car, in the car's frame (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	FVector LightBarOffset = FVector(0.f, 0.f, 130.f);

	/** Police light bar blink period (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float LightPeriod = 0.5f;

private:
	struct FUnit
	{
		TWeakObjectPtr<AGTPVehicle> Car;
		TArray<TWeakObjectPtr<UPointLightComponent>> Lights;
		FGTPLaneFollower Follower;
		FGTPDriveSettings Settings;
		float StoppedTime = 0.f;
		bool bOnLanes = false;
		int8 LightState = -1;
	};

	bool SpawnUnit(const FVector& PlayerLocation);
	bool FindSpawnPoint(const FVector& PlayerLocation, int32& OutLane, int32& OutPoint, FTransform& OutTransform) const;
	void DriveUnit(FUnit& Unit, UGTPWantedComponent& Wanted, const AActor& Player, float DeltaSeconds);
	void DriveStraight(FUnit& Unit, const FVector& Target, float MaxSpeedKmh);
	void DriveOnLanes(FUnit& Unit, const FVector& Goal, bool bHasGoal, float MaxSpeedKmh);
	void AddLightBar(FUnit& Unit);
	void BlinkLights(FUnit& Unit, bool bOn, bool bPhase);
	bool HasLineOfSight(const AGTPVehicle* Car, const AActor& Player, const FVector& Target) const;
	void RemoveUnit(int32 Index);

	UPROPERTY(Transient)
	TObjectPtr<AGTPTrafficNetwork> Network;

	TArray<FUnit> Units;
	TSubclassOf<AGTPVehicle> LoadedPoliceClass;
	float NextSpawnTime = 0.f;
	FRandomStream Random;
};
