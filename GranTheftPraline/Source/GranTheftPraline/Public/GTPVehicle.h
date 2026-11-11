#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "WheeledVehiclePawn.h"
#include "GTPVehicle.generated.h"

class AController;
class ACharacter;
class UAnimMontage;
class UCameraComponent;
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
 * - wheel bones Phys_Wheel_FL, Phys_Wheel_FR, Phys_Wheel_BL, Phys_Wheel_BR;
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
	 * Takes whoever sits at the wheel out at once and puts them at Location
	 * (river recovery: the car sank). The player controller gets its character
	 * back. Returns the character, or null when nobody was inside.
	 */
	ACharacter* EjectDriverTo(const FVector& Location, const FRotator& Rotation);

	/** Car fell into water: engine dead, wrecked, no more input. */
	void Drown();

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

	/** True while someone sits at the wheel of a car that is not wrecked. */
	UFUNCTION(BlueprintPure, Category = "Vehicle")
	bool IsEngineRunning() const { return Driver != nullptr && !bWrecked; }

	/**
	 * Driving inputs for a non-player driver (UGTPAIDriverComponent).
	 * Ignored while the player drives or once the car is wrecked.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void SetAIDriveInput(float Throttle, float Brake, float Steering, bool bHandbrake);

	/** True when a non-player character sits at the wheel. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seats")
	bool HasAIDriver() const { return Driver != nullptr && State != EGTPVehicleState::Driven; }

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
	void ReleaseCharacter(ACharacter* Character, const FVector& Location, const FRotator& Rotation);
	float PlayMontageOr(ACharacter* Character, const TSoftObjectPtr<UAnimMontage>& Montage, float Fallback) const;

	// Camera
	void UpdateChaseCamera(float DeltaSeconds);

	// Damage
	UFUNCTION()
	void OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
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
	float LastImpactTime = -1000.f;
};
