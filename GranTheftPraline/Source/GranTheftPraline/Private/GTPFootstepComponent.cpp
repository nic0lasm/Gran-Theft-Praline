#include "GTPFootstepComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPLog.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPFootsteps, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarDebugFeet(TEXT("gtp.Audio.DebugFeet"), 0,
		TEXT("1: log the foot heights above the ground and every step of the player (calibration); also -DebugFeet."));

	bool DebugFeet()
	{
		static const bool bParam = FParse::Param(FCommandLine::Get(), TEXT("DebugFeet"));
		return bParam || CVarDebugFeet.GetValueOnGameThread() != 0;
	}
}

UGTPFootstepComponent::UGTPFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// After the animation has posed the bones this frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UGTPFootstepComponent::BeginPlay()
{
	Super::BeginPlay();
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	PlantSettings.PlantHeight = Settings->FootPlantHeight;
	PlantSettings.LiftHysteresis = Settings->FootLiftHysteresis;
	FootBones[0] = Settings->LeftFootBone;
	FootBones[1] = Settings->RightFootBone;
	Feet[0].Reset();
	Feet[1].Reset();
	Body = FindBody();
}

USkeletalMeshComponent* UGTPFootstepComponent::FindBody() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (Mesh && Mesh->GetBoneIndex(FootBones[0]) != INDEX_NONE)
		{
			return Mesh;
		}
	}
	// Any visible skeletal mesh with the foot bones (GASP visual mesh, pedestrians).
	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetBoneIndex(FootBones[0]) != INDEX_NONE && Mesh->GetBoneIndex(FootBones[1]) != INDEX_NONE)
		{
			return Mesh;
		}
	}
	return nullptr;
}

bool UGTPFootstepComponent::ShouldListen() const
{
	const AActor* Owner = GetOwner();
	if (!bBudgetAllowed || !Owner || Owner->IsHidden() || Owner->GetAttachParentActor())
	{
		// Hidden or attached: sitting in a vehicle.
		return false;
	}
	const USkeletalMeshComponent* Mesh = Body.Get();
	if (!Mesh || !Mesh->IsVisible())
	{
		return false;
	}
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (Player)
	{
		FVector Listener, Front, Right;
		Player->GetAudioListenerPosition(Listener, Front, Right);
		const float MaxDistance = GetDefault<UGTPAudioSettings>()->FootstepMaxDistance;
		if (FVector::DistSquared(Listener, Owner->GetActorLocation()) > FMath::Square(MaxDistance))
		{
			return false;
		}
	}
	return true;
}

void UGTPFootstepComponent::UpdateGround(float DeltaTime)
{
	const AActor* Owner = GetOwner();
	const FVector Location = Owner->GetActorLocation();
	if (bHasLastLocation && DeltaTime > KINDA_SMALL_NUMBER)
	{
		const FVector Delta = Location - LastLocation;
		// Teleports (respawn, leaving a car) are not walking.
		const float Speed = Delta.Size2D() / DeltaTime;
		GroundSpeed = Speed < 3000.f ? FMath::FInterpTo(GroundSpeed, Speed, DeltaTime, 8.f) : 0.f;
	}
	LastLocation = Location;
	bHasLastLocation = true;

	GroundProbeTimer -= DeltaTime;
	if (GroundProbeTimer > 0.f && bHasGround)
	{
		return;
	}
	GroundProbeTimer = GroundProbeInterval;

	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		// The capsule bottom is the ground while walking.
		const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		if (Movement && Movement->IsMovingOnGround())
		{
			GroundZ = Location.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			bHasGround = true;
			return;
		}
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPFootGround), false, Owner);
	const FVector Start = Location + FVector(0.f, 0.f, 100.f);
	const FVector End = Location - FVector(0.f, 0.f, 300.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		GroundZ = Hit.ImpactPoint.Z;
		bHasGround = true;
	}
}

void UGTPFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Body.IsValid())
	{
		Body = FindBody();
		if (!Body.IsValid())
		{
			if (!bWarnedNoBones)
			{
				bWarnedNoBones = true;
				UE_LOG(LogGTPFootsteps, Warning, TEXT("%s: no skeletal mesh with bones %s/%s, no footsteps"),
					*GetNameSafe(GetOwner()), *FootBones[0].ToString(), *FootBones[1].ToString());
			}
			SetComponentTickInterval(1.f);
			return;
		}
	}
	if (!ShouldListen())
	{
		// Check again a few times per second; reset so the first frame back is not a step.
		SetComponentTickInterval(0.3f);
		bHasLastLocation = false;
		bHasGround = false;
		Feet[0].Reset();
		Feet[1].Reset();
		return;
	}
	SetComponentTickInterval(0.f);
	UpdateGround(DeltaTime);

	// Falls: a landing replaces the steps of that frame.
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const bool bFalling = Movement && Movement->IsFalling();
	if (bFalling)
	{
		FallSpeed = FMath::Max(FallSpeed, -Movement->Velocity.Z);
		bWasFalling = true;
		return;
	}
	if (bWasFalling)
	{
		bWasFalling = false;
		const float Speed = FallSpeed;
		FallSpeed = 0.f;
		Feet[0].Reset();
		Feet[1].Reset();
		const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
		if (Speed >= Settings->LandMinSpeed)
		{
			Step(GetOwner()->GetActorLocation(), true, FMath::GetMappedRangeValueClamped(FVector2f(Settings->LandMinSpeed, 1200.f), FVector2f(0.6f, 1.f), Speed));
		}
		return;
	}
	if (!bHasGround || GroundSpeed < 20.f)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Body.Get();
	for (int32 Foot = 0; Foot < 2; ++Foot)
	{
		const FVector FootLocation = Mesh->GetSocketLocation(FootBones[Foot]);
		const float Height = FootLocation.Z - GroundZ;
		if (DebugFeet() && Character && Character->IsPlayerControlled())
		{
			GTP_TRACE(LogGTPFootsteps, TEXT("[Feet] %s h=%.1f speed=%.0f t=%.3f"), Foot == 0 ? TEXT("L") : TEXT("R"), Height, GroundSpeed, GetWorld()->GetTimeSeconds());
		}
		if (Feet[Foot].Update(Height, DeltaTime, PlantSettings))
		{
			Step(FootLocation, false, GTPStepGain(GroundSpeed));
		}
	}
}

void UGTPFootstepComponent::Step(const FVector& FootLocation, bool bLanding, float Gain)
{
	UGTPAudioMix* Mix = UGTPAudioMix::Get(this);
	if (!Mix)
	{
		return;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPFootstep), false, GetOwner());
	Params.bReturnPhysicalMaterial = true;
	const FVector Start = FootLocation + FVector(0.f, 0.f, 40.f);
	const FVector End = FootLocation - FVector(0.f, 0.f, 80.f);
	FVector Where = FootLocation;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		LastSurface = Mix->ResolveSurface(Hit);
		Where = Hit.ImpactPoint;
	}
	USoundBase* Sound = Mix->PickFootstep(LastSurface, bLanding);
	if (!Sound)
	{
		return;
	}
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	const float Spread = Settings->FootstepPitchSpread;
	const float Pitch = 1.f + FMath::FRandRange(-Spread, Spread);
	const float Volume = Settings->FootstepVolume * Gain * FMath::FRandRange(0.9f, 1.f);
	Mix->PlayAt(this, Sound, EGTPAudioCategory::Footsteps, Where, Volume, Pitch);
	++StepCount;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (DebugFeet() && Character && Character->IsPlayerControlled())
	{
		GTP_TRACE(LogGTPFootsteps, TEXT("[Step] %s %s gain=%.2f"), bLanding ? TEXT("land") : TEXT("step"),
			*StaticEnum<EGTPSurface>()->GetNameStringByValue(static_cast<int64>(LastSurface)), Gain);
	}
}
