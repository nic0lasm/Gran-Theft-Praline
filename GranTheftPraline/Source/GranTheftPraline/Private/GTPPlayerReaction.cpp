#include "GTPPlayerReaction.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GTPHealth.h"
#include "GTPRagdoll.h"
#include "GTPVehicle.h"
#include "TimerManager.h"

namespace
{
EGTPImpactSeverity ToBlueprint(GTPImpact::ESeverity S)
{
	return static_cast<EGTPImpactSeverity>(static_cast<int32>(S));
}

GTPImpact::FVec ToLogic(const FVector& V)
{
	return {static_cast<float>(V.X), static_cast<float>(V.Y), static_cast<float>(V.Z)};
}
}

UGTPPlayerReactionComponent::UGTPPlayerReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UGTPRagdollComponent* UGTPPlayerReactionComponent::GetRagdoll()
{
	if (!Ragdoll)
	{
		Ragdoll = GetOwner()->FindComponentByClass<UGTPRagdollComponent>();
		if (!Ragdoll)
		{
			Ragdoll = NewObject<UGTPRagdollComponent>(GetOwner(), TEXT("GTPRagdoll"));
			Ragdoll->RegisterComponent();
		}
	}
	return Ragdoll;
}

void UGTPPlayerReactionComponent::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	DefaultWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
	Character->LandedDelegate.AddDynamic(this, &UGTPPlayerReactionComponent::OnLandedDynamic);
	GetWorld()->GetTimerManager().SetTimer(CarTimer, this, &UGTPPlayerReactionComponent::CheckCars, CarCheckInterval, true);
}

void UGTPPlayerReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CarTimer);
		World->GetTimerManager().ClearTimer(StumbleTimer);
	}
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LandedDelegate.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void UGTPPlayerReactionComponent::OnLandedDynamic(const FHitResult& Hit)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || Character->GetCharacterMovement()->MovementMode == MOVE_None)
	{
		return;
	}
	// Landed fires after the velocity is zeroed; the speed was remembered by the last car check tick.
	const float Down = -LastFallSpeed;
	const GTPImpact::FLandingResult R = GTPImpact::ClassifyLanding(Down, Character->bIsCrouched, Settings);
	if (R.Kind == GTPImpact::ELanding::None)
	{
		return;
	}
	if (R.Kind == GTPImpact::ELanding::Ragdoll)
	{
		ApplyHit(GTPImpact::ESeverity::Knock, FVector::ZeroVector, R.Damage, 0.f, true);
	}
	else
	{
		TakeDamage(R.Damage, EGTPDamageCause::Fall);
		Stumble();
	}
}

void UGTPPlayerReactionComponent::ReactToBailOut(FVector VehicleVelocity)
{
	const GTPImpact::ESeverity S = GTPImpact::ClassifyBailOut(GTPImpact::KmhFromCmS(static_cast<float>(VehicleVelocity.Size2D())));
	if (S == GTPImpact::ESeverity::None)
	{
		return;
	}
	if (S == GTPImpact::ESeverity::Bump)
	{
		Stumble();
		return;
	}
	const float Damage = S == GTPImpact::ESeverity::Launch ? 25.f : 8.f;
	ApplyHit(S, VehicleVelocity * 0.35f + FVector(0.f, 0.f, 150.f), Damage, S == GTPImpact::ESeverity::Launch ? 4.f : 0.f);
}

void UGTPPlayerReactionComponent::CheckCars()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || GetRagdoll()->IsRagdoll())
	{
		return;
	}
	LastFallSpeed = static_cast<float>(Character->GetVelocity().Z);

	const FVector Center = Character->GetActorLocation();
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_Vehicle);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GTPPlayerCars), false, Character);
	if (!GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(CarCheckRadius), Query))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AGTPVehicle* Car = Cast<AGTPVehicle>(Overlap.GetActor());
		if (!Car)
		{
			continue;
		}
		const double* Last = LastHit.Find(Car);
		if (Last && Now - *Last < CarHitCooldown)
		{
			continue;
		}
		// Contact: the car's bounds reach the capsule.
		const FBox Bounds = Car->GetComponentsBoundingBox();
		const FVector Closest = Bounds.GetClosestPointTo(Center);
		if (FVector::Dist2D(Closest, Center) > Radius + 15.f)
		{
			continue;
		}
		GTPImpact::FCarHitInput In;
		In.CarVelocity = ToLogic(Car->GetVelocity());
		In.PersonVelocity = ToLogic(Character->GetVelocity());
		In.ToCar = ToLogic(Closest - Center);
		const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Car->GetRootComponent());
		In.CarMassKg = Body ? FMath::Max(300.f, Body->GetMass()) : 1200.f;
		const GTPImpact::FCarHitResult R = GTPImpact::ClassifyCarHit(In, Settings);
		if (R.Severity == GTPImpact::ESeverity::None)
		{
			continue;
		}
		LastHit.Add(Car, Now);
		ApplyHit(R.Severity, FVector(R.LaunchVelocity.X, R.LaunchVelocity.Y, R.LaunchVelocity.Z), R.Damage, R.SpinRadS);
		return;
	}
}

float UGTPPlayerReactionComponent::GetHealth() const
{
	const UGTPHealthComponent* Shared = UGTPHealthComponent::Find(GetOwner());
	return Shared ? Shared->GetHealth() : Health;
}

void UGTPPlayerReactionComponent::TakeDamage(float Damage, EGTPDamageCause Cause)
{
	// One health per body: when the combat health component exists it is the truth
	// (cars and falls ignore armour); otherwise the local 0..100 value stands in.
	if (UGTPHealthComponent* Shared = UGTPHealthComponent::Find(GetOwner()))
	{
		const FGTPDamageOutcome Outcome = Shared->ApplyDamage(Damage, Cause, nullptr, true);
		Health = Outcome.HealthAfter;
		if (!Outcome.bIgnored)
		{
			OnHurt.Broadcast(Outcome.HealthLost, Health);
		}
		return;
	}
	Health = FMath::Max(0.f, Health - Damage);
	OnHurt.Broadcast(Damage, Health);
}

void UGTPPlayerReactionComponent::ApplyHit(GTPImpact::ESeverity Severity, const FVector& Launch, float Damage, float Spin, bool bFromFall)
{
	TakeDamage(Damage, bFromFall ? EGTPDamageCause::Fall : EGTPDamageCause::Vehicle);
	if (Severity == GTPImpact::ESeverity::Bump)
	{
		Stumble();
		return;
	}
	GetRagdoll()->BeginRagdoll(Launch, ToBlueprint(Severity), Health, Spin);
}

void UGTPPlayerReactionComponent::Stumble()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || bStumbling)
	{
		return;
	}
	bStumbling = true;
	Character->GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed * StumbleSpeedScale;
	if (UAnimMontage* Montage = StumbleMontage.LoadSynchronous())
	{
		if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(Montage);
		}
	}
	GetWorld()->GetTimerManager().SetTimer(StumbleTimer, this, &UGTPPlayerReactionComponent::EndStumble, StumbleSeconds, false);
}

void UGTPPlayerReactionComponent::EndStumble()
{
	bStumbling = false;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
	}
}
