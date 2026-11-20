#pragma once

#include "CoreMinimal.h"
#include "GTPPoliceBrainLogic.h"
#include "GTPPoliceTactics.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "GTPChaseTally.h"
#include "GTPDriveLogic.h"
#include "GTPTraffic.h"
#include "GTPUnitRecovery.h"
#include "GTPWantedLogic.h"
#include "GTPWitnessLogic.h"
#include "GTPWanted.generated.h"

class ACharacter;
class AGTPVehicle;
class AGTPTrafficNetwork;
class UPointLightComponent;
class UGTPPoliceSenses;
class UGTPPoliceQueries;

/** Fired when the player is arrested (after the level was cleared). */
DECLARE_MULTICAST_DELEGATE(FGTPArrestedDelegate);

/** Fired once when police close in on a player who cannot get away: "Police! Don't move!" (sound, subtitle). */
DECLARE_MULTICAST_DELEGATE(FGTPArrestWarningDelegate);

/** Fired when the player dies (the level is cleared at the respawn, as in GTA: wasted). */
DECLARE_MULTICAST_DELEGATE(FGTPWastedDelegate);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** False with -Police=0 or -NoPolice: no director, no stars, no hooks. */
	static bool IsPoliceEnabled();

	/** The wanted component of the first local player, null if there is none. */
	static UGTPWantedComponent* Find(const UObject* WorldContext);

	/**
	 * Reports a crime at Location. The police learn of it only if a witness saw or heard it.
	 * Scene: the thing the crime is done to or in (the stolen car, the car that hit somebody). A witness's line of
	 * sight ends 150 cm above Location: inside the body of a tall car, which then hid its own theft from everybody
	 * (gate of candidate 50). The scene and the player's own pawn never cut that line.
	 */
	void ReportCrime(EGTPCrimeKind Kind, const FVector& Location, bool bVictimInvolved = false, const AActor* Scene = nullptr);

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

	/** Seconds left of the police warning on screen ("Police! Don't move!"), 0 when not shown. */
	UFUNCTION(BlueprintPure, Category = "Wanted")
	float GetWarningTime() const { return WarningLeft; }

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
	FGTPArrestWarningDelegate OnArrestWarning;
	FGTPWastedDelegate OnWasted;
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

	/** The police eyes and ears (AIPerception), set by the director; null with -PolicePerception=0. */
	void SetSenses(UGTPPoliceSenses* InSenses);

	/** Seconds since a unit last had the player in sight. */
	float GetSecondsSinceSeen() const;

	/** True for a while after the player fired, threatened or hit someone: two stars then shoot back. */
	bool IsPlayerAggressive() const;

	/** The police are told where the player is for DispatchSeconds (a scripted jump, a radio call). */
	void CallPolice() { DispatchLeft = DispatchSeconds; }

	/** Off: nobody can arrest the player (scripted demos that must show a chase first). */
	void SetArrestEnabled(bool bEnabled) { bArrestEnabled = bEnabled; }
	bool IsArrestEnabled() const { return bArrestEnabled; }

	/** How the police learned of the player's crimes since the start (the run of the packaged game reads it). */
	struct FCrimeReports
	{
		/** Nobody saw or heard: no effect. */
		int32 Unwitnessed = 0;
		/** A police unit saw or heard it: counted at once. */
		int32 ByPolice = 0;
		/** Somebody phoned (a passer-by, the victim): counted after the call. */
		int32 Called = 0;
		/** Calls that reached the police and raised the heat. */
		int32 CalledApplied = 0;
		/** The last call: people who saw or heard it, and whether the victim was one of the callers. */
		int32 LastCallPersons = 0;
		bool bLastCallVictim = false;
		/** The first call that counted: game time (s), stars just after, police units alive then. -1 = none yet. */
		float FirstCallTime = -1.f;
		int32 FirstCallStars = 0;
		int32 FirstCallUnits = 0;
	};
	const FCrimeReports& GetCrimeReports() const { return CrimeReports; }

	/** True when witnesses decide whether a crime counts (false with -Witnesses=0 or bRequireWitnesses off). */
	bool AreWitnessesRequired() const;

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

	/**
	 * Out of sight, the search runs at HiddenSearchRate times the speed when the player keeps still (below
	 * HiddenSpeedCmS), more than HiddenMinDistance from every police unit: hiding in a courtyard.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	float HiddenSearchRate = 2.f;
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	float HiddenSpeedCmS = 150.f;
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	float HiddenMinDistance = 2500.f;

	/** Changing car or leaving the car out of sight keeps only this share of the search time (0..1). */
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	float CarChangeKeep = 0.35f;

	/**
	 * After an arrest the player reappears at the point tagged GTPPoliceStation (a TargetPoint), else at
	 * ArrestReturnLocation when ArrestReturnEnabled, else at the player start; the police take the weapon
	 * and the ammunition, and the level is gone.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	bool bArrestTakesWeapon = true;

	/** Where the player is taken after an arrest (world cm); ignored while ArrestReturnEnabled is false (then: the player start). */
	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	bool ArrestReturnEnabled = false;

	UPROPERTY(Config, EditAnywhere, Category = "Wanted")
	FVector ArrestReturnLocation = FVector::ZeroVector;

	/** Seconds the "arrested" banner stays and the player cannot move. */
	UPROPERTY(EditAnywhere, Category = "Wanted")
	float BustedSeconds = 3.f;

private:
	bool CanSeePlayer(const AActor* Watcher, const FVector& PlayerLocation) const;
	/** True when the crime raised the heat (false: arrest banner up, or the same crime just counted). */
	bool ApplyCrime(EGTPCrimeKind Kind, const FVector& Location);
	void TickPendingReports(float Now);
	void OnBusted();
	void OnPlayerKilled();
	void OnPlayerRespawnedAfterDeath();
	void BindCombat();
	void TrackCarChange(const APawn* Pawn, bool bSeen);
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
	FCrimeReports CrimeReports;
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
	TWeakObjectPtr<const APawn> LastPlayerPawn;
	TWeakObjectPtr<UGTPPoliceSenses> Senses;
	float LastSeenTime = -1000.f;
	bool bArrestWarned = false;
	float WarningLeft = 0.f;
	bool bDeadNow = false;
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
	int32 MaxUnits = 4;

	/**
	 * Character put at the wheel of police cars; empty = the traffic's driver
	 * (the player's character: a camera system per spawn). Use a light
	 * character without camera to make spawning cheap.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Police", meta = (MetaClass = "/Script/Engine.Character"))
	FSoftClassPath PoliceDriverClass;

	/**
	 * Units are kept this long (s) after the level is lost, driving the lanes with lights off, and
	 * reused when a new level comes (they drive to it: no unit is ever moved by hand). 0 takes them away at once.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float IdleLifetime = 150.f;

	/** Spawn ring around the player (cm): far enough to be out of sight, close enough to arrive. */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float MinSpawnDistance = 5000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float MaxSpawnDistance = 12000.f;

	/** Beyond this a car is taken away when out of sight (cm), and another one is sent. */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float LeashDistance = 30000.f;

	/**
	 * A car sent to replace one that was lost (left behind, given up, wrecked) is never made nearer than this (cm):
	 * the first cars of a level arrive from the spawn ring, the next ones have to drive to the player, or the
	 * distance to the nearest unit says nothing of how the police drive (4 units were put back at 50 to 120 m
	 * from the player in the pursuit of candidate 49).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float ReplacementMinDistance = 15000.f;

	/** Seconds between two spawns. */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float SpawnInterval = 2.f;

	/** Light bar position on the car, in the car's frame (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	FVector LightBarOffset = FVector(0.f, 0.f, 130.f);

	/** With -VehicleLod=1 the light bar is off beyond this distance (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float LightBarDistance = 12000.f;

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
	float LiteLightDistance = 5000.f;

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
		/** Siren mode last set (EGTPSirenMode as int, -1 = never). */
		int8 SirenState = -1;
		/** Obstacle probe ignored until then (s): a unit stopped behind traffic pushes through (ramming). */
		float RamUntil = 0.f;
		float NextWakeTime = 0.f;
		/** A roadblock car: stands across the road, handbrake on, lights on (counts apart from the chasing cars). */
		bool bBlock = false;
		/** Seconds slower than 15 km/h with something close ahead: the unit shoves through it. */
		float SlowTime = 0.f;
		/** Tactical state (GTPPoliceBrainLogic.h): drives the siren, logged with the unit. */
		GTPPoliceBrain::EState Brain = GTPPoliceBrain::EState::Idle;
		float LastObstacle = -1.f;
		float LastThrottle = 0.f;
		float LastBrake = 0.f;
		float LastSteer = 0.f;
		/** Held against something: backs off. Off its wheels or held for good: given up (GTPUnitRecovery.h). */
		FGTPUnitRecovery Recovery;
		/** Stopped at the end of a dead end: the unit takes the lane back the other way, until it heads along TurnYaw (degrees). */
		bool bTurnAround = false;
		float TurnYaw = 0.f;
		/** The dead end it turned round in, kept out of its way until AvoidUntil (s). */
		int32 AvoidLane = INDEX_NONE;
		float AvoidUntil = 0.f;
	};

	/** Siren follows the lights: off without stars, yelp in pursuit, two-tone otherwise. */
	void UpdateSiren(FUnit& Unit, int32 Stars, bool bPursuit);

	/** The warning of an arrest: the nearest police car sounds its horn for half a second. */
	void OnArrestWarning();
	void StopWarningHorn();
	TWeakObjectPtr<AGTPVehicle> HornCar;
	FTimerHandle HornTimer;
	bool bWarningBound = false;

	bool SpawnUnit(const FVector& PlayerLocation, const FTransform* Placement = nullptr, bool bBlock = false);
	/** Two cars across the road ahead of a fast player (3 stars and more): one call puts one car of the pair. */
	bool SpawnRoadblockCar(const APawn& Player, int32 CarIndex);
	bool PlaceRoadblockCar(const APawn& Player, int32 CarIndex, const FVector& Point, const FVector& Along, double Distance);

public:
	/**
	 * The pursuit in numbers since the last reset (GTPChaseTally.h): speed of the chasing cars, distance to the
	 * player. Filled every frame while the player is wanted; roadblock cars are left out.
	 */
	const FGTPChaseTally& GetChaseTally() const { return ChaseTally; }
	const FGTPChaseTallySettings& GetChaseTallySettings() const { return ChaseTallySettings; }
	void ResetChaseTally() { ChaseTally.Reset(); }
	/** Cars chasing now (roadblock cars apart), and officers on foot. */
	void CountUnits(int32& OutChasing, int32& OutBlocks, int32& OutOfficers) const;
	/** Distance (m) from Where to the nearest chasing car, roadblock car and officer on foot; -1 for a kind with nobody. */
	void NearestUnits(const FVector& Where, float& OutChasing, float& OutBlock, float& OutOfficer) const;

	/**
	 * Lane places where a roadblock across the player's road would work: 140 to 240 m ahead of a player driving
	 * fast, on a road parallel to their travel. Geometry only; called by the EQS generator (UGTPPoliceQueries).
	 */
	void CollectRoadblockSites(const APawn& Player, TArray<FVector>& OutSites, int32 Level = 1) const;

private:
	/** Where the player is heading: their velocity, else the last direction they were driving in, else where the car points. */
	FVector2D GetTravelDirection(const APawn& Player) const;
	mutable FVector2D LastTravelDirection = FVector2D::ZeroVector;
	bool IsRoadblockSite(const APawn& Player, const FGTPTrafficLane& Lane, int32 Level, FVector& OutPoint, FVector& OutAlong, double& OutDistance) const;
	bool FindSpawnPoint(const FVector& PlayerLocation, int32& OutLane, int32& OutPoint, FTransform& OutTransform) const;
	/**
	 * True when a police car can be made at Place, on the ground the trace Ground found there. False when that trace
	 * stopped on a car or a person (the "ground" was a roof), or when one stands in the room the new car takes.
	 * Gate of candidate 51: a unit was made on a traffic car standing on its lane, came to rest against it on two
	 * wheels and stayed there 12 s at 0 km/h, which alone took the units' average under the floor.
	 * bRoadblock: the other police cars are not looked at (the two cars of a roadblock stand nose to tail).
	 * OutWhat: the name of what is in the way.
	 */
	bool IsSpawnPlaceClear(const FTransform& Place, const FHitResult& Ground, bool bRoadblock, FString& OutWhat) const;
	/** The line that says why no place was found for a car is written once in a while, not every try. */
	mutable double NextNoPlaceLog = 0.0;
	void DriveUnit(FUnit& Unit, UGTPWantedComponent& Wanted, const AActor& Player, float DeltaSeconds);
	void DriveStraight(FUnit& Unit, const FVector& Target, float MaxSpeedKmh);
	/** The pedals the driving decided, tamed by the steering (GTPUnitRecovery::Tame), given to the car. */
	void ApplyDrive(FUnit& Unit, const FGTPDriveCommand& Command, bool bHandbrake);
	/** One line when a unit backs off or is given up: where, how it lies, what is in front of it. */
	void TraceUnitTrouble(const FUnit& Unit, const TCHAR* What) const;
	void DriveOnLanes(FUnit& Unit, const FVector& Goal, bool bHasGoal, float MaxSpeedKmh);
	void AddLightBar(FUnit& Unit);
	void UpdateUnitDetail(AGTPVehicle* Car, float DistanceToView) const;
	bool SeatDriver(FUnit& Unit);
	void BlinkLights(FUnit& Unit, bool bOn, bool bPhase);
	bool HasLineOfSight(const AGTPVehicle* Car, const AActor& Player, const FVector& Target) const;
	void RemoveUnit(int32 Index);
	void ManageOfficers(UGTPWantedComponent& Wanted, const APawn& Player, int32 Stars, float Now, float DeltaSeconds);
	void RemoveOfficer(int32 Index, UGTPWantedComponent& Wanted);

	UPROPERTY(Transient)
	TObjectPtr<AGTPTrafficNetwork> Network;

	TArray<FUnit> Units;
	FGTPChaseTally ChaseTally;
	FGTPChaseTallySettings ChaseTallySettings;
	FGTPUnitRecoverySettings RecoverySettings;
	float NextTallyTraceTime = 0.f;
	/** Level-dependent behaviour of the current frame (GTPPoliceTactics::Profile). */
	GTPPoliceTactics::FProfile Profile;
	/** Spawn bias: when set, FindSpawnPoint picks a point ahead of the player's travel direction, farther away. */
	bool bAheadSpawn = false;
	FVector2D AheadDirection = FVector2D::ZeroVector;
	/** Spawn bias: when set, FindSpawnPoint picks a point between ReplacementMinDistance and the leash (a replacement). */
	bool bFarSpawn = false;
	/** Chasing cars sent since the level rose from nothing: past the number the level asks for, a car sent is a replacement. */
	int32 ResponseSent = 0;
	int32 LastStars = 0;
	float NextBlockTime = 0.f;
	UPROPERTY(Transient)
	TObjectPtr<UGTPPoliceSenses> Senses;

	/** EQS questions (search points for the officers); null with -PoliceEqs=0. */
	UPROPERTY(Transient)
	TObjectPtr<UGTPPoliceQueries> Queries;
	float NextSearchQueryTime = 0.f;
	float NextSiteQueryTime = 0.f;
	float PushedSearchPointsTime = -1000.f;

	/** Seconds the player has been (nearly) still in a car: officers get out to arrest them. */
	float PlayerStillSeconds = 0.f;
	TArray<TWeakObjectPtr<class AGTPPoliceOfficer>> Officers;
	GTPPoliceTactics::FSettings Tactics;
	float NextDeployTime = 0.f;
	float NextUnitTraceTime = 0.f;
	float NoStarsSeconds = 0.f;
	/** UPROPERTY so the garbage collector keeps the loaded Blueprint classes: a bare TSubclassOf member left them to be freed, and the next spawn read a dead class. */
	UPROPERTY(Transient)
	TSubclassOf<AGTPVehicle> LoadedPoliceClass;
	UPROPERTY(Transient)
	TSubclassOf<ACharacter> LoadedDriverClass;
	float NextSpawnTime = 0.f;
	FRandomStream Random;
};
