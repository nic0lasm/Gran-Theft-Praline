#pragma once

#include "CoreMinimal.h"
#include "GTPPoliceTactics.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "GTPDriveLogic.h"
#include "GTPTraffic.h"
#include "GTPWantedLogic.h"
#include "GTPWitnessLogic.h"
#include "GTPWanted.generated.h"

class ACharacter;
class AGTPVehicle;
class AGTPTrafficNetwork;
class UPointLightComponent;

/** Fired when the player is arrested (after the level was cleared). */
DECLARE_MULTICAST_DELEGATE(FGTPArrestedDelegate);

/** Fired when the stars or the phase change (Clear, Pursuit, Searching). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPWantedChangedDelegate, int32 /*Stars*/, EGTPWantedPhase /*Phase*/);

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
UCLASS(ClassGroup = (Gameplay), Config = Game, meta = (BlueprintSpawnableComponent))
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
	void ReportCrime(EGTPCrimeKind Kind, const FVector& Location, bool bVictimInvolved = false);

	/**
	 * Crimes count only when somebody saw or heard them (GTPWitnessLogic.h): police in sight
	 * report at once, a person in sight calls after a few seconds, shots and killings are
	 * heard. -Witnesses=0 makes every crime count at once (tests, demo scripts).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	bool bRequireWitnesses = true;

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

	/** Raises the level to at least Stars (1 to 5) with no crime attached; never lowers it. For missions. */
	void EnsureMinStars(int32 Stars);

	/** Native events for missions and UI; unbind when done. */
	FGTPArrestedDelegate OnArrested;
	FGTPWantedChangedDelegate OnWantedChanged;

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

	/** Officers on foot (AGTPPoliceOfficer): they see, witness and arrest like the cars. */
	void RegisterOfficer(AActor* Officer);
	void UnregisterOfficer(AActor* Officer);
	int32 GetOfficerCount() const { return Officers.Num(); }

	/** True for a while after the player fired, threatened or hit someone: two stars then shoot back. */
	bool IsPlayerAggressive() const;

	/** Off: nobody can arrest the player (scripted demos that must show a chase first). */
	void SetArrestEnabled(bool bEnabled) { bArrestEnabled = bEnabled; }

	/** Distance from which police see the player when nothing is in the way (cm). */
	UPROPERTY(EditAnywhere, Category = "Wanted")
	float SightRange = 6500.f;

	/**
	 * After the level rises (a crime, forced stars) the police are told where the player is for this
	 * long (s): witnesses and radio. The last known position follows the player and the level cannot
	 * drop, so the first units drive to the player, not to where the crime happened.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	float DispatchSeconds = 25.f;

	/** Police this close notice the player even behind cover (cm). */
	UPROPERTY(EditAnywhere, Category = "Wanted")
	float NoticeRange = 1200.f;

	/** Where the player is taken after an arrest (world cm); ignored while ArrestReturnEnabled is false (the player stays where they stand). */
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	bool ArrestReturnEnabled = false;

	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	FVector ArrestReturnLocation = FVector::ZeroVector;

	/** Seconds the "arrested" banner stays and the player cannot move. */
	UPROPERTY(EditAnywhere, Category = "Wanted")
	float BustedSeconds = 3.f;

private:
	bool CanSeePlayer(const AActor* Watcher, const FVector& PlayerLocation) const;
	void ApplyCrime(EGTPCrimeKind Kind, const FVector& Location);
	void TickPendingReports(float Now);
	void OnBusted();
	void NotifyIfChanged();
	void EndBusted();

	FGTPWantedState State;
	TArray<TWeakObjectPtr<AGTPVehicle>> Police;
	TArray<TWeakObjectPtr<AActor>> Officers;
	float LastAggressiveTime = -1000.f;
	float DispatchLeft = 0.f;
	bool bArrestEnabled = true;
	FVector LastKnownLocation = FVector::ZeroVector;

	/** Crimes seen by a person who is calling the police: reported when due. */
	struct FPendingReport
	{
		EGTPCrimeKind Kind = EGTPCrimeKind::HitVehicle;
		FVector Location = FVector::ZeroVector;
		float DueTime = 0.f;
	};
	TArray<FPendingReport> PendingReports;
	FGTPWitnessSettings WitnessSettings;
	int32 ScriptedStars = 0;
	float ScriptedTime = -1.f;
	bool bHoldLevel = false;
	float NextTraceTime = 0.f;
	bool bSeenNow = false;
	int32 LastNotifiedStars = 0;
	int32 LastNotifiedPhase = 0;
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

	/**
	 * Character put at the wheel of police cars; empty = the traffic's driver
	 * (the player's character: a camera system per spawn). Use a light
	 * character without camera to make spawning cheap.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Police", meta = (MetaClass = "/Script/Engine.Character"))
	FSoftClassPath PoliceDriverClass;

	/**
	 * Units are kept this long (s) after the level is lost, driving the lanes with lights off, and
	 * reused (moved out of sight) when a new level comes: creating a car and its driver costs a
	 * visible hitch. 0 takes them away at once.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float IdleLifetime = 150.f;

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

	/** With -VehicleLod=1 the light bar is off beyond this distance (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float LightBarDistance = 6000.f;

	/**
	 * Cheaper police for the M1 (-PoliceLite=1): one light per car (its colour alternates), lights
	 * only within LiteLightDistance, shadows and full detail only within LiteDetailDistance (cm),
	 * at most LiteMaxUnits cars unless -PoliceMax is given. Off by default until measured (A/B).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Cost")
	bool bPoliceLite = false;

	UPROPERTY(Config, EditAnywhere, Category = "Police|Cost")
	int32 LiteMaxUnits = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Police|Cost")
	float LiteLightDistance = 3000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Police|Cost")
	float LiteDetailDistance = 1500.f;

	/**
	 * Officers on foot (AGTPPoliceOfficer): at most this many at once (-PoliceOfficers=N, 0 = none,
	 * the cars alone). They get out of a car when the player is on foot nearby, or the car is stuck.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Officers")
	int32 MaxOfficers = 3;

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
		/** Creation stages spread over frames: 1 = driver to spawn, 2 = light bar to add, 0 = ready. */
		int8 Stage = 0;
		/** Seconds without a wanted level (an idle unit is kept for IdleLifetime, then taken away). */
		float IdleTime = 0.f;
		float NextRepositionTime = 0.f;
		/** Siren mode last set (EGTPSirenMode as int, -1 = never). */
		int8 SirenState = -1;
		/** Obstacle probe ignored until then (s): a unit stopped behind traffic pushes through (ramming). */
		float RamUntil = 0.f;
		float NextWakeTime = 0.f;
		float LastObstacle = -1.f;
		float LastThrottle = 0.f;
		float LastBrake = 0.f;
	};

	/** Siren follows the lights: off without stars, yelp in pursuit, two-tone otherwise. */
	void UpdateSiren(FUnit& Unit, int32 Stars, bool bPursuit);

	bool SpawnUnit(const FVector& PlayerLocation);
	bool FindSpawnPoint(const FVector& PlayerLocation, int32& OutLane, int32& OutPoint, FTransform& OutTransform) const;
	void DriveUnit(FUnit& Unit, UGTPWantedComponent& Wanted, const AActor& Player, float DeltaSeconds);
	void DriveStraight(FUnit& Unit, const FVector& Target, float MaxSpeedKmh);
	void DriveOnLanes(FUnit& Unit, const FVector& Goal, bool bHasGoal, float MaxSpeedKmh);
	void AddLightBar(FUnit& Unit);
	void UpdateUnitDetail(AGTPVehicle* Car, float DistanceToView) const;
	bool SeatDriver(FUnit& Unit);
	void RepositionUnit(FUnit& Unit, const FVector& PlayerLocation);
	void BlinkLights(FUnit& Unit, bool bOn, bool bPhase);
	bool HasLineOfSight(const AGTPVehicle* Car, const AActor& Player, const FVector& Target) const;
	void RemoveUnit(int32 Index);
	void ManageOfficers(UGTPWantedComponent& Wanted, const APawn& Player, int32 Stars, float Now, float DeltaSeconds);
	void RemoveOfficer(int32 Index, UGTPWantedComponent& Wanted);

	UPROPERTY(Transient)
	TObjectPtr<AGTPTrafficNetwork> Network;

	TArray<FUnit> Units;
	TArray<TWeakObjectPtr<class AGTPPoliceOfficer>> Officers;
	GTPPoliceTactics::FSettings Tactics;
	float NextDeployTime = 0.f;
	float NextUnitTraceTime = 0.f;
	float NoStarsSeconds = 0.f;
	TSubclassOf<AGTPVehicle> LoadedPoliceClass;
	TSubclassOf<ACharacter> LoadedDriverClass;
	float NextSpawnTime = 0.f;
	FRandomStream Random;
};
