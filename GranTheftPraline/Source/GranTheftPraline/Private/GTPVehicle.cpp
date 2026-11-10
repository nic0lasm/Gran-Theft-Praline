#include "GTPVehicle.h"

#include "GTPCameraModeComponent.h"
#include "GTPEngineSoundComponent.h"
#include "GTPPlayerController.h"
#include "GTPVehicleInput.h"
#include "GTPVehicleWheels.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPVehicle, Log, All);

namespace
{
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float ExitPollInterval = 0.1f;
	constexpr float MaxExitWait = 3.f;
	constexpr float MinTimeBetweenImpacts = 0.15f;

	EGTPDamageZone ZoneOfSide(EGTPVehicleSide Side)
	{
		return Side == EGTPVehicleSide::Left ? EGTPDamageZone::Left : EGTPDamageZone::Right;
	}

	FName ZoneTag(EGTPDamageZone Zone)
	{
		switch (Zone)
		{
		case EGTPDamageZone::Front: return TEXT("Front");
		case EGTPDamageZone::Rear: return TEXT("Rear");
		case EGTPDamageZone::Left: return TEXT("Left");
		default: return TEXT("Right");
		}
	}
}

AGTPVehicle::AGTPVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// The car turns with its wheels, never with the view.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// AWheeledVehiclePawn leaves the body kinematic (Epic's template turns
	// physics on in its Blueprint): without it Chaos never moves the car.
	GetMesh()->SetSimulatePhysics(true);

	ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
	ChaseArm->SetupAttachment(GetMesh());
	ChaseArm->bUsePawnControlRotation = true;
	ChaseArm->bInheritRoll = false;
	ChaseArm->bDoCollisionTest = true;
	ChaseArm->bEnableCameraLag = true;
	// Tight lag, capped: the car stays the same size on screen at speed.
	ChaseArm->CameraLagSpeed = 14.f;
	ChaseArm->CameraLagMaxDistance = 150.f;
	ChaseArm->bEnableCameraRotationLag = true;
	ChaseArm->CameraRotationLagSpeed = 10.f;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);
	ChaseCamera->bUsePawnControlRotation = false;

	InteriorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InteriorCamera"));
	InteriorCamera->SetupAttachment(GetMesh());
	InteriorCamera->bUsePawnControlRotation = true;
	InteriorCamera->bAutoActivate = false;

	EngineSound = CreateDefaultSubobject<UGTPEngineSoundComponent>(TEXT("EngineSound"));
	EngineSound->SetupAttachment(GetMesh());
	// Engine bay, front of the car.
	EngineSound->SetRelativeLocation(FVector(150.f, 0.f, 60.f));

	DentMorphTargets.Add(EGTPDamageZone::Front, TEXT("Dent_Front"));
	DentMorphTargets.Add(EGTPDamageZone::Rear, TEXT("Dent_Rear"));
	DentMorphTargets.Add(EGTPDamageZone::Left, TEXT("Dent_Left"));
	DentMorphTargets.Add(EGTPDamageZone::Right, TEXT("Dent_Right"));

	ApplyCityCarTuning();
}

UChaosWheeledVehicleMovementComponent* AGTPVehicle::GetWheeledMovement() const
{
	return Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
}

void AGTPVehicle::ApplyCityCarTuning()
{
	// A current French city car: about 1,150 kg, 100 hp class, front-wheel
	// drive, 5-speed automatic for the player. Each model's Blueprint may
	// override any of this (docs/gameplay/vehicles.md, section 4).
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move)
	{
		return;
	}
	Move->Mass = 1150.f;
	Move->ChassisWidth = 175.f;
	Move->ChassisHeight = 145.f;
	Move->DragCoefficient = 0.32f;
	Move->DownforceCoefficient = 0.1f;
	// Brake held while stopped reverses, as in most open-world games.
	Move->bReverseAsBrake = true;

	Move->WheelSetups.SetNum(4);
	Move->WheelSetups[0].WheelClass = UGTPWheelFront::StaticClass();
	Move->WheelSetups[0].BoneName = TEXT("Phys_Wheel_FL");
	Move->WheelSetups[1].WheelClass = UGTPWheelFront::StaticClass();
	Move->WheelSetups[1].BoneName = TEXT("Phys_Wheel_FR");
	Move->WheelSetups[2].WheelClass = UGTPWheelRear::StaticClass();
	Move->WheelSetups[2].BoneName = TEXT("Phys_Wheel_BL");
	Move->WheelSetups[3].WheelClass = UGTPWheelRear::StaticClass();
	Move->WheelSetups[3].BoneName = TEXT("Phys_Wheel_BR");

	// Chaos has no drivetrain losses: about 130 Nm at the wheels' input gives
	// a real 100 hp city car's pace (0-50 km/h in about 4 s, 0-100 in about 11 s).
	Move->EngineSetup.MaxTorque = 130.f;
	Move->EngineSetup.MaxRPM = 6000.f;
	Move->EngineSetup.EngineIdleRPM = 850.f;
	Move->EngineSetup.EngineBrakeEffect = 0.2f;
	if (FRichCurve* Torque = Move->EngineSetup.TorqueCurve.GetRichCurve())
	{
		Torque->Reset();
		Torque->AddKey(0.f, 0.55f);
		Torque->AddKey(1800.f, 1.f);
		Torque->AddKey(4500.f, 0.95f);
		Torque->AddKey(6000.f, 0.7f);
	}

	Move->DifferentialSetup.DifferentialType = EVehicleDifferential::FrontWheelDrive;

	Move->TransmissionSetup.bUseAutomaticGears = true;
	Move->TransmissionSetup.bUseAutoReverse = true;
	Move->TransmissionSetup.FinalRatio = 4.1f;
	Move->TransmissionSetup.ForwardGearRatios = {3.73f, 2.05f, 1.39f, 1.03f, 0.82f};
	Move->TransmissionSetup.ReverseGearRatios = {3.55f};
	Move->TransmissionSetup.ChangeUpRPM = 4800.f;
	Move->TransmissionSetup.ChangeDownRPM = 2000.f;
	Move->TransmissionSetup.GearChangeTime = 0.25f;

	// Less steering at speed so the car stays drivable on a keyboard.
	if (FRichCurve* Steering = Move->SteeringSetup.SteeringCurve.GetRichCurve())
	{
		Steering->Reset();
		Steering->AddKey(0.f, 1.f);
		Steering->AddKey(30.f, 0.75f);
		Steering->AddKey(80.f, 0.3f);
		Steering->AddKey(120.f, 0.22f);
	}
}

void AGTPVehicle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	for (UStaticMeshComponent* Old : GlassComponents)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	GlassComponents.Reset();

	USkeletalMeshComponent* Body = GetMesh();
	for (const TPair<EGTPDamageZone, TObjectPtr<UStaticMesh>>& Pair : GlassMeshes)
	{
		if (!Pair.Value || !Body)
		{
			continue;
		}
		UStaticMeshComponent* Glass = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Glass->SetStaticMesh(Pair.Value);
		Glass->SetupAttachment(Body);
		Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Glass->SetCastShadow(false);
		Glass->ComponentTags = {TEXT("Glass"), ZoneTag(Pair.Key)};
		Glass->RegisterComponent();
		GlassComponents.Add(Glass);
	}
}

void AGTPVehicle::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;

	ChaseArm->SetRelativeLocation(FVector(0.f, 0.f, ChasePivotHeight));
	ChaseArm->TargetArmLength = ChaseDistance;
	ChaseArm->SocketOffset = FVector(0.f, 0.f, ChaseHeight);
	ChaseCamera->SetFieldOfView(BaseFOV);
	InteriorCamera->SetRelativeLocation(InteriorEyeOffset);
	InteriorCamera->SetFieldOfView(InteriorFOV);

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetNotifyRigidBodyCollision(true);
		Body->OnComponentHit.AddDynamic(this, &AGTPVehicle::OnBodyHit);
	}

	// Parked cars do not roll away.
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetHandbrakeInput(true);
	}
}

float AGTPVehicle::GetSpeedKmh() const
{
	const UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	return Move ? Move->GetForwardSpeed() * CmPerSecToKmh : 0.f;
}

float AGTPVehicle::GetEngineRpm() const
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	return Move ? Move->GetEngineRotationSpeed() : 0.f;
}

int32 AGTPVehicle::GetGear() const
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	return Move ? Move->GetCurrentGear() : 0;
}

void AGTPVehicle::SetAIDriveInput(float Throttle, float Brake, float Steering, bool bHandbrake)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || !HasAIDriver() || bWrecked)
	{
		return;
	}
	ThrottleAmount = FMath::Clamp(Throttle, 0.f, 1.f);
	Move->SetThrottleInput(ThrottleAmount);
	Move->SetBrakeInput(FMath::Clamp(Brake, 0.f, 1.f));
	Move->SetSteeringInput(FMath::Clamp(Steering, -1.f, 1.f));
	Move->SetHandbrakeInput(bHandbrake);
}

// --- Input -----------------------------------------------------------------

void AGTPVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	AGTPPlayerController* PC = Cast<AGTPPlayerController>(GetController());
	UGTPVehicleInput* Input = PC ? PC->GetVehicleInput() : nullptr;
	if (!Enhanced || !Input)
	{
		UE_LOG(LogGTPVehicle, Warning, TEXT("%s: no enhanced input or vehicle input set, driving disabled"), *GetName());
		return;
	}
	Enhanced->BindAction(Input->Throttle, ETriggerEvent::Triggered, this, &AGTPVehicle::OnThrottle);
	Enhanced->BindAction(Input->Throttle, ETriggerEvent::Completed, this, &AGTPVehicle::OnThrottleReleased);
	Enhanced->BindAction(Input->Brake, ETriggerEvent::Triggered, this, &AGTPVehicle::OnBrake);
	Enhanced->BindAction(Input->Brake, ETriggerEvent::Completed, this, &AGTPVehicle::OnBrakeReleased);
	Enhanced->BindAction(Input->Steer, ETriggerEvent::Triggered, this, &AGTPVehicle::OnSteer);
	Enhanced->BindAction(Input->Steer, ETriggerEvent::Completed, this, &AGTPVehicle::OnSteerReleased);
	Enhanced->BindAction(Input->Handbrake, ETriggerEvent::Started, this, &AGTPVehicle::OnHandbrakeStarted);
	Enhanced->BindAction(Input->Handbrake, ETriggerEvent::Completed, this, &AGTPVehicle::OnHandbrakeCompleted);
	Enhanced->BindAction(Input->Look, ETriggerEvent::Triggered, this, &AGTPVehicle::OnLook);
	Enhanced->BindAction(Input->LookBehind, ETriggerEvent::Started, this, &AGTPVehicle::OnLookBehindStarted);
	Enhanced->BindAction(Input->LookBehind, ETriggerEvent::Completed, this, &AGTPVehicle::OnLookBehindCompleted);
	// Enter / exit and the view toggle are bound by the player controller.
}

void AGTPVehicle::OnThrottle(const FInputActionValue& Value)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven && !bWrecked)
	{
		ThrottleAmount = FMath::Clamp(Value.Get<float>(), 0.f, 1.f);
		Move->SetThrottleInput(ThrottleAmount);
	}
}

void AGTPVehicle::OnThrottleReleased()
{
	ThrottleAmount = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetThrottleInput(0.f);
	}
}

void AGTPVehicle::OnBrake(const FInputActionValue& Value)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven)
	{
		Move->SetBrakeInput(FMath::Clamp(Value.Get<float>(), 0.f, 1.f));
	}
}

void AGTPVehicle::OnBrakeReleased()
{
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetBrakeInput(0.f);
	}
}

void AGTPVehicle::OnSteer(const FInputActionValue& Value)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven)
	{
		Move->SetSteeringInput(FMath::Clamp(Value.Get<float>(), -1.f, 1.f));
	}
}

void AGTPVehicle::OnSteerReleased()
{
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetSteeringInput(0.f);
	}
}

void AGTPVehicle::OnHandbrakeStarted()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven)
	{
		Move->SetHandbrakeInput(true);
	}
}

void AGTPVehicle::OnHandbrakeCompleted()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven)
	{
		Move->SetHandbrakeInput(false);
	}
}

void AGTPVehicle::OnLook(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	if (!Look.IsNearlyZero())
	{
		AddControllerYawInput(Look.X);
		AddControllerPitchInput(Look.Y);
		LastLookInputTime = GetWorld()->GetTimeSeconds();
	}
}

void AGTPVehicle::OnLookBehindStarted()
{
	bLookingBehind = true;
	ChaseArm->bUsePawnControlRotation = false;
	ChaseArm->SetRelativeRotation(FRotator(ChasePitch, 180.f, 0.f));
	InteriorCamera->bUsePawnControlRotation = false;
	InteriorCamera->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
}

void AGTPVehicle::OnLookBehindCompleted()
{
	bLookingBehind = false;
	ChaseArm->bUsePawnControlRotation = true;
	InteriorCamera->bUsePawnControlRotation = true;
}

void AGTPVehicle::ClearDrivingInput()
{
	ThrottleAmount = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(0.f);
		Move->SetSteeringInput(0.f);
		Move->SetHandbrakeInput(false);
	}
	if (bLookingBehind)
	{
		OnLookBehindCompleted();
	}
}

void AGTPVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	State = EGTPVehicleState::Driven;
	ClearDrivingInput();
	LastLookInputTime = -1000.f;
	if (const AGTPPlayerController* PC = Cast<AGTPPlayerController>(NewController))
	{
		SetInteriorView(PC->GetCameraMode() && PC->GetCameraMode()->IsFirstPerson());
	}
}

void AGTPVehicle::UnPossessed()
{
	Super::UnPossessed();
	ClearDrivingInput();
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		// A car left behind by a bail out keeps rolling; otherwise it parks.
		Move->SetHandbrakeInput(FMath::Abs(GetSpeedKmh()) < BailOutSpeedKmh);
	}
}

// --- Camera ----------------------------------------------------------------

void AGTPVehicle::SetInteriorView(bool bEnable)
{
	bInteriorView = bEnable;
	ChaseCamera->SetActive(!bEnable);
	InteriorCamera->SetActive(bEnable);
}

void AGTPVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == EGTPVehicleState::Driven && IsLocallyControlled())
	{
		UpdateChaseCamera(DeltaSeconds);
	}
}

void AGTPVehicle::UpdateChaseCamera(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	const float Speed = FMath::Abs(GetSpeedKmh());
	const float SpeedAlpha = FOVMaxSpeedKmh > 0.f ? FMath::Clamp(Speed / FOVMaxSpeedKmh, 0.f, 1.f) : 0.f;
	ChaseCamera->SetFieldOfView(FMath::Lerp(BaseFOV, MaxSpeedFOV, SpeedAlpha));

	if (bInteriorView || bLookingBehind)
	{
		return;
	}
	// GTA-style: after a short pause in look input, swing back behind the car.
	const float SinceLook = GetWorld()->GetTimeSeconds() - LastLookInputTime;
	if (SinceLook > RecenterDelay && Speed > RecenterMinSpeedKmh)
	{
		const FRotator Current = PC->GetControlRotation();
		const FRotator Target(ChasePitch, GetActorRotation().Yaw, 0.f);
		PC->SetControlRotation(FMath::RInterpTo(Current, Target, DeltaSeconds, RecenterSpeed));
	}
}

// --- Enter and exit --------------------------------------------------------

EGTPVehicleSide AGTPVehicle::GetPassengerSide() const
{
	return DriverSide == EGTPVehicleSide::Left ? EGTPVehicleSide::Right : EGTPVehicleSide::Left;
}

FVector AGTPVehicle::GetDoorLocation(EGTPVehicleSide Side) const
{
	FVector Offset = LeftDoorOffset;
	if (Side == EGTPVehicleSide::Right)
	{
		Offset.Y = -Offset.Y;
	}
	return GetActorTransform().TransformPosition(Offset);
}

bool AGTPVehicle::CanEnter(const ACharacter* Character) const
{
	if (!Character || Character == Driver || bWrecked)
	{
		return false;
	}
	if (State == EGTPVehicleState::Entering || State == EGTPVehicleState::Exiting)
	{
		return false;
	}
	// A moving car cannot be boarded (a driven one stops for a person in front of it).
	if (FMath::Abs(GetSpeedKmh()) > MaxEntrySpeedKmh)
	{
		return false;
	}
	// Never take the car from another player.
	const AController* Current = GetController();
	return !(Current && Current->IsPlayerController());
}

EGTPEntryKind AGTPVehicle::GetEntryKind() const
{
	if (Driver)
	{
		return EGTPEntryKind::Carjack;
	}
	if (bLocked && !bGlassBroken[static_cast<int32>(ZoneOfSide(DriverSide))])
	{
		return EGTPEntryKind::BreakWindow;
	}
	return EGTPEntryKind::OpenDoor;
}

float AGTPVehicle::PlayMontageOr(ACharacter* Character, const TSoftObjectPtr<UAnimMontage>& Montage, float Fallback) const
{
	if (UAnimMontage* Loaded = Montage.LoadSynchronous())
	{
		const float Length = Character->PlayAnimMontage(Loaded);
		if (Length > 0.f)
		{
			return Length;
		}
	}
	return Fallback;
}

bool AGTPVehicle::BeginEnter(ACharacter* Character, AController* DriverController)
{
	if (!CanEnter(Character) || !DriverController)
	{
		return false;
	}
	PendingEntry = GetEntryKind();
	EnteringCharacter = Character;
	EnteringController = DriverController;
	State = EGTPVehicleState::Entering;

	// Freeze the character at the driver door, facing the car. A walk to the
	// door with motion warping replaces this snap when the entry animations exist.
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	DriverController->SetIgnoreMoveInput(true);
	Character->MoveIgnoreActorAdd(this);
	const FVector Door = GetDoorLocation(DriverSide);
	const FVector Stand(Door.X, Door.Y, Character->GetActorLocation().Z);
	const FRotator Facing(0.f, (GetActorLocation() - Stand).GetSafeNormal2D().Rotation().Yaw, 0.f);
	Character->SetActorLocationAndRotation(Stand, Facing, false, nullptr, ETeleportType::TeleportPhysics);

	float Duration = EnterDuration;
	switch (PendingEntry)
	{
	case EGTPEntryKind::Carjack:
		// The car stops where it is while its driver is pulled out.
		ClearDrivingInput();
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetBrakeInput(1.f);
			Move->SetHandbrakeInput(true);
		}
		if (ACharacter* Victim = Driver)
		{
			// Pulled out through the driver door, thrown a little further out.
			Driver = nullptr;
			const FVector Out = (Door - GetActorLocation()).GetSafeNormal2D();
			ReleaseCharacter(Victim, Door + Out * 110.f + FVector(0.f, 0.f, Victim->GetSimpleCollisionHalfHeight()), Facing);
			Victim->LaunchCharacter(Out * 250.f, true, false);
			// A non-player driver runs off, away from the car (no navigation mesh needed).
			if (AAIController* VictimAI = Cast<AAIController>(Victim->GetController()))
			{
				const FVector FleeTo = Door + Out * VictimFleeDistance + GetActorForwardVector() * (VictimFleeDistance * 0.5f);
				VictimAI->MoveToLocation(FleeTo, 50.f, false, false);
			}
		}
		Duration = PlayMontageOr(Character, CarjackMontage, CarjackDuration);
		break;
	case EGTPEntryKind::BreakWindow:
		BreakGlass(ZoneOfSide(DriverSide));
		Duration = PlayMontageOr(Character, BreakWindowMontage, BreakWindowDuration);
		break;
	default:
		Duration = PlayMontageOr(Character, EnterMontage, EnterDuration);
		break;
	}
	UE_LOG(LogGTPVehicle, Log, TEXT("%s: entering (%s, %.1f s)"), *GetName(),
		*StaticEnum<EGTPEntryKind>()->GetNameStringByValue(static_cast<int64>(PendingEntry)), Duration);
	GetWorldTimerManager().SetTimer(SequenceTimer, this, &AGTPVehicle::FinishEnter, FMath::Max(Duration, 0.05f), false);
	return true;
}

void AGTPVehicle::FinishEnter()
{
	ACharacter* Character = EnteringCharacter;
	AController* DriverController = EnteringController;
	EnteringCharacter = nullptr;
	EnteringController = nullptr;
	if (!IsValid(Character) || !IsValid(DriverController))
	{
		State = EGTPVehicleState::Parked;
		return;
	}
	AttachToSeat(Character);
	Driver = Character;
	bLocked = false;
	DriverController->SetIgnoreMoveInput(false);
	State = EGTPVehicleState::Driven;
	DriverController->Possess(this);
}

void AGTPVehicle::SeatOccupant(ACharacter* Character)
{
	if (!Character || Driver)
	{
		return;
	}
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->DisableMovement();
	}
	AttachToSeat(Character);
	Driver = Character;
}

void AGTPVehicle::AttachToSeat(ACharacter* Character)
{
	Character->SetActorEnableCollision(false);
	USkeletalMeshComponent* Body = GetMesh();
	if (Body && Body->DoesSocketExist(DriverSeatSocket))
	{
		Character->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, DriverSeatSocket);
	}
	else
	{
		Character->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform);
		Character->SetActorRelativeTransform(FTransform(FRotator::ZeroRotator, DriverSeatOffset));
	}
	Character->SetActorHiddenInGame(!bShowDriverInside);
}

void AGTPVehicle::ReleaseCharacter(ACharacter* Character, const FVector& Location, const FRotator& Rotation)
{
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorHiddenInGame(false);
	Character->SetActorEnableCollision(true);
	Character->MoveIgnoreActorRemove(this);
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}

void AGTPVehicle::RequestExit()
{
	if (State != EGTPVehicleState::Driven || !Driver)
	{
		return;
	}
	if (FMath::Abs(GetSpeedKmh()) > BailOutSpeedKmh)
	{
		DoExit(true);
		return;
	}
	State = EGTPVehicleState::Exiting;
	ClearDrivingInput();
	ExitWaitTime = 0.f;
	PollExit();
	GetWorldTimerManager().SetTimer(SequenceTimer, this, &AGTPVehicle::PollExit, ExitPollInterval, true);
}

void AGTPVehicle::PollExit()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	const float Speed = GetSpeedKmh();
	if (FMath::Abs(Speed) <= StoppedSpeedKmh || ExitWaitTime >= MaxExitWait)
	{
		GetWorldTimerManager().ClearTimer(SequenceTimer);
		DoExit(false);
		return;
	}
	ExitWaitTime += ExitPollInterval;
	if (Move)
	{
		// Brake only while rolling forward: with reverse-as-brake, braking at
		// a standstill would start reversing.
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(Speed > StoppedSpeedKmh ? 1.f : 0.f);
		Move->SetHandbrakeInput(true);
	}
}

bool AGTPVehicle::FindExitLocation(const ACharacter* Character, FVector& OutLocation, EGTPVehicleSide& OutSide) const
{
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 35.f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPVehicleExit), false, this);
	Params.AddIgnoredActor(Character);

	// Driver door, then the other door (climbing across), then the roof.
	const EGTPVehicleSide Sides[] = {DriverSide, GetPassengerSide()};
	for (const EGTPVehicleSide Side : Sides)
	{
		const FVector Test = GetDoorLocation(Side) + FVector(0.f, 0.f, HalfHeight + 10.f);
		if (!GetWorld()->OverlapBlockingTestByChannel(Test, FQuat::Identity, ECC_Pawn, Shape, Params))
		{
			OutLocation = Test;
			OutSide = Side;
			return true;
		}
	}
	const FBox Bounds = GetComponentsBoundingBox();
	const FVector Roof(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z + HalfHeight + 5.f);
	if (!GetWorld()->OverlapBlockingTestByChannel(Roof, FQuat::Identity, ECC_Pawn, Shape, Params))
	{
		OutLocation = Roof;
		OutSide = DriverSide;
		return true;
	}
	return false;
}

void AGTPVehicle::DoExit(bool bBailOut)
{
	GetWorldTimerManager().ClearTimer(SequenceTimer);
	ACharacter* Character = Driver;
	AController* Controller = GetController();
	if (!Character)
	{
		State = EGTPVehicleState::Parked;
		return;
	}

	FVector Location;
	EGTPVehicleSide Side;
	if (!FindExitLocation(Character, Location, Side))
	{
		// Boxed in: stay inside, the player can drive out.
		UE_LOG(LogGTPVehicle, Log, TEXT("%s: no free exit"), *GetName());
		State = EGTPVehicleState::Driven;
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetBrakeInput(0.f);
			Move->SetHandbrakeInput(false);
		}
		return;
	}

	const FVector CarVelocity = GetVelocity();
	Driver = nullptr;
	State = EGTPVehicleState::Parked;
	ReleaseCharacter(Character, Location, FRotator(0.f, GetActorRotation().Yaw, 0.f));
	if (Controller)
	{
		Controller->Possess(Character);
	}

	if (bBailOut)
	{
		// Thrown sideways with part of the car's speed; the car rolls on.
		const FVector Out = (Location - GetActorLocation()).GetSafeNormal2D();
		Character->LaunchCharacter(Out * BailOutLaunchSpeed + CarVelocity * 0.6f + FVector(0.f, 0.f, 200.f), true, true);
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetHandbrakeInput(false);
		}
	}
	else
	{
		PlayMontageOr(Character, ExitMontage, ExitDuration);
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetBrakeInput(0.f);
			Move->SetHandbrakeInput(true);
		}
	}
	UE_LOG(LogGTPVehicle, Log, TEXT("%s: driver out (%s)"), *GetName(), bBailOut ? TEXT("bail out") : TEXT("stopped"));
}

// --- Damage ----------------------------------------------------------------

void AGTPVehicle::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Mass = HitComponent ? HitComponent->GetMass() : 0.f;
	if (Mass <= 0.f || Now - LastImpactTime < MinTimeBetweenImpacts)
	{
		return;
	}
	// Change of speed caused by the impact, in cm/s.
	const float DeltaV = NormalImpulse.Size() / Mass;
	if (DeltaV < MinImpactSpeed)
	{
		return;
	}
	LastImpactTime = Now;
	ApplyZoneDamage(ZoneFromWorldLocation(Hit.ImpactPoint), (DeltaV - MinImpactSpeed) * DamagePerImpactSpeed);
}

EGTPDamageZone AGTPVehicle::ZoneFromWorldLocation(const FVector& WorldLocation) const
{
	// Normalised by a typical half length and half width, so corners split fairly.
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const float Along = Local.X / 200.f;
	const float Across = Local.Y / 88.f;
	if (FMath::Abs(Along) >= FMath::Abs(Across))
	{
		return Along >= 0.f ? EGTPDamageZone::Front : EGTPDamageZone::Rear;
	}
	return Across < 0.f ? EGTPDamageZone::Left : EGTPDamageZone::Right;
}

void AGTPVehicle::ApplyZoneDamage(EGTPDamageZone Zone, float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	const int32 Index = static_cast<int32>(Zone);
	ZoneDamage[Index] += Amount;

	if (const FName* Morph = DentMorphTargets.Find(Zone))
	{
		const float Dent = FullDentDamage > 0.f ? FMath::Clamp(ZoneDamage[Index] / FullDentDamage, 0.f, 1.f) : 1.f;
		GetMesh()->SetMorphTarget(*Morph, Dent);
	}
	if (!bGlassBroken[Index] && ZoneDamage[Index] >= GlassBreakDamage)
	{
		BreakGlass(Zone);
	}

	Health = FMath::Max(0.f, Health - Amount);
	OnDamageChanged(Zone, ZoneDamage[Index], Health);
	if (Health <= 0.f && !bWrecked)
	{
		SetWrecked();
	}
}

float AGTPVehicle::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	FVector Where = GetActorLocation();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		Where = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.ImpactPoint;
	}
	else if (DamageCauser)
	{
		Where = DamageCauser->GetActorLocation();
	}
	ApplyZoneDamage(ZoneFromWorldLocation(Where), Actual);
	return Actual;
}

void AGTPVehicle::BreakGlass(EGTPDamageZone Zone)
{
	const int32 Index = static_cast<int32>(Zone);
	if (bGlassBroken[Index])
	{
		return;
	}
	bGlassBroken[Index] = true;
	const FName Tag = ZoneTag(Zone);
	TInlineComponentArray<UPrimitiveComponent*> Parts(this);
	for (UPrimitiveComponent* Part : Parts)
	{
		if (Part->ComponentHasTag(TEXT("Glass")) && Part->ComponentHasTag(Tag))
		{
			Part->SetVisibility(false);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	OnGlassBroken(Zone);
}

void AGTPVehicle::SetWrecked()
{
	bWrecked = true;
	ThrottleAmount = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetThrottleInput(0.f);
	}
	UE_LOG(LogGTPVehicle, Log, TEXT("%s: wrecked"), *GetName());
	OnWrecked();
}
