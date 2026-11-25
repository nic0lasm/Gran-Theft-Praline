#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "WheeledVehiclePawn.h"
#include "GTPCarjackLogic.h"
#include "GTPCameraProbe.h"
#include "GTPVehicle.generated.h"

class APlayerController;

class AController;
class ACharacter;
class UAnimMontage;
class UCameraComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UChaosWheeledVehicleMovementComponent;
class UGTPEngineSoundComponent;
class UPrimitiveComponent;
class USpringArmComponent;
struct FInputActionValue;

/** Side of the car, seen from the driver's seat. France drives on the right: the driver sits on the left. */
/** Why a car is lost to the game (missions, ADR-0014). */
UENUM(BlueprintType)
enum class EGTPVehicleLoss : uint8
{
	Wrecked,
	Drowned,
	Destroyed
};

class AGTPVehicle;
/** Any car of the world lost: wrecked (health 0), drowned, or its actor destroyed. Native, no cost without listeners. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPVehicleWrecked, AGTPVehicle* /*Vehicle*/, EGTPVehicleLoss /*Reason*/);

UENUM(BlueprintType)
enum class EGTPVehicleSide : uint8
{
	Left,
	Right
};

/** How the player gets in, chosen from the state of the car when the enter key is pressed. */
UENUM(BlueprintType)
enum class EGTPEntryKind : uint8
{
	/** Unlocked, empty: open the door and sit. */
	OpenDoor,
	/** Locked, empty: break the side window, then open from inside. */
	BreakWindow,
	/** Someone at the wheel: open the door and pull the driver out. */
	Carjack
};

UENUM(BlueprintType)
enum class EGTPVehicleState : uint8
{
	Parked,
	Entering,
	Driven,
	Exiting,
	Wrecked
};

/** Damage zones, each driving one morph target on the body mesh. */
UENUM(BlueprintType)
enum class EGTPDamageZone : uint8
{
	Front,
	Rear,
	Left,
	Right
};

/**
 * Drivable car: Chaos Vehicles physics, GTA-style enter and exit, chase and
 * interior cameras, simple damage (dents through morph targets, breakable
 * glass, wrecked state).
 *
 * The C++ class carries the behaviour and the default tuning of a French
 * city car (about 1,100 kg, front-wheel drive). Each model is a Blueprint
 * child that sets the skeletal mesh, its animation Blueprint (wheel
 * controller), the glass components and, if needed, its own tuning.
 *
 * Mesh conventions (docs/gameplay/vehicles.md, section 9):
 * - wheel bones Phys_Wheel_FL, Phys_Wheel_FR, Phys_Wheel_BL, Phys_Wheel_BR (else found by
 *   name and position, see FindWheelBones);
 * - socket seat_driver (falls back to DriverSeatOffset);
 * - morph targets Dent_Front, Dent_Rear, Dent_Left, Dent_Right;
 * - glass as separate components tagged "Glass" plus one zone tag
 *   (Front, Rear, Left, Right).
 *
 * Driving input comes from the player controller's vehicle input set
 * (UGTPVehicleInput), built in code: no input asset is needed.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPVehicle : public AWheeledVehiclePawn
{
	GENERATED_BODY()

public:
	AGTPVehicle(const FObjectInitializer& ObjectInitializer);

	// --- Enter and exit -------------------------------------------------

	/** True when Character may start getting in now. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seats")
	bool CanEnter(const ACharacter* Character) const;

	UFUNCTION(BlueprintPure, Category = "Vehicle|Seats")
	EGTPEntryKind GetEntryKind() const;

	/** World position where a character stands to open this door. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seats")
	FVector GetDoorLocation(EGTPVehicleSide Side) const;

	/**
	 * Starts the entry sequence: Character walks to the driver door, opens
	 * it (breaking the window or pulling the driver out if needed), sits,
	 * then DriverController possesses the car. Returns false if the car cannot be
	 * entered now.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Seats")
	bool BeginEnter(ACharacter* Character, AController* DriverController);

	/** Asks the driver to get out: stops first, or bails out above BailOutSpeedKmh. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Seats")
	void RequestExit();

	/**
	 * A non-player driver threatened at gunpoint: gets out (stopping first, or bailing
	 * out when fast) and runs away from ThreatLocation once outside.
	 */
	void PanicExit(const FVector& ThreatLocation);

	/** True once after the player's car (or the player) hit this car. */
	bool ConsumeHitByPlayer()
	{
		const bool bHit = bHitByPlayer;
		bHitByPlayer = false;
		return bHit;
	}

	/**
	 * Takes whoever sits at the wheel out at once and puts them at Location
	 * (river recovery: the car sank). The player controller gets its character
	 * back. Returns the character, or null when nobody was inside.
	 */
	ACharacter* EjectDriverTo(const FVector& Location, const FRotator& Rotation);

	/** Car fell into water: engine dead, wrecked, no more input. */
	void Drown();

	/**
	 * Marks a teleport or a physics switch by the traffic or the police (How: a literal). AI cars
	 * take no impact damage for a moment after it: the solver pushes a body out of whatever it
	 * was placed into, and that push read as a 300 km/h crash.
	 */
	void NotePlaced(const TCHAR* How);

	/** The last NotePlaced: how (a literal) and when (world seconds). */
	const TCHAR* GetLastPlacedHow() const { return LastPlacedHow; }

	/**
	 * A script holds the wheel and has just given its steering to the movement component (the filmed flight, a test
	 * drive): for half a second the driving aids leave the steering alone (UpdateDriveAssist, the pull of the dents).
	 */
	void NoteScriptedSteering();

	/** What the dents add to the player's steering (-1 to 1, negative to the left): 0 with no side beaten more than the other. */
	float GetDentPull() const;
	float GetLastPlacedTime() const { return LastPlacedTime; }

	/** Seats a non-player driver (traffic, tests) without any sequence. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Seats")
	void SeatOccupant(ACharacter* Character);

	/**
	 * Seated non-player driver made lite (-DriverLite=0 keeps it whole): no actor tick, no
	 * movement, camera or controller tick, cameras and spring arms off; only the meshes keep
	 * animating. Undone when the character leaves the car (ReleaseCharacter). Police and
	 * traffic both seat through SeatOccupant, so both get it.
	 */
	static void SetDriverLite(ACharacter* Character, bool bLite);
	static bool IsDriverLiteWanted();

	UFUNCTION(BlueprintPure, Category = "Vehicle|Seats")
	ACharacter* GetDriver() const { return Driver; }

	/** Wrecked is reported for a destroyed car nobody sits in. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	EGTPVehicleState GetVehicleState() const { return (bWrecked && State == EGTPVehicleState::Parked) ? EGTPVehicleState::Wrecked : State; }

	UFUNCTION(BlueprintPure, Category = "Vehicle|Damage")
	bool IsWrecked() const { return bWrecked; }

	/**
	 * Fired once per car when it is wrecked or drowned, and when its actor is destroyed
	 * (traffic recycling included): filter on the actor's tags. Static, so a listener needs
	 * no car to bind to: AGTPVehicle::OnVehicleWrecked().AddUObject(this, &UMine::OnLost).
	 */
	static FGTPVehicleWrecked& OnVehicleWrecked();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Signed forward speed, in km/h. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetSpeedKmh() const;

	/** Engine speed reported by Chaos, in rpm. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetEngineRpm() const;

	/** Current gear: 0 neutral, negative reverse. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	int32 GetGear() const;

	/** Throttle applied by whoever drives (player or AI), 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	float GetThrottleAmount() const { return ThrottleAmount; }

	/** True while someone sits at the wheel of a car that is not wrecked, once the engine has started. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsEngineRunning() const;

	/**
	 * A non-player driver brakes and stays put for Seconds: the player walked up to the
	 * car to take it (UGTPVehicleInteractionComponent). Read by the traffic driver.
	 */
	void RequestAIStop(float Seconds);
	bool IsAIStopRequested() const;

	/**
	 * Driving inputs for a non-player driver (UGTPAIDriverComponent).
	 * Ignored while the player drives or once the car is wrecked.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetAIDriveInput(float Throttle, float Brake, float Steering, bool bHandbrake, bool bReverse = false);

	/**
	 * Same pedals as SetAIDriveInput for the player's own car driven by a test tool
	 * (-TestDrive physics mode, GTPTestDrive.h): keeps ThrottleAmount (engine sound,
	 * load) in step. Does nothing on a wrecked car.
	 */
	void SetScriptedDriveInput(float Throttle, float Brake, float Steering, bool bHandbrake);

	/** True when a non-player character sits at the wheel. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seats")
	bool HasAIDriver() const { return Driver != nullptr && State != EGTPVehicleState::Driven && State != EGTPVehicleState::Exiting; }

	/**
	 * The non-player driver stops and gets out (bailing out when fast). bFlee: then runs from
	 * From. The car is no longer traffic once they are out (crash, threat).
	 */
	void AIGetOut(bool bFlee, const FVector& From);

	/** Alarm: the horn pulses for AlarmSeconds when a car with bAlarm is broken into or hit while parked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Seats")
	bool bAlarm = false;

	void TriggerAlarm();

	/** A parked car far from the player: physics and the Chaos vehicle off; back on when the player comes near. */
	void SetParkedAsleep(bool bAsleep);
	bool IsParkedAsleep() const { return bParkedAsleep; }

	/**
	 * This actor is a MassTraffic car shown near the player (GTPMass, -MassTrafficBodies): Mass moves it along its lane
	 * every frame, as Epic's LowResSpawnedActor, so its body follows without simulating, the parked sleep for good (a
	 * car that hits it is stopped as by a wall), and nobody gets in. One way: the actor stays MassTraffic's. Called every
	 * frame the car is shown (UGTPMassTrafficBodyProcessor): after the first call it only keeps it kinematic.
	 */
	void SetShownByMassTraffic();
	bool IsShownByMassTraffic() const { return bShownByMassTraffic; }

	// --- Camera ---------------------------------------------------------

	/** Interior (first person) view instead of the chase camera. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Camera")
	void SetInteriorView(bool bEnable);

	UFUNCTION(BlueprintPure, Category = "Vehicle|Camera")
	bool IsInteriorView() const { return bInteriorView; }

	// --- Damage ---------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Vehicle|Damage")
	float GetHealth() const { return Health; }

	/** Health as a share of MaxHealth, 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Damage")
	float GetHealthRatio() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	/** Adds damage to one zone (dents, glass) and to the health of the car. */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
	void ApplyZoneDamage(EGTPDamageZone Zone, float Amount);

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Locked cars are entered by breaking a side window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Seats")
	bool bLocked = false;

protected:
	virtual void PreRegisterAllComponents() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	// Components ---------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<USpringArmComponent> ChaseArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<UCameraComponent> ChaseCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	TObjectPtr<UCameraComponent> InteriorCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Sound")
	TObjectPtr<UGTPEngineSoundComponent> EngineSound;

	// Seats and doors ----------------------------------------------------

	/** Side of the driver's door. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	EGTPVehicleSide DriverSide = EGTPVehicleSide::Left;

	/** Where a character stands to open the left door, in actor space (cm). The right door mirrors it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	FVector LeftDoorOffset = FVector(10.f, -150.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	FName DriverSeatSocket = TEXT("seat_driver");

	/** Used when the mesh has no DriverSeatSocket, in actor space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	FVector DriverSeatOffset = FVector(-20.f, -38.f, 40.f);

	/** The driver stays visible once a seated pose exists for our character; hidden until then. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	bool bShowDriverInside = false;

	/** Optional montages played on the character (left-side versions; right side is mirrored later). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	TSoftObjectPtr<UAnimMontage> EnterMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	TSoftObjectPtr<UAnimMontage> BreakWindowMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	TSoftObjectPtr<UAnimMontage> CarjackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seats")
	TSoftObjectPtr<UAnimMontage> ExitMontage;

	/** Duration of each entry, used when no montage is set (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float EnterDuration = 0.9f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float BreakWindowDuration = 1.6f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float CarjackDuration = 2.2f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float ExitDuration = 0.7f;

	/** Above this speed the driver jumps out instead of stopping first (km/h). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float BailOutSpeedKmh = 40.f;

	/** Below this speed the car counts as stopped for a normal exit (km/h). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float StoppedSpeedKmh = 4.f;

	/** Above this speed nobody can get in (km/h). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float MaxEntrySpeedKmh = 12.f;

	/** Starting the engine of a car that was not running, and hot-wiring one entered through a broken window (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float EngineStartSeconds = 0.7f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float HotwireSeconds = 1.4f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float AlarmSeconds = 15.f;

	/** How far a driver pulled out of the car runs off (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float VictimFleeDistance = 2500.f;

	/** Sideways launch speed of a bail out (cm/s). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Seats")
	float BailOutLaunchSpeed = 450.f;

	// Camera -------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ChaseDistance = 620.f;

	/** Height of the chase arm pivot above the mesh origin (cm). The car's
	 *  origin is on the ground: a pivot inside the body keeps the arm's
	 *  collision probe from hitting the road and pulling the camera under the car. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ChasePivotHeight = 100.f;

	/** Camera height above the pivot, at the end of the arm (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ChaseHeight = 70.f;

	/** Radius of the chase camera's collision probe (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ChaseProbeRadius = 20.f;

	/** Props under this bounds radius (cm) never pull the chase camera in (poles, bins, signs). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ChaseSmallObjectRadius = 150.f;

	/** Squeezed to nothing, the chase camera rises this much (cm) above the car, as the ceiling allows. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ChaseSqueezeLift = 160.f;

	/** People and small props between the chase camera and the car are hidden while in the way. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	bool bChaseHideOccluders = true;

	/** Pitch the camera returns to when recentring (degrees). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ChasePitch = -10.f;

	/** Seconds without look input before the camera swings back behind the car. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float RecenterDelay = 1.5f;

	/** Recentring speed (interpolation speed, 1/s). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float RecenterSpeed = 2.5f;

	/** No recentring below this speed, so the player can look around a parked car (km/h). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float RecenterMinSpeedKmh = 10.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float BaseFOV = 75.f;

	/** FOV reached at FOVMaxSpeedKmh, for a sense of speed. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float MaxSpeedFOV = 88.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float FOVMaxSpeedKmh = 130.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float InteriorFOV = 85.f;

	/** The chase camera pulls back this much more at FOVMaxSpeedKmh (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ChaseSpeedExtraDistance = 150.f;

	/** Share of the way the recentring camera turns towards where the car is going rather than where it points (slides). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float VelocityLookWeight = 0.6f;

	/** Camera jolt of the hardest impact (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Camera")
	float ImpactShakeCm = 22.f;

	/**
	 * Arcade help for the player's car (GTPCarjackLogic.h): torque, handbrake turns, stability, damage. PlayerTorque is
	 * for 1 150 kg, as much per kilogram for a heavier car (BeginPlay).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Driving")
	float PlayerTorque = 300.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Driving")
	float DriftRearGrip = 0.55f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Driving")
	float DriftYawAccel = 160.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Driving")
	float StabilityDamping = 1.6f;

	/** Driver eye position for the interior view, in actor space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Camera")
	FVector InteriorEyeOffset = FVector(-25.f, -38.f, 115.f);

	// Damage -------------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Damage")
	float MaxHealth = 1000.f;

	/** Impacts below this change of speed do nothing (cm/s, about 11 km/h). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Damage")
	float MinImpactSpeed = 300.f;

	/** Damage per cm/s of speed change above MinImpactSpeed. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Damage")
	float DamagePerImpactSpeed = 0.12f;

	/** Zone damage at which a zone's dent morph is fully applied. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Damage")
	float FullDentDamage = 400.f;

	/** Zone damage at which the glass of that zone breaks. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Damage")
	float GlassBreakDamage = 120.f;

	/** Below this share of health the engine smokes (smoke FX comes with the art pass). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicle|Damage")
	float SmokeHealthRatio = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Damage")
	TMap<EGTPDamageZone, FName> DentMorphTargets;

	/**
	 * Glass panels of the car, one static mesh per zone, modelled in the
	 * body's space (tools/vehicles/prepare_vehicle.py). Each becomes a
	 * component on the root bone, tagged "Glass" plus the zone.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Damage")
	TMap<EGTPDamageZone, TObjectPtr<UStaticMesh>> GlassMeshes;

	/**
	 * City Sample cars (tools/vehicles/unreal/make_epic_vehicles.py): the meshes that draw the car. Their
	 * body mesh, which carries the physics and the wheel bones, is City Sample's rig, with no geometry:
	 * alone, the car is not drawn (gate 67: the stolen Car05 driven invisible for a minute). Each one
	 * becomes a component on the body, posed by it (leader pose), without collision: the exterior (paint,
	 * tyres, windscreen), then the glass and lights. Empty on our own cars, drawn by their body.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Look")
	TArray<TObjectPtr<USkeletalMesh>> LookMeshes;

public:
	/** The components built from LookMeshes (rebuilt on each construction). */
	const TArray<TObjectPtr<USkeletalMeshComponent>>& GetLookComponents() const { return LookComponents; }

	/**
	 * Whether a car class draws anything a player sees as a car, read from its defaults (no spawn): its
	 * body mesh or one of its LookMeshes at least 1 m long. OutWhy: each mesh with its box (cm) and its
	 * material slots, how many have a material.
	 */
	static bool IsClassDrawn(const UClass* Class, FString& OutWhy);

protected:

	/** Blueprint hook: dents, glass, smoke and wreck changes, for FX and sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle|Damage")
	void OnDamageChanged(EGTPDamageZone Zone, float AccumulatedZoneDamage, float NewHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle|Damage")
	void OnGlassBroken(EGTPDamageZone Zone);

	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle|Damage")
	void OnWrecked();

private:
	// Input
	void OnThrottle(const FInputActionValue& Value);
	void OnThrottleReleased();
	void OnBrake(const FInputActionValue& Value);
	void OnBrakeReleased();
	void OnSteer(const FInputActionValue& Value);
	void OnSteerReleased();
	void OnHandbrakeStarted();
	void OnHandbrakeCompleted();
	void OnLook(const FInputActionValue& Value);
	void OnLookBehindStarted();
	void OnLookBehindCompleted();
	void ClearDrivingInput();

	// Enter / exit
	void FinishEnter();
	void PollExit();
	void DoExit(bool bBailOut);
	bool FindExitLocation(const ACharacter* Character, FVector& OutLocation, EGTPVehicleSide& OutSide) const;
	EGTPVehicleSide GetPassengerSide() const;
	void AttachToSeat(ACharacter* Character);
	/** The driver of the player's car found off the seat is said in the log and seated again. */
	void KeepDriverSeated();
	int32 SeatedAgainCount = 0;
	/** OnBodyHit: how many touches were named since the car was last placed (NotePlaced). */
	int32 PlacedHitsTold = 8;
	/** OnBodyHit: how many touches by somebody on foot were said while nobody drove the car. */
	int32 StandingTouchesTold = 0;
	void ReleaseCharacter(ACharacter* Character, const FVector& Location, const FRotator& Rotation);
	/** Tag of a hidden seated driver's mesh put to sleep by AttachToSeat (no pose at all until released). */
	static FName SeatDormantTag();
	float PlayMontageOr(ACharacter* Character, const TSoftObjectPtr<UAnimMontage>& Montage, float Fallback) const;

	// Camera
	void UpdateChaseCamera(float DeltaSeconds);
	void UpdateChaseCollision(const APlayerController* PC, float DeltaSeconds);
	FGTPCameraOccluders ChaseOccluders;
	float ChaseArmWanted = 620.f;
	float ChaseFraction = 1.f;
	float ChaseLift = 0.f;
	float ChaseStatSeconds = 0.f, ChaseStatSqueezed = 0.f, ChaseStatMaxLift = 0.f, ChaseStatShortest = 1e6f;
	int32 ChaseStatMaxHidden = 0;
	FString ChaseStatWall;

	// Arcade driving help and model profiles
	void UpdateDriveAssist(float DeltaSeconds);
	void SetRearGrip(float Multiplier);
	void ApplyModelProfile();

	// Damage
	UFUNCTION()
	void OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** One log line every 2 s at most for the impacts the car ignores (they came every frame). */
	void NoteIgnoredImpact(const AActor* OtherActor, float DeltaV, const TCHAR* Why);

	/**
	 * Keeps only the chassis body of the physics asset (the body Chaos drives): the other bodies
	 * and every constraint are switched off. On candidate 31 City Sample's cars were in contact on
	 * every frame (551 000 impact lines in 2 min, Chaos solver at 32 ms); extra bodies in their
	 * physics assets are the suspect. Logged once per class, with the count.
	 * -CarAllBodies keeps them all. Checked again every second (a recreated physics state brings
	 * them back).
	 */
	void KeepChassisBodyOnly();
	/**
	 * In a game world, before the components register (Chaos places each wheel at its bone's
	 * reference pose position when it creates the vehicle): when the mesh lacks a wheel setup's bone
	 * (City Sample's car meshes do not name theirs Phys_Wheel_*), the four wheel bones are taken from
	 * the mesh by name and position, once per class, and logged ("vehicle wheels"). Not for the car
	 * MassTraffic spawns to mine its simple physics from (IsPhysicsTemplateSpawn), left as at
	 * candidate 46 and logged ("vehicle physics template").
	 */
	void FindWheelBones();
	/**
	 * Spawned as MassTraffic's physics template: at the class's default root location, with no owner,
	 * no instigator and no attach parent (default spawn parameters), which none of our spawns uses.
	 */
	bool IsPhysicsTemplateSpawn() const;
	/** Wheel bones taken from the mesh by FindWheelBones: a City Sample car. */
	bool bWheelBonesFromMesh = false;
	/**
	 * City Sample cars only: Chaos shares the weight between the springs as if the centre of mass sat
	 * over the chassis body's origin (ComputeSprungMasses), but City Sample's chassis body alone puts
	 * it far back (Police13: 0.29 of the wheelbase from the rear axle) and the nose lifts under power.
	 * The centre of mass goes over the origin, at its own height (CenterOfMassOverride): set by
	 * FindWheelBones before the vehicle is created, from the chassis body's shapes; here, from the
	 * first tick, only when that could not be done. Logged once per class ("vehicle balance").
	 */
	void CentreMassOverOrigin();
	EGTPDamageZone ZoneFromWorldLocation(const FVector& WorldLocation) const;
	void BreakGlass(EGTPDamageZone Zone);
	void SetWrecked(EGTPVehicleLoss Reason = EGTPVehicleLoss::Wrecked);

	void ApplyCityCarTuning();
	UChaosWheeledVehicleMovementComponent* GetWheeledMovement() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> Driver;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> EnteringCharacter;

	UPROPERTY(Transient)
	TObjectPtr<AController> EnteringController;

	EGTPVehicleState State = EGTPVehicleState::Parked;
	EGTPEntryKind PendingEntry = EGTPEntryKind::OpenDoor;
	FTimerHandle SequenceTimer;
	float ExitWaitTime = 0.f;

	float ThrottleAmount = 0.f;

	/**
	 * Drive probe (docs/gameplay/vehicles.md 8.7), on unless -NoDriveProbe: for the player's car every
	 * second while it moves or the throttle is down, and for a police car every 3 s (40 lines at most),
	 * a "drive probe" line with the pedals, gear, rpm, the weight the springs carry, and for each wheel
	 * its contact, spring force, drive torque, its own speed against the car's, and the ground's
	 * physical material and friction; how often the chassis touched the ground; the chassis box's
	 * height above the ground under the car. Once per class, a "drive config" line (mass, differential,
	 * powered wheels, wheel figures, centre of mass and origin between the axles). For the player's car,
	 * 0-50 and 0-100 km/h from a standstill with the throttle held, and the top speed.
	 */
	void ProbeDrive();
	void LogDriveConfig(UChaosWheeledVehicleMovementComponent& Move, USkeletalMeshComponent& Body) const;
	/** Height of the chassis body's box above the ground under the car (cm), or -9999 without ground. */
	float ChassisClearanceCm(USkeletalMeshComponent& Body) const;
	/** A body's collision shapes as a box in the car's component space (cm). */
	FBox CollisionBoxInComponent(USkeletalMeshComponent& Body, const FBodyInstance& Part) const;
	/** Height, in the car's component space, where the wheels touch flat ground with the suspension at rest; OutMaxRaise: the largest suspension raise. */
	float WheelContactZ(const UChaosWheeledVehicleMovementComponent& Move, USkeletalMeshComponent& Body, float& OutMaxRaise) const;
	/** Once per class, before the other bodies go: each body of the physics asset, its shapes and how high it sits above the wheels' contact plane. */
	void LogCollisionBodies(USkeletalMeshComponent& Body) const;
	float NextDriveProbeTime = 0.f;
	int32 PoliceProbeLines = 0;
	int32 GroundHitsSinceProbe = 0;
	float LaunchTime = -1.f;
	bool bLaunch50Logged = false;
	float TopSpeedKmh = 0.f;
	/**
	 * -DriveLaunchTest: the player's car (the game starts in it with -PlayerPawn=<car class>) is put
	 * on the longest clear straight of the map (PlaceForLaunchTest), waits 3 s, then drives along it
	 * at full throttle, held on the line, until it must brake before the end of the straight (or 40 s),
	 * so the probe above times its 0-50 and 0-100 km/h; one "drive launch test" line gives the top
	 * speed, 0-130, the speed every 100 m, what ended the run and the torque, mass and centre of mass.
	 */
	void RunLaunchTest();
	/**
	 * Longest straight of the traffic lanes (lanes within 1.5 m of a line, on through the next lane
	 * that keeps the heading) whose road a car-wide box crosses without meeting the static world;
	 * the car is put 3 m into it. Logged ("drive launch test: straight of ...").
	 */
	void PlaceForLaunchTest();
	float LaunchTestStart = -1.f;
	bool bLaunchTestDone = false;
	FVector LaunchTestFrom = FVector::ZeroVector;
	FVector2D LaunchLineStart = FVector2D::ZeroVector;
	FVector2D LaunchLineDir = FVector2D::ZeroVector;
	float LaunchLineCm = 0.f;
	int32 LaunchPlaceTries = 0;
	/** Physics off and the car held where PlaceForLaunchTest put it until then (game time), -1 when not held. */
	float LaunchHoldUntil = -1.f;
	FTransform LaunchHoldAt;
	TArray<float> LaunchTestMarks;
	float LaunchTest130 = -1.f;

	/** -DriveDiag=1 trace, see TraceDriveDiag. */
	static bool IsDriveDiagWanted();
	void TraceDriveDiag(float Brake, bool bHandbrake);
	float LastDriveDiagTime = -100.f;

	bool bInteriorView = false;
	bool bLookingBehind = false;
	float LastLookInputTime = -1000.f;

	float Health = 1000.f;
	bool bWrecked = false;
	float ZoneDamage[4] = {0.f, 0.f, 0.f, 0.f};
	bool bGlassBroken[4] = {false, false, false, false};

	/** Components built from GlassMeshes (rebuilt on each construction). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> GlassComponents;

	/** Components built from LookMeshes (rebuilt on each construction). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> LookComponents;
	float LastImpactTime = -1000.f;
	float IgnoredLogTime = -1000.f;
	int32 IgnoredSinceLog = 0;
	float IgnoredMaxDeltaV = 0.f;
	float NextBodyCheckTime = 0.f;
	float LastPlacedTime = -1000.f;
	const TCHAR* LastPlacedHow = TEXT("none");
	bool bHitByPlayer = false;
	bool bFleeAfterExit = false;

	GTPCarjack::FDriveAssist Assist;
	bool bPlayerAssist = false;
	bool bHandbrakeHeld = false;
	float SteerInput = 0.f;
	/** NoteScriptedSteering: when a script last steered (world seconds), and the pull of the dents left out was said once. */
	float ScriptSteerTime = -1000.f;
	bool bDentPullWithheldTold = false;
	float BaseTorque = 130.f;
	float RearGripBase = 1.f;
	float RearGripNow = 1.f;
	float AppliedPower = 1.f;
	float UpsideDownTime = 0.f;
	/** On its roof or side and stopped for 2.5 s: lifted and set on its wheels (the player's car and AI-driven cars). */
	void PutBackOnWheels(float DeltaSeconds);
	float ImpactShake = 0.f;
	double AIStopUntil = -1.0;
	double EngineStartUntil = -1.0;
	FTimerHandle AlarmTimer;
	double AlarmUntil = -1.0;
	bool bAlarmHornOn = false;
	bool bParkedAsleep = false;
	bool bShownByMassTraffic = false;
	void PulseAlarm();
	FVector FleeFrom = FVector::ZeroVector;
};
