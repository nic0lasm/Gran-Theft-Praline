#include "GTPVehicle.h"

#include "Kismet/GameplayStatics.h"

#include "GTPCameraModeComponent.h"
#include "GTPCarjackVictim.h"
#include "GTPLog.h"
#include "GTPPlayerReaction.h"
#include "GTPEngineSoundComponent.h"
#include "GTPHornComponent.h"
#include "GTPTraffic.h"
#include "GTPPlayerController.h"
#include "GTPVehicleInput.h"
#include "GTPVehicleWheels.h"
#include "GTPWanted.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Crimes of the player at the wheel go to the wanted level (GTPWanted.h).
namespace
{
	void ReportPlayerCrime(const AGTPVehicle* Vehicle, EGTPCrimeKind Kind, bool bVictimInvolved = false)
	{
		if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(Vehicle))
		{
			Wanted->ReportCrime(Kind, Vehicle->GetActorLocation(), bVictimInvolved);
		}
	}
}


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
	// Traffic and police cars are never possessed: their driver is a seated
	// character. With the engine default (inputs need a local controller) Chaos
	// replaces the pedals of an unpossessed car with its replicated state, zero,
	// so the car idles in neutral at 850 rpm whatever SetThrottleInput asks.
	Move->SetRequiresControllerForInputs(false);

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
	KeepChassisBodyOnly();
	// The chase camera's arm only works for the player's car (PossessedBy turns it on).
	ChaseArm->SetComponentTickEnabled(Cast<APlayerController>(GetController()) != nullptr);

	// Parked cars do not roll away.
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		// Again here in case a Blueprint turned it back on (see ApplyCityCarTuning).
		Move->SetRequiresControllerForInputs(false);
		Move->SetHandbrakeInput(true);
	}
	ApplyModelProfile();
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		BaseTorque = Move->EngineSetup.MaxTorque;
		if (Move->Wheels.Num() > 2 && Move->Wheels[2])
		{
			RearGripBase = Move->Wheels[2]->FrictionForceMultiplier;
			RearGripNow = RearGripBase;
		}
	}
	// The arcade boost is relative to a 130 N.m city car: a 500 N.m saloon (City Sample) keeps
	// its edge over a Clio in the player's hands.
	Assist.PlayerTorque = PlayerTorque * FMath::Max(1.f, BaseTorque / 130.f);
	Assist.DriftRearGrip = DriftRearGrip;
	Assist.DriftYawAccel = DriftYawAccel;
	Assist.StabilityDamping = StabilityDamping;
}

void AGTPVehicle::ApplyModelProfile()
{
	// Driving profiles of docs/gameplay/vehicles.md section 4, by model, for the models
	// whose Blueprint (tools/vehicles/unreal/import_vehicle.py) keeps the city car tuning.
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	if (!Move || !Body)
	{
		return;
	}
	const FString Model = GetClass()->GetName();
	float Torque = 0.f;
	float MassKg = 0.f;
	if (Model.Contains(TEXT("Kangoo")) || Model.Contains(TEXT("Berlingo")) || Model.Contains(TEXT("Trafic")))
	{
		// Van: heavier, slower, long braking.
		Torque = 200.f;
		MassKg = 1500.f;
	}
	else if (Model.Contains(TEXT("208")))
	{
		Torque = 140.f;
		MassKg = 1160.f;
	}
	if (Torque > 0.f)
	{
		Move->EngineSetup.MaxTorque = Torque;
		Move->SetMaxEngineTorque(Torque);
	}
	if (MassKg > 0.f)
	{
		Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	}
}

bool AGTPVehicle::IsEngineRunning() const
{
	const UWorld* World = GetWorld();
	return Driver != nullptr && !bWrecked && (!World || World->GetTimeSeconds() >= EngineStartUntil);
}

void AGTPVehicle::RequestAIStop(float Seconds)
{
	if (const UWorld* World = GetWorld())
	{
		AIStopUntil = FMath::Max(AIStopUntil, World->GetTimeSeconds() + Seconds);
	}
}

bool AGTPVehicle::IsAIStopRequested() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < AIStopUntil;
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

void AGTPVehicle::SetAIDriveInput(float Throttle, float Brake, float Steering, bool bHandbrake, bool bReverse)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || !HasAIDriver() || bWrecked)
	{
		return;
	}
	// The brake reverses a stopped car only when the AI asks for it: an AI car held on
	// the brake at a light or in a queue otherwise backed away (front-wheel drive, the
	// handbrake holds only the rear wheels). The player's pedals keep reverse-as-brake.
	Move->bReverseAsBrake = bReverse;
	// Out of reverse when the AI no longer asks for it: the automatic gearbox keeps gear -1
	// after backing out, and the throttle then drove the car backwards (-45 km/h measured).
	if (!bReverse && Move->GetCurrentGear() < 0)
	{
		Move->SetTargetGear(Throttle > 0.05f ? 1 : 0, true);
	}
	ThrottleAmount = FMath::Clamp(Throttle, 0.f, 1.f);
	Move->SetThrottleInput(ThrottleAmount);
	Move->SetBrakeInput(FMath::Clamp(Brake, 0.f, 1.f));
	Move->SetSteeringInput(FMath::Clamp(Steering, -1.f, 1.f));
	Move->SetHandbrakeInput(bHandbrake);

	// A car pulling away from a stop: a sleeping body ignores the pedals, and a
	// gearbox left in neutral by the ghost mode or a parked state stays there.
	if (ThrottleAmount > 0.05f && Brake < 0.05f && FMath::Abs(GetSpeedKmh()) < 1.f)
	{
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			if (Body->IsSimulatingPhysics() && !Body->IsAnyRigidBodyAwake())
			{
				Body->WakeAllRigidBodies();
			}
		}
		if (Move->GetCurrentGear() == 0)
		{
			Move->SetTargetGear(1, true);
		}
	}
	TraceDriveDiag(Brake, bHandbrake);
}

bool AGTPVehicle::IsDriveDiagWanted()
{
	static const bool bWanted = []
	{
		int32 Switch = 0;
		FParse::Value(FCommandLine::Get(), TEXT("DriveDiag="), Switch);
		return Switch != 0;
	}();
	return bWanted;
}

void AGTPVehicle::TraceDriveDiag(float Brake, bool bHandbrake)
{
	// -DriveDiag=1: every 5 s, the drivetrain of each AI-driven car within 60 m
	// of the player (gear, rpm, speed, pedals, controller, physics state).
	if (!IsDriveDiagWanted())
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() - LastDriveDiagTime < 5.f)
	{
		return;
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(6000.f))
	{
		return;
	}
	LastDriveDiagTime = World->GetTimeSeconds();
	const UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	UE_LOG(LogGTPVehicle, Log, TEXT("drive diag %s: gear=%d rpm=%.0f kmh=%.1f throttle=%.2f brake=%.2f handbrake=%d controller=%d simulating=%d awake=%d movetick=%d"),
		*GetName(), Move ? Move->GetCurrentGear() : -99, GetEngineRpm(), GetSpeedKmh(), ThrottleAmount, Brake, bHandbrake ? 1 : 0,
		GetController() ? 1 : 0, Body && Body->IsSimulatingPhysics() ? 1 : 0, Body && Body->IsAnyRigidBodyAwake() ? 1 : 0,
		Move && Move->IsComponentTickEnabled() ? 1 : 0);
}

void AGTPVehicle::SetScriptedDriveInput(float Throttle, float Brake, float Steering, bool bHandbrake)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || bWrecked)
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
		// The engine is still starting (or being hot-wired): nothing yet.
		ThrottleAmount = IsEngineRunning() ? FMath::Clamp(Value.Get<float>(), 0.f, 1.f) : 0.f;
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
		SteerInput = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
		Move->SetSteeringInput(SteerInput);
	}
}

void AGTPVehicle::OnSteerReleased()
{
	SteerInput = 0.f;
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
		bHandbrakeHeld = true;
		Move->SetHandbrakeInput(true);
	}
}

void AGTPVehicle::OnHandbrakeCompleted()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	bHandbrakeHeld = false;
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
	SteerInput = 0.f;
	bHandbrakeHeld = false;
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
	// A car taken from an AI driver: brake held while stopped reverses again (SetAIDriveInput).
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->bReverseAsBrake = true;
	}
	if (const AGTPPlayerController* PC = Cast<AGTPPlayerController>(NewController))
	{
		SetInteriorView(PC->GetCameraMode() && PC->GetCameraMode()->IsFirstPerson());
	}
	// The player's car gets the arcade torque (GTA pick-up); AI cars keep the real one.
	bPlayerAssist = Cast<APlayerController>(NewController) != nullptr;
	AppliedPower = -1.f;
	ChaseArm->SetComponentTickEnabled(bPlayerAssist);
}

void AGTPVehicle::UnPossessed()
{
	Super::UnPossessed();
	ClearDrivingInput();
	ChaseArm->SetComponentTickEnabled(false);
	if (bPlayerAssist)
	{
		bPlayerAssist = false;
		SetRearGrip(RearGripBase);
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetMaxEngineTorque(BaseTorque);
		}
	}
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
	if (const UWorld* World = GetWorld(); World && World->GetTimeSeconds() >= NextBodyCheckTime)
	{
		NextBodyCheckTime = World->GetTimeSeconds() + 1.f;
		KeepChassisBodyOnly();
	}
	if (State == EGTPVehicleState::Driven && IsLocallyControlled())
	{
		if (bPlayerAssist)
		{
			UpdateDriveAssist(DeltaSeconds);
		}
		UpdateChaseCamera(DeltaSeconds);
	}
}

void AGTPVehicle::SetRearGrip(float Multiplier)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || FMath::IsNearlyEqual(Multiplier, RearGripNow, 0.01f))
	{
		return;
	}
	RearGripNow = Multiplier;
	for (int32 Wheel = 2; Wheel < Move->Wheels.Num(); ++Wheel)
	{
		Move->SetWheelFrictionMultiplier(Wheel, Multiplier);
	}
}

void AGTPVehicle::UpdateDriveAssist(float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	if (!Move || !Body || !Body->IsSimulatingPhysics() || bWrecked)
	{
		return;
	}
	const float Speed = GetSpeedKmh();

	// Power: arcade torque, less of it when the car is badly damaged.
	const float Power = GTPCarjack::PowerFromHealth(GetHealthRatio(), Assist);
	if (!FMath::IsNearlyEqual(Power, AppliedPower, 0.02f))
	{
		AppliedPower = Power;
		Move->SetMaxEngineTorque(Assist.PlayerTorque * Power);
	}

	// Handbrake turns: less rear grip, and the tail swings with the steering.
	const bool bDrift = bHandbrakeHeld && FMath::Abs(Speed) >= Assist.DriftMinSpeedKmh;
	SetRearGrip(bDrift ? RearGripBase * Assist.DriftRearGrip : RearGripBase);
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	float SlipDeg = 0.f;
	if (Velocity.Size2D() > 300.f)
	{
		const FVector Heading = Velocity.GetSafeNormal2D();
		SlipDeg = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Forward, Heading).Z, FVector::DotProduct(Forward, Heading))));
	}
	const float YawRate = static_cast<float>(Body->GetPhysicsAngularVelocityInDegrees().Z);
	const float YawAccel = GTPCarjack::YawAssist(bHandbrakeHeld, Speed, SteerInput, SlipDeg, YawRate, Assist);
	if (YawAccel != 0.f)
	{
		Body->AddTorqueInDegrees(FVector(0.f, 0.f, YawAccel), NAME_None, true);
	}

	// A car beaten on one side pulls to that side.
	const float FullDent = FullDentDamage > 0.f ? FullDentDamage : 1.f;
	const float Pull = GTPCarjack::PullFromDents(FMath::Clamp(ZoneDamage[static_cast<int32>(EGTPDamageZone::Left)] / FullDent, 0.f, 1.f),
		FMath::Clamp(ZoneDamage[static_cast<int32>(EGTPDamageZone::Right)] / FullDent, 0.f, 1.f), Assist);
	if (Pull != 0.f)
	{
		Move->SetSteeringInput(FMath::Clamp(SteerInput + Pull * FMath::Clamp(FMath::Abs(Speed) / 30.f, 0.f, 1.f), -1.f, 1.f));
	}

	// On its roof or side and stopped: put back on its wheels after a moment, as in GTA.
	if (GetActorUpVector().Z < 0.25f && FMath::Abs(Speed) < 5.f)
	{
		UpsideDownTime += DeltaSeconds;
		if (UpsideDownTime > 2.5f)
		{
			UpsideDownTime = 0.f;
			SetActorLocationAndRotation(GetActorLocation() + FVector(0.f, 0.f, 120.f), FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr,
				ETeleportType::TeleportPhysics);
			Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			NotePlaced(TEXT("flipped back"));
			GTP_TRACE(LogGTPVehicle, TEXT("%s: put back on its wheels"), *GetName());
		}
	}
	else
	{
		UpsideDownTime = 0.f;
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
	ChaseCamera->SetFieldOfView(FMath::Lerp(BaseFOV, MaxSpeedFOV, SpeedAlpha) + ImpactShake * 4.f);
	// Further back at speed, so the road ahead stays in view.
	ChaseArm->TargetArmLength = FMath::FInterpTo(ChaseArm->TargetArmLength, ChaseDistance + SpeedAlpha * ChaseSpeedExtraDistance, DeltaSeconds, 2.f);
	// Impact jolt, fading in a fraction of a second.
	if (ImpactShake > 0.01f)
	{
		ChaseCamera->SetRelativeLocation(FVector(0.f, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * ImpactShake * ImpactShakeCm);
		ImpactShake *= FMath::Exp(-8.f * DeltaSeconds);
	}
	else if (ImpactShake > 0.f)
	{
		ImpactShake = 0.f;
		ChaseCamera->SetRelativeLocation(FVector::ZeroVector);
	}

	if (bInteriorView || bLookingBehind)
	{
		return;
	}
	// GTA-style: after a short pause in look input, swing back behind the car.
	const float SinceLook = GetWorld()->GetTimeSeconds() - LastLookInputTime;
	if (SinceLook > RecenterDelay && Speed > RecenterMinSpeedKmh)
	{
		const FRotator Current = PC->GetControlRotation();
		// In a slide the camera looks part of the way towards where the car is going.
		float TargetYaw = static_cast<float>(GetActorRotation().Yaw);
		const FVector Velocity = GetVelocity();
		if (Velocity.Size2D() > 500.f && GetSpeedKmh() > 0.f)
		{
			const float Slide = FRotator::NormalizeAxis(static_cast<float>(Velocity.Rotation().Yaw) - TargetYaw);
			TargetYaw += FMath::Clamp(Slide, -60.f, 60.f) * VelocityLookWeight;
		}
		const FRotator Target(ChasePitch, TargetYaw, 0.f);
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
	SetParkedAsleep(false);
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
		if (DriverController && DriverController->IsPlayerController())
		{
			// The driver pulled out of the car calls the police.
			ReportPlayerCrime(this, EGTPCrimeKind::CarTheft, Driver != nullptr);
		}
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
			// A non-player driver runs off, comes back at the thief, or cowers (GTPCarjackLogic.h).
			if (Cast<AAIController>(Victim->GetController()))
			{
				UGTPCarjackVictimComponent::Start(Victim, this, Character);
			}
		}
		Duration = PlayMontageOr(Character, CarjackMontage, CarjackDuration);
		break;
	case EGTPEntryKind::BreakWindow:
		if (DriverController && DriverController->IsPlayerController())
		{
			ReportPlayerCrime(this, EGTPCrimeKind::CarTheft);
		}
		BreakGlass(ZoneOfSide(DriverSide));
		TriggerAlarm();
		Duration = PlayMontageOr(Character, BreakWindowMontage, BreakWindowDuration);
		break;
	default:
		Duration = PlayMontageOr(Character, EnterMontage, EnterDuration);
		break;
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: entering (%s, %.1f s)"), *GetName(),
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
	// A parked car starts (or is hot-wired through the broken window); a car taken from its
	// driver is already running.
	if (PendingEntry != EGTPEntryKind::Carjack)
	{
		const float Start = PendingEntry == EGTPEntryKind::BreakWindow ? HotwireSeconds : EngineStartSeconds;
		EngineStartUntil = GetWorld()->GetTimeSeconds() + Start;
		GTP_TRACE(LogGTPVehicle, TEXT("%s: engine starting (%.1f s)"), *GetName(), Start);
	}
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
	if (IsDriverLiteWanted() && !Character->IsPlayerControlled())
	{
		SetDriverLite(Character, true);
	}
}

bool AGTPVehicle::IsDriverLiteWanted()
{
	static const bool bWanted = []
	{
		int32 Switch = 1;
		FParse::Value(FCommandLine::Get(), TEXT("DriverLite="), Switch);
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle: seated drivers %s (-DriverLite=%d)"), Switch != 0 ? TEXT("lite") : TEXT("whole"), Switch);
		return Switch != 0;
	}();
	return bWanted;
}

void AGTPVehicle::SetDriverLite(ACharacter* Character, bool bLite)
{
	static const FName LiteTag(TEXT("GTPDriverLite"));
	static const FName TickTag(TEXT("GTPLiteTick"));
	static const FName ActiveTag(TEXT("GTPLiteActive"));
	if (!Character || Character->Tags.Contains(LiteTag) == bLite)
	{
		return;
	}
	if (bLite)
	{
		Character->Tags.Add(LiteTag);
	}
	else
	{
		Character->Tags.Remove(LiteTag);
	}
	Character->SetActorTickEnabled(!bLite);
	if (AController* Brain = Character->GetController())
	{
		Brain->SetActorTickEnabled(!bLite);
	}
	if (bLite)
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_None);
		}
	}
	TInlineComponentArray<UActorComponent*> Parts(Character);
	for (UActorComponent* Part : Parts)
	{
		// Meshes keep their pose update: a visible driver stays seated, not frozen.
		if (Part->IsA<USkeletalMeshComponent>())
		{
			continue;
		}
		if (bLite)
		{
			if (Part->IsComponentTickEnabled())
			{
				Part->ComponentTags.AddUnique(TickTag);
				Part->SetComponentTickEnabled(false);
			}
			const bool bView = Part->IsA<UCameraComponent>() || Part->IsA<USpringArmComponent>()
				|| Part->GetClass()->GetName().Contains(TEXT("Camera"));
			if (bView && Part->IsActive())
			{
				Part->ComponentTags.AddUnique(ActiveTag);
				Part->Deactivate();
			}
		}
		else
		{
			if (Part->ComponentTags.Remove(TickTag) > 0)
			{
				Part->SetComponentTickEnabled(true);
			}
			// An AI character pulled out never needs its camera: activating a gameplay camera
			// creates a camera host and can take the player's view. Only a player gets it back.
			if (Part->ComponentTags.Remove(ActiveTag) > 0 && Character->IsPlayerControlled())
			{
				Part->Activate();
			}
		}
	}
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
	SetDriverLite(Character, false);
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

void AGTPVehicle::PanicExit(const FVector& ThreatLocation)
{
	if (!Driver || Cast<APlayerController>(GetController()))
	{
		return;
	}
	// Traffic drivers keep the car Parked while driving (HasAIDriver): they had no way out.
	if (HasAIDriver())
	{
		AIGetOut(true, ThreatLocation);
		return;
	}
	if (State != EGTPVehicleState::Driven)
	{
		return;
	}
	bFleeAfterExit = true;
	FleeFrom = ThreatLocation;
	RequestExit();
}

void AGTPVehicle::AIGetOut(bool bFlee, const FVector& From)
{
	if (!HasAIDriver() || bWrecked)
	{
		return;
	}
	bFleeAfterExit = bFlee;
	FleeFrom = From;
	SetAIDriveInput(0.f, 0.f, 0.f, false);
	// Exiting: no longer traffic (HasAIDriver), the AI driver stops ticking and the network
	// forgets the car. Stops first, the driver does not jump out of a moving car.
	State = EGTPVehicleState::Exiting;
	ClearDrivingInput();
	ExitWaitTime = 0.f;
	PollExit();
	if (State == EGTPVehicleState::Exiting)
	{
		GetWorldTimerManager().SetTimer(SequenceTimer, this, &AGTPVehicle::PollExit, ExitPollInterval, true);
	}
}

void AGTPVehicle::TriggerAlarm()
{
	if (!bAlarm || bWrecked || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bWasOn = AlarmUntil > Now;
	AlarmUntil = Now + AlarmSeconds;
	if (!bWasOn)
	{
		GTP_TRACE(LogGTPVehicle, TEXT("%s: alarm"), *GetName());
		bAlarmHornOn = false;
		PulseAlarm();
		GetWorldTimerManager().SetTimer(AlarmTimer, this, &AGTPVehicle::PulseAlarm, 0.45f, true);
	}
}

void AGTPVehicle::PulseAlarm()
{
	// The horn on and off, twice a second, until AlarmUntil.
	const bool bOver = !GetWorld() || GetWorld()->GetTimeSeconds() >= AlarmUntil || bWrecked;
	bAlarmHornOn = bOver ? false : !bAlarmHornOn;
	UGTPHornComponent::SetHorn(this, bAlarmHornOn);
	if (bOver)
	{
		GetWorldTimerManager().ClearTimer(AlarmTimer);
	}
}

void AGTPVehicle::SetParkedAsleep(bool bAsleep)
{
	if (bParkedAsleep == bAsleep || (bAsleep && (Driver || State != EGTPVehicleState::Parked)))
	{
		return;
	}
	bParkedAsleep = bAsleep;
	USkeletalMeshComponent* Body = GetMesh();
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetComponentTickEnabled(!bAsleep);
	}
	if (Body)
	{
		Body->SetSimulatePhysics(!bAsleep);
		if (!bAsleep)
		{
			// From rest: no speed carried over from the kinematic sleep.
			Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}
	if (!bAsleep)
	{
		NotePlaced(TEXT("parked wake"));
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

ACharacter* AGTPVehicle::EjectDriverTo(const FVector& Location, const FRotator& Rotation)
{
	GetWorldTimerManager().ClearTimer(SequenceTimer);
	ACharacter* Character = Driver;
	if (!Character)
	{
		return nullptr;
	}
	AController* PawnController = GetController();
	Driver = nullptr;
	State = EGTPVehicleState::Parked;
	ReleaseCharacter(Character, Location, Rotation);
	if (PawnController && PawnController->IsPlayerController())
	{
		PawnController->Possess(Character);
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: driver taken out to %s"), *GetName(), *Location.ToCompactString());
	return Character;
}

void AGTPVehicle::Drown()
{
	if (!bWrecked)
	{
		Health = 0.f;
		SetWrecked(EGTPVehicleLoss::Drowned);
	}
	ClearDrivingInput();
	GTP_TRACE(LogGTPVehicle, TEXT("%s: drowned"), *GetName());
}

void AGTPVehicle::DoExit(bool bBailOut)
{
	GetWorldTimerManager().ClearTimer(SequenceTimer);
	ACharacter* Character = Driver;
	AController* PawnController = GetController();
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
		GTP_TRACE(LogGTPVehicle, TEXT("%s: no free exit"), *GetName());
		bFleeAfterExit = false;
		// A traffic driver stays seated in a stopped car (it is no longer traffic); the
		// player can drive out.
		State = PawnController && PawnController->IsPlayerController() ? EGTPVehicleState::Driven : EGTPVehicleState::Parked;
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
	if (PawnController)
	{
		PawnController->Possess(Character);
	}

	if (bBailOut)
	{
		// Thrown sideways with part of the car's speed; the car rolls on.
		const FVector Out = (Location - GetActorLocation()).GetSafeNormal2D();
		Character->LaunchCharacter(Out * BailOutLaunchSpeed + CarVelocity * 0.6f + FVector(0.f, 0.f, 200.f), true, true);
		// Stumble, fall or be thrown depending on the speed (ADR-0006), when the character reacts.
		if (UGTPPlayerReactionComponent* Reaction = Character->FindComponentByClass<UGTPPlayerReactionComponent>())
		{
			Reaction->ReactToBailOut(CarVelocity);
		}
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
	if (bFleeAfterExit)
	{
		// Threatened: runs away from the gun, no navigation mesh needed.
		bFleeAfterExit = false;
		if (AAIController* Brain = Cast<AAIController>(Character->GetController()))
		{
			FVector Away = (Location - FleeFrom).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = -GetActorRightVector();
			}
			Brain->MoveToLocation(Location + Away * VictimFleeDistance * 2.f, 50.f, false, false);
		}
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: driver out (%s)"), *GetName(), bBailOut ? TEXT("bail out") : TEXT("stopped"));
}

// --- Damage ----------------------------------------------------------------

void AGTPVehicle::KeepChassisBodyOnly()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || Body->Bodies.Num() <= 1)
	{
		return;
	}
	static const bool bAllBodies = FParse::Param(FCommandLine::Get(), TEXT("CarAllBodies"));
	if (bAllBodies)
	{
		return;
	}
	// The body Chaos drives: the component's root body.
	const FBodyInstance* Chassis = Body->GetBodyInstance();
	int32 BodiesOff = 0;
	int32 ConstraintsOff = 0;
	for (FConstraintInstance* Constraint : Body->Constraints)
	{
		if (Constraint && Constraint->IsValidConstraintInstance())
		{
			Constraint->TermConstraint();
			++ConstraintsOff;
		}
	}
	for (FBodyInstance* Part : Body->Bodies)
	{
		if (Part && Part != Chassis && Part->IsValidBodyInstance())
		{
			Part->TermBody();
			++BodiesOff;
		}
	}
	if (BodiesOff == 0 && ConstraintsOff == 0)
	{
		return;
	}
	static TSet<FName> Logged;
	if (!Logged.Contains(GetClass()->GetFName()))
	{
		Logged.Add(GetClass()->GetFName());
		const FBox Local = Body->CalcBounds(FTransform::Identity).GetBox();
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle physics: %s keeps its chassis body (%s, bone %s) only: %d other bodies and %d constraints of %s switched off; collision %s, chassis box %.0f x %.0f x %.0f cm, bottom %.0f cm"),
			*GetClass()->GetName(), Chassis && Chassis->BodySetup.IsValid() ? TEXT("found") : TEXT("none"),
			Chassis && Chassis->BodySetup.IsValid() ? *Chassis->BodySetup->BoneName.ToString() : TEXT("-"), BodiesOff, ConstraintsOff,
			Body->GetPhysicsAsset() ? *Body->GetPhysicsAsset()->GetName() : TEXT("no physics asset"), *Body->GetCollisionProfileName().ToString(),
			Local.GetSize().X, Local.GetSize().Y, Local.GetSize().Z, Local.Min.Z);
	}
}

void AGTPVehicle::NoteIgnoredImpact(const AActor* OtherActor, float DeltaV, const TCHAR* Why)
{
	++IgnoredSinceLog;
	IgnoredMaxDeltaV = FMath::Max(IgnoredMaxDeltaV, DeltaV);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - IgnoredLogTime < 2.f)
	{
		return;
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: %d impacts ignored since the last line (%s; last from %s, up to %.0f cm/s)"), *GetName(), IgnoredSinceLog, Why,
		*GetNameSafe(OtherActor), IgnoredMaxDeltaV);
	IgnoredLogTime = Now;
	IgnoredSinceLog = 0;
	IgnoredMaxDeltaV = 0.f;
}

void AGTPVehicle::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Two bodies of the same car touching each other is no impact.
	if (OtherActor == this)
	{
		return;
	}
	// Crimes by the player at the wheel: running into people, cars and police.
	if (OtherActor && State == EGTPVehicleState::Driven && Cast<APlayerController>(GetController()) && FMath::Abs(GetSpeedKmh()) > 12.f)
	{
		if (OtherActor->IsA<ACharacter>())
		{
			ReportPlayerCrime(this, EGTPCrimeKind::HitPedestrian, true);
		}
		else if (OtherActor->IsA<AGTPVehicle>())
		{
			ReportPlayerCrime(this, OtherActor->ActorHasTag(GTPPolice::PoliceTag) ? EGTPCrimeKind::HitPolice : EGTPCrimeKind::HitVehicle);
		}
	}
	// Rammed or touched by the player: the traffic driver reacts (GTPTrafficReactLogic.h).
	if (const AGTPVehicle* Rammer = Cast<AGTPVehicle>(OtherActor))
	{
		bHitByPlayer |= Cast<APlayerController>(Rammer->GetController()) != nullptr;
	}
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
	if (bPlayerAssist)
	{
		ImpactShake = FMath::Max(ImpactShake, FMath::Clamp(DeltaV / 1200.f, 0.2f, 1.f));
	}
	// Traffic does not wreck itself: a hit between two AI cars, or from a moving kinematic body (a
	// ghost car teleported along its lane, a pedestrian capsule), deals no damage unless the player's
	// car is one of the two. Ghost cars carry an infinite mass, so brushing one gave a driven car
	// enough speed change to wreck at once. -TrafficCrashDamage=1 restores the old rule.
	static const bool bTrafficCrashDamage = []
	{
		int32 Switch = 0;
		FParse::Value(FCommandLine::Get(), TEXT("TrafficCrashDamage="), Switch);
		return Switch != 0;
	}();
	const AGTPVehicle* OtherCar = Cast<AGTPVehicle>(OtherActor);
	const bool bPlayerInvolved = Cast<APlayerController>(GetController()) != nullptr
		|| (OtherCar && Cast<APlayerController>(OtherCar->GetController()) != nullptr);
	const bool bKinematicOther = OtherComp && OtherComp->Mobility == EComponentMobility::Movable && !OtherComp->IsSimulatingPhysics();
	if (!bPlayerInvolved && OtherCar && HasAIDriver() && OtherCar->HasAIDriver() && Now - LastPlacedTime > 3.f && Now - OtherCar->LastPlacedTime > 3.f)
	{
		// Two traffic cars crashed for real (not a ghost brushing or a placement): the drivers
		// react (GTPTrafficIncident.h). Reported once, by the car with the lower address.
		if (this < OtherCar)
		{
			if (const UGTPAIDriverComponent* Mine = FindComponentByClass<UGTPAIDriverComponent>())
			{
				if (AGTPTrafficNetwork* Network = Mine->GetNetwork(); Network && !Mine->IsGhost())
				{
					Network->NotifyTrafficCrash(this, const_cast<AGTPVehicle*>(OtherCar), DeltaV);
				}
			}
		}
	}
	if (!bTrafficCrashDamage && !bPlayerInvolved && (OtherCar || bKinematicOther))
	{
		NoteIgnoredImpact(OtherActor, DeltaV, TEXT("between AI cars"));
		return;
	}
	const float SincePlaced = Now - LastPlacedTime;
	if (!bTrafficCrashDamage && !bPlayerInvolved && SincePlaced < 1.5f)
	{
		NoteIgnoredImpact(OtherActor, DeltaV, LastPlacedHow);
		return;
	}
	LastImpactTime = Now;
	const float HealthBefore = Health;
	ApplyZoneDamage(ZoneFromWorldLocation(Hit.ImpactPoint), (DeltaV - MinImpactSpeed) * DamagePerImpactSpeed);
	if (bWrecked && HealthBefore > 0.f)
	{
		const USkeletalMeshComponent* Body = GetMesh();
		GTP_TRACE(LogGTPVehicle, TEXT("%s: wrecked by an impact with %s (%.0f cm/s) | %s%s, %s, %.0f km/h, %.1f s after %s"), *GetName(),
			*GetNameSafe(OtherActor), DeltaV, *GetClass()->GetName(), ActorHasTag(GTPPolice::PoliceTag) ? TEXT(" police") : TEXT(""),
			Body && Body->IsSimulatingPhysics() ? TEXT("simulated") : TEXT("kinematic"), GetSpeedKmh(), SincePlaced, LastPlacedHow);
	}
}

void AGTPVehicle::NotePlaced(const TCHAR* How)
{
	LastPlacedTime = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	LastPlacedHow = How;
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
	if (!Driver && State == EGTPVehicleState::Parked)
	{
		SetParkedAsleep(false);
		TriggerAlarm();
	}

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

FGTPVehicleWrecked& AGTPVehicle::OnVehicleWrecked()
{
	static FGTPVehicleWrecked Lost;
	return Lost;
}

void AGTPVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		OnVehicleWrecked().Broadcast(this, EGTPVehicleLoss::Destroyed);
	}
	Super::EndPlay(EndPlayReason);
}

void AGTPVehicle::SetWrecked(EGTPVehicleLoss Reason)
{
	const bool bFirst = !bWrecked;
	bWrecked = true;
	ThrottleAmount = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetThrottleInput(0.f);
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: wrecked"), *GetName());
	OnWrecked();
	if (bFirst)
	{
		OnVehicleWrecked().Broadcast(this, Reason);
	}
}
