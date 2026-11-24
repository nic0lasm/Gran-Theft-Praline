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
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GTPWeapon.h"
#include "GTPCameraProbe.h"

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
		SetBodyHidden(nullptr, false);
		Occluders.Clear();
	}
}

void UGTPFollowCameraComponent::SetBodyHidden(const ACharacter* Character, bool bHidden)
{
	// Owner-no-see would not work: the view target is our camera actor, not the character. The visual attached to
	// the pawn (Nicolito) is hidden too: hiding only the pawn's own mesh left his legs and arms in the picture (gate 76).
	if (!bHidden || !Character)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Part : HiddenBodyParts)
		{
			if (Part.IsValid())
			{
				Part->SetHiddenInGame(false);
				Part->SetCastHiddenShadow(false);
			}
		}
		HiddenBodyParts.Reset();
		return;
	}
	if (HiddenBodyParts.Num() > 0)
	{
		return;
	}
	TArray<AActor*> Actors;
	const_cast<ACharacter*>(Character)->GetAttachedActors(Actors, false, true);
	Actors.Add(const_cast<ACharacter*>(Character));
	for (AActor* Actor : Actors)
	{
		if (!Actor || Actor == Camera)
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Parts(Actor);
		for (UPrimitiveComponent* Part : Parts)
		{
			if (Part && Part->IsVisible() && !Part->bHiddenInGame)
			{
				Part->SetCastHiddenShadow(true);
				Part->SetHiddenInGame(true);
				HiddenBodyParts.Add(Part);
			}
		}
	}
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
	CurrentLift = 0.f;
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

	if (!bAutoRecenter || AimAlpha > 0.05f)
	{
		return; // never drift while aiming a gun
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

	// Aiming a gun: in over the shoulder the weapon names (GTA-like).
	const UGTPWeaponComponent* Weapon = Character->FindComponentByClass<UGTPWeaponComponent>();
	// -NoAimFraming keeps the free framing while aiming: the whole body in view, to judge the aim pose itself.
	static const bool bNoAimFraming = FParse::Param(FCommandLine::Get(), TEXT("NoAimFraming"));
	const bool bAiming = Weapon && Weapon->IsAiming() && !bNoAimFraming;
	const float TargetSide = (Weapon && !Weapon->IsRightShoulder()) ? -1.f : 1.f;
	const float AimRate = SmoothAlpha(AimBlendSpeed, DeltaTime);
	AimAlpha = bInstant ? (bAiming ? 1.f : 0.f) : FMath::Lerp(AimAlpha, bAiming ? 1.f : 0.f, AimRate);
	ShoulderSide = bInstant ? TargetSide : FMath::Lerp(ShoulderSide, TargetSide, AimRate);

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
	TargetArm = FMath::Lerp(TargetArm, AimArmLength, AimAlpha);
	// The aim framing follows at its own (faster) rate, the free framing at ArmSpeed.
	CurrentArm = bInstant ? TargetArm : FMath::Lerp(CurrentArm, TargetArm, SmoothAlpha(FMath::Lerp(ArmSpeed, AimBlendSpeed, AimAlpha), DeltaTime));
	const FVector2D Shoulder = FMath::Lerp(ShoulderOffset, AimShoulderOffset, AimAlpha);

	// Aiming, the framing follows the head down (or up): the aim pose may lower it well under the pivot.
	float TargetHeadOffset = 0.f;
	if (bAimFollowsHead && AimAlpha > 0.01f)
	{
		static const FName HeadBone(TEXT("head"));
		const USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (Mesh && Mesh->GetBoneIndex(HeadBone) != INDEX_NONE)
		{
			const float Offset = static_cast<float>(Mesh->GetSocketLocation(HeadBone).Z - RawPivot.Z);
			TargetHeadOffset = FMath::Clamp(Offset, -AimHeadMaxDrop, AimHeadMaxRise) * AimAlpha;
		}
	}
	AimHeadOffset = bInstant ? TargetHeadOffset : FMath::Lerp(AimHeadOffset, TargetHeadOffset, AimRate);
	const FVector Desired = SmoothedPivot + FVector(0.f, 0.f, AimHeadOffset)
		+ Rotation.RotateVector(FVector(-CurrentArm, Shoulder.X * ShoulderSide, Shoulder.Y));

	// Collision, as in the genre: walls, ground and cars pull the arm in; people and small props (poles, bins,
	// bollards) do not, they are hidden while in the way. Squeezed, the camera rises over the shoulder instead of
	// pressing into the back of the head (gate 76: on the roofline against a wall, in the granite, legs in the picture).
	float HitFraction = 1.f;
	FVector Pivot = RawPivot;
	FVector Wanted = Desired;
	TArray<const AActor*> Ignored{Character, Camera};
	{
		TArray<AActor*> Attached;
		Character->GetAttachedActors(Attached, false, true);
		Ignored.Append(Attached);
	}
	if (UWorld* World = GetWorld())
	{
		FString Wall;
		const float Flat = GTPCameraProbe::Sweep(World, RawPivot, Desired, ProbeRadius, SmallObjectRadius, Ignored, &Wall);
		const float TargetLift = Flat < 1.f ? SqueezeLift * (1.f - Flat) : 0.f;
		CurrentLift = bInstant ? TargetLift : FMath::Lerp(CurrentLift, TargetLift, SmoothAlpha(TargetLift > CurrentLift ? 8.f : CollisionRecoverSpeed, DeltaTime));
		HitFraction = Flat;
		if (CurrentLift > 1.f)
		{
			// Up from the pivot as far as the ceiling allows, then the same arm from there.
			const FVector Up(0.f, 0.f, CurrentLift);
			const float Room = GTPCameraProbe::Sweep(World, RawPivot, RawPivot + Up, ProbeRadius, SmallObjectRadius, Ignored);
			const FVector Raised = RawPivot + Up * Room;
			const float High = GTPCameraProbe::Sweep(World, Raised, Desired + Up * Room, ProbeRadius, SmallObjectRadius, Ignored);
			if (High > Flat)
			{
				Pivot = Raised;
				Wanted = Desired + Up * Room;
				HitFraction = High;
			}
		}
		if (Flat < 1.f && !bSqueezed)
		{
			UE_LOG(LogGTPFollowCamera, Verbose, TEXT("Camera squeezed by %s (%.0f%% of the arm)"), *Wall, Flat * 100.f);
		}
		bSqueezed = Flat < 1.f;
	}
	if (bInstant || HitFraction < CollisionFraction)
	{
		CollisionFraction = HitFraction;
	}
	else
	{
		CollisionFraction = FMath::Lerp(CollisionFraction, HitFraction, SmoothAlpha(CollisionRecoverSpeed, DeltaTime));
	}
	const FVector Location = Pivot + (Wanted - Pivot) * CollisionFraction;
	CurrentDistance = FVector::Dist(Location, RawPivot);
	// Raised, the view tips down towards the shoulders so the player stays in the picture.
	FRotator ViewRotation = Rotation;
	if (CurrentLift > 1.f)
	{
		const double Horizontal = FMath::Max(50.0, FVector::Dist2D(Location, RawPivot));
		const double Drop = Location.Z - RawPivot.Z;
		ViewRotation.Pitch = FMath::Min(ViewRotation.Pitch, ViewRotation.Pitch - FMath::RadiansToDegrees(FMath::Atan2(FMath::Max(0.0, Drop - Shoulder.Y), Horizontal)) * 0.7);
	}

	const float TargetFOV = FMath::Lerp(FMath::Lerp(BaseFOV, SprintFOV, SprintAlpha), AimFOV, AimAlpha);
	CurrentFOV = bInstant ? TargetFOV : FMath::Lerp(CurrentFOV, TargetFOV, SmoothAlpha(FOVSpeed, DeltaTime));

	Camera->SetActorLocationAndRotation(Location, ViewRotation);
	Camera->GetCameraComponent()->SetFieldOfView(CurrentFOV);

	// Pressed against a wall the camera ends up in the head: hide the body for the player only (shadow kept).
	SetBodyHidden(Character, CurrentDistance < MinDistance);
	// People and small props between the camera and the player's chest, hidden while they are in the way.
	if (bHideOccluders)
	{
		const int32 Count = Occluders.Update(GetWorld(), Location, RawPivot - FVector(0.f, 0.f, 20.f), OccluderRadius, SmallObjectRadius, Ignored);
		LastOccluderCount = Count;
	}
	// One line every 10 s for the gate: how often the arm was squeezed, how far it rose, what was hidden.
	if (!bInstant)
	{
		StatSeconds += DeltaTime;
		StatSqueezed += bSqueezed ? DeltaTime : 0.f;
		StatBodyHidden += HiddenBodyParts.Num() > 0 ? DeltaTime : 0.f;
		StatMaxLift = FMath::Max(StatMaxLift, CurrentLift);
		StatMinDistance = FMath::Min(StatMinDistance, CurrentDistance);
		StatMaxOccluders = FMath::Max(StatMaxOccluders, LastOccluderCount);
		if (StatSeconds >= 10.f)
		{
			UE_LOG(LogGTPFollowCamera, Log, TEXT("on-foot camera (10 s): squeezed %.0f%%, raised up to %.0f cm, nearest %.0f cm from the shoulders, body hidden %.0f%%, up to %d parts hidden in the way"),
				100.f * StatSqueezed / StatSeconds, StatMaxLift, StatMinDistance, 100.f * StatBodyHidden / StatSeconds, StatMaxOccluders);
			StatSeconds = StatSqueezed = StatBodyHidden = StatMaxLift = 0.f;
			StatMinDistance = 1e6f;
			StatMaxOccluders = 0;
		}
	}
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
	Occluders.Clear();
	SetBodyHidden(nullptr, false);
	if (Camera)
	{
		Camera->Destroy();
		Camera = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
