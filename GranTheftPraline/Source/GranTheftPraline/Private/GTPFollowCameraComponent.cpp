#include "GTPFollowCameraComponent.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPFollowCamera, Log, All);

namespace
{
	/** Frame-rate independent smoothing factor for a rate in 1/s (0 = no smoothing). */
	float SmoothAlpha(float Rate, float DeltaTime)
	{
		return Rate > 0.f ? 1.f - FMath::Exp(-Rate * DeltaTime) : 1.f;
	}
}

UGTPFollowCameraComponent::UGTPFollowCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After character movement, before the camera manager reads the view target.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

APlayerController* UGTPFollowCameraComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

ACameraActor* UGTPFollowCameraComponent::EnsureCamera()
{
	if (!Camera && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		if (Camera)
		{
			UCameraComponent* Component = Camera->GetCameraComponent();
			Component->SetConstraintAspectRatio(false);
			Component->bOverrideAspectRatioAxisConstraint = false;
			Component->SetFieldOfView(BaseFOV);
		}
	}
	return Camera;
}

AActor* UGTPFollowCameraComponent::GetViewTargetFor(APawn* Pawn, bool bSnap)
{
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!bEnabled || !Character)
	{
		return nullptr;
	}
	ACameraActor* View = EnsureCamera();
	if (View && (bSnap || !bHasState))
	{
		Snap(Character);
	}
	return View;
}

void UGTPFollowCameraComponent::SetDriving(bool bDrive)
{
	bDriving = bDrive && bEnabled;
	SetComponentTickEnabled(bDriving);
	if (!bDriving)
	{
		bManualRecenter = false;
		SetBodyHidden(HiddenBody.Get(), false);
	}
}

void UGTPFollowCameraComponent::SetBodyHidden(USkeletalMeshComponent* Mesh, bool bHidden)
{
	if (HiddenBody.IsValid() && HiddenBody.Get() != Mesh)
	{
		HiddenBody->SetOwnerNoSee(false);
		HiddenBody = nullptr;
	}
	if (!Mesh || Mesh->bOwnerNoSee == bHidden)
	{
		return;
	}
	Mesh->SetOwnerNoSee(bHidden);
	HiddenBody = bHidden ? Mesh : nullptr;
}

void UGTPFollowCameraComponent::NotifyLookInput()
{
	if (const UWorld* World = GetWorld())
	{
		LastLookInputTime = World->GetTimeSeconds();
	}
	// The player took the camera: a running manual recenter gives way.
	bManualRecenter = false;
}

void UGTPFollowCameraComponent::RequestRecenter()
{
	APlayerController* PC = GetPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!bDriving || !Pawn)
	{
		return;
	}
	bManualRecenter = true;
	ManualRecenterElapsed = 0.f;
	ManualRecenterFrom = PC->GetControlRotation();
	ManualRecenterFrom.Pitch = FRotator::NormalizeAxis(ManualRecenterFrom.Pitch);
	ManualRecenterTo = FRotator(RecenterPitch, Pawn->GetActorRotation().Yaw, 0.f);
}

void UGTPFollowCameraComponent::Snap(const ACharacter* Character)
{
	bHasState = false;
	CollisionFraction = 1.f;
	CurrentFOV = BaseFOV;
	CurrentArm = ArmLength;
	if (APlayerController* PC = GetPlayerController())
	{
		UpdateCamera(PC, Character, 0.f, true);
	}
}

void UGTPFollowCameraComponent::UpdateRecenter(APlayerController* PC, const ACharacter* Character, float DeltaTime)
{
	if (bManualRecenter)
	{
		ManualRecenterElapsed += DeltaTime;
		const float Alpha = RecenterBlendTime > 0.f ? FMath::Clamp(ManualRecenterElapsed / RecenterBlendTime, 0.f, 1.f) : 1.f;
		const float Eased = FMath::SmoothStep(0.f, 1.f, Alpha);
		FRotator Rotation;
		Rotation.Yaw = ManualRecenterFrom.Yaw + FMath::FindDeltaAngleDegrees(ManualRecenterFrom.Yaw, ManualRecenterTo.Yaw) * Eased;
		Rotation.Pitch = FMath::Lerp(ManualRecenterFrom.Pitch, ManualRecenterTo.Pitch, Eased);
		Rotation.Roll = 0.f;
		PC->SetControlRotation(Rotation);
		if (Alpha >= 1.f)
		{
			bManualRecenter = false;
			LastLookInputTime = GetWorld()->GetTimeSeconds();
		}
		return;
	}

	if (!bAutoRecenter)
	{
		return;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector Velocity = Character->GetVelocity() * FVector(1.f, 1.f, 0.f);
	const float Speed = Velocity.Size();
	if (Speed < MinRecenterSpeed || (Movement && Movement->IsFalling())
		|| GetWorld()->GetTimeSeconds() - LastLookInputTime < RecenterDelay)
	{
		return;
	}
	FRotator Control = PC->GetControlRotation();
	const float YawGap = FMath::FindDeltaAngleDegrees(Control.Yaw, Velocity.Rotation().Yaw);
	if (FMath::Abs(YawGap) > MaxRecenterAngle)
	{
		// Walking towards the camera: let the player see the face, as on foot in the genre.
		return;
	}
	// Faster travel drifts faster; a slow walk barely moves the view.
	const float SpeedScale = FMath::Clamp(Speed / 450.f, 0.f, 1.5f);
	const float Alpha = SmoothAlpha(RecenterRate * SpeedScale, DeltaTime);
	const float Pitch = static_cast<float>(FRotator::NormalizeAxis(Control.Pitch));
	const float MaxStep = MaxRecenterTurnRate * DeltaTime;
	Control.Yaw += FMath::Clamp(YawGap * Alpha, -MaxStep, MaxStep);
	Control.Pitch = Pitch + (RecenterPitch - Pitch) * Alpha * 0.5f;
	PC->SetControlRotation(Control);
}

void UGTPFollowCameraComponent::UpdateCamera(APlayerController* PC, const ACharacter* Character, float DeltaTime, bool bInstant)
{
	if (!Camera)
	{
		return;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bFalling = Movement && Movement->IsFalling();
	const bool bCrouched = Character->bIsCrouched;
	const float Speed = Character->GetVelocity().Size2D();

	// Pivot: shoulders, trailing the character a little.
	const FVector RawPivot = Character->GetActorLocation() + FVector(0.f, 0.f, PivotHeight);
	if (bInstant || !bHasState)
	{
		SmoothedPivot = RawPivot;
		SmoothedRotation = PC->GetControlRotation();
	}
	else
	{
		const double HorizontalAlpha = SmoothAlpha(HorizontalLagSpeed, DeltaTime);
		const float VerticalAlpha = SmoothAlpha(bFalling ? AirVerticalLagSpeed : VerticalLagSpeed, DeltaTime);
		FVector2D Planar = FMath::Lerp(FVector2D(SmoothedPivot), FVector2D(RawPivot), HorizontalAlpha);
		const FVector2D Trail = Planar - FVector2D(RawPivot);
		if (Trail.SizeSquared() > FMath::Square(MaxHorizontalLag))
		{
			Planar = FVector2D(RawPivot) + Trail.GetSafeNormal() * MaxHorizontalLag;
		}
		double Height = FMath::Lerp(SmoothedPivot.Z, RawPivot.Z, (double)VerticalAlpha);
		Height = FMath::Clamp(Height, RawPivot.Z - (double)MaxVerticalLag, RawPivot.Z + (double)MaxVerticalLag);
		SmoothedPivot = FVector(Planar.X, Planar.Y, Height);

		const FRotator Control = PC->GetControlRotation();
		if (RotationLagSpeed > 0.f)
		{
			SmoothedRotation = FQuat::Slerp(SmoothedRotation.Quaternion(), Control.Quaternion(), SmoothAlpha(RotationLagSpeed, DeltaTime)).Rotator();
		}
		else
		{
			SmoothedRotation = Control;
		}
	}
	bHasState = true;
	const FRotator Rotation(FRotator::NormalizeAxis(SmoothedRotation.Pitch), SmoothedRotation.Yaw, 0.f);

	// Framing: farther when sprinting, closer crouched or looking up.
	const float SprintAlpha = SprintFullSpeed > SprintBlendStartSpeed
		? FMath::Clamp((Speed - SprintBlendStartSpeed) / (SprintFullSpeed - SprintBlendStartSpeed), 0.f, 1.f)
		: 0.f;
	float TargetArm = ArmLength + SprintArmExtra * SprintAlpha - (bCrouched ? CrouchArmReduction : 0.f);
	const float ViewPitch = static_cast<float>(Rotation.Pitch);
	if (ViewPitch > 0.f)
	{
		TargetArm *= FMath::Lerp(1.f, LookUpArmScale, FMath::Clamp(ViewPitch / 60.f, 0.f, 1.f));
	}
	CurrentArm = bInstant ? TargetArm : FMath::Lerp(CurrentArm, TargetArm, SmoothAlpha(ArmSpeed, DeltaTime));

	const FVector Desired = SmoothedPivot + Rotation.RotateVector(FVector(-CurrentArm, ShoulderOffset.X, ShoulderOffset.Y));

	// Collision: sweep from the unlagged pivot (inside the capsule, never in a wall).
	float HitFraction = 1.f;
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPFollowCamera), false, Character);
		Params.AddIgnoredActor(Camera);
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, RawPivot, Desired, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(ProbeRadius), Params)
			&& !Hit.bStartPenetrating)
		{
			HitFraction = Hit.Time;
		}
	}
	if (bInstant || HitFraction < CollisionFraction)
	{
		CollisionFraction = HitFraction;
	}
	else
	{
		CollisionFraction = FMath::Lerp(CollisionFraction, HitFraction, SmoothAlpha(CollisionRecoverSpeed, DeltaTime));
	}
	const FVector Location = RawPivot + (Desired - RawPivot) * CollisionFraction;
	CurrentDistance = FVector::Dist(Location, RawPivot);

	const float TargetFOV = FMath::Lerp(BaseFOV, SprintFOV, SprintAlpha);
	CurrentFOV = bInstant ? TargetFOV : FMath::Lerp(CurrentFOV, TargetFOV, SmoothAlpha(FOVSpeed, DeltaTime));

	Camera->SetActorLocationAndRotation(Location, Rotation);
	Camera->GetCameraComponent()->SetFieldOfView(CurrentFOV);

	// Pressed against a wall the camera ends up in the head: hide the body for the player only (shadow kept).
	SetBodyHidden(Character->GetMesh(), CurrentDistance < MinDistance);
}

void UGTPFollowCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	APlayerController* PC = GetPlayerController();
	const ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!bDriving || !Character || !Camera || PC->IsPaused())
	{
		return;
	}
	UpdateRecenter(PC, Character, DeltaTime);
	UpdateCamera(PC, Character, DeltaTime, false);

	// The sample character activates its own camera from Blueprint events that
	// may run after possession: take the view back unless a blend is running.
	const APlayerCameraManager* Manager = PC->PlayerCameraManager;
	if (PC->GetViewTarget() != Camera && Manager && Manager->PendingViewTarget.Target == nullptr)
	{
		UE_LOG(LogGTPFollowCamera, Log, TEXT("View target was %s, taking it back"), *GetNameSafe(PC->GetViewTarget()));
		PC->SetViewTarget(Camera);
	}
}

void UGTPFollowCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
