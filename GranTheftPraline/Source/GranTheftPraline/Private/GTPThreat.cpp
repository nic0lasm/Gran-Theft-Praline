#include "GTPThreat.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogGTPThreat);

// ---------------------------------------------------------------- subsystem

bool UGTPThreatSubsystem::IsEnabled()
{
	int32 Value = 0;
	FParse::Value(FCommandLine::Get(), TEXT("-ThreatResponse="), Value);
	return Value != 0;
}

bool UGTPThreatSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsEnabled() && Super::ShouldCreateSubsystem(Outer);
}

void UGTPThreatSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	GTP_TRACE(LogGTPThreat, TEXT("threat response: on"));
	if (FParse::Param(FCommandLine::Get(), TEXT("ThreatDemo")))
	{
		// Tests: the player is an armed person aiming where the camera looks.
		InWorld.GetTimerManager().SetTimer(DemoTimer, this, &UGTPThreatSubsystem::RefreshDemoThreat, 0.05f, true);
	}
}

void UGTPThreatSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DemoTimer);
	}
	Components.Reset();
	Super::Deinitialize();
}

void UGTPThreatSubsystem::RefreshDemoThreat()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return;
	}
	FVector Location;
	FRotator Rotation;
	PC->GetPlayerViewPoint(Location, Rotation);
	SetThreat(PC->GetPawn(), true, Location, Rotation.Vector());
}

void UGTPThreatSubsystem::SetThreat(AActor* InSource, bool bInArmed, const FVector& InOrigin, const FVector& AimDirection)
{
	Source = InSource;
	bArmed = bInArmed;
	Origin = InOrigin;
	Aim = AimDirection.GetSafeNormal();
	bActive = InSource != nullptr;
}

void UGTPThreatSubsystem::ClearThreat()
{
	bActive = false;
}

void UGTPThreatSubsystem::NotifyShot(const FVector& From, AActor* Target)
{
	++ShotSerial;
	LastShotFrom = From;
	LastShotTarget = Target;
}

void UGTPThreatSubsystem::Register(UGTPThreatResponseComponent* Component)
{
	Components.AddUnique(Component);
}

void UGTPThreatSubsystem::Unregister(UGTPThreatResponseComponent* Component)
{
	Components.Remove(Component);
}

float UGTPThreatSubsystem::NearbyFear(const UGTPThreatResponseComponent* Self, const FVector& Location, float RadiusCm) const
{
	float Highest = 0.f;
	for (const TWeakObjectPtr<UGTPThreatResponseComponent>& Weak : Components)
	{
		const UGTPThreatResponseComponent* Other = Weak.Get();
		if (!Other || Other == Self || !Other->GetOwner())
		{
			continue;
		}
		if (FVector::Dist(Other->GetOwner()->GetActorLocation(), Location) <= RadiusCm)
		{
			Highest = FMath::Max(Highest, Other->GetSpreadFear());
		}
	}
	return Highest;
}

// ---------------------------------------------------------------- component

UGTPThreatResponseComponent::UGTPThreatResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGTPThreatResponseComponent::BeginPlay()
{
	Super::BeginPlay();
	Brain.Temperament = static_cast<GTPThreat::ETemperament>(static_cast<int32>(InitialTemperament));
	if (!UGTPThreatSubsystem::IsEnabled() || !GetWorld())
	{
		return; // off: no tick, no cost
	}
	UGTPThreatSubsystem* Subsystem = GetWorld()->GetSubsystem<UGTPThreatSubsystem>();
	if (!Subsystem)
	{
		return;
	}
	Subsystem->Register(this);
	SeenShotSerial = Subsystem->GetShotSerial();
	if (UGTPHitReactionComponent* Hit = GetOwner()->FindComponentByClass<UGTPHitReactionComponent>())
	{
		Hit->OnReaction.AddUObject(this, &UGTPThreatResponseComponent::OnHitReaction);
	}
	SetComponentTickEnabled(true);
}

void UGTPThreatResponseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IdleTimer);
		if (UGTPThreatSubsystem* Subsystem = World->GetSubsystem<UGTPThreatSubsystem>())
		{
			Subsystem->Unregister(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UGTPThreatResponseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ThinkAccumulator += DeltaTime;
	if (ThinkAccumulator >= ThinkInterval)
	{
		Think(ThinkAccumulator);
		ThinkAccumulator = 0.f;
	}
	// Characters that back away or run need their input every frame.
	if (Brain.State == GTPThreat::EState::Backing || Brain.State == GTPThreat::EState::Fleeing)
	{
		MoveCharacter(DeltaTime);
	}
}

void UGTPThreatResponseComponent::Think(float Dt)
{
	UWorld* World = GetWorld();
	UGTPThreatSubsystem* Subsystem = World ? World->GetSubsystem<UGTPThreatSubsystem>() : nullptr;
	AActor* Owner = GetOwner();
	if (!Subsystem || !Owner)
	{
		return;
	}
	const FVector Me = Owner->GetActorLocation();
	GTPThreat::FPerception P;
	if (Subsystem->HasThreat() && Subsystem->GetSource() != Owner)
	{
		const FVector Head = Me + FVector(0.f, 0.f, 80.f);
		const FVector From = Subsystem->GetOrigin();
		const FVector Aim = Subsystem->GetAim();
		P.bThreatArmed = Subsystem->IsArmed();
		P.DistanceToThreatCm = static_cast<float>(FVector::Dist(From, Head));
		bool bAimed = GTPThreat::InAimCone(From.X, From.Y, From.Z, Aim.X, Aim.Y, Aim.Z, Head.X, Head.Y, Head.Z, AimRangeCm, AimHalfAngleDeg);
		if (bAimed)
		{
			// Something solid in between: he is not in the line of fire.
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPThreatSight), false, Subsystem->GetSource());
			if (World->LineTraceSingleByChannel(Hit, From, Head, ECC_Visibility, Params) && Hit.GetActor() != Owner)
			{
				bAimed = false;
			}
		}
		P.bAimedAt = bAimed;
	}
	if (Subsystem->GetShotSerial() != SeenShotSerial)
	{
		SeenShotSerial = Subsystem->GetShotSerial();
		P.bShotAtMe = Subsystem->GetLastShotTarget().Get() == Owner;
		P.bShotNearby = !P.bShotAtMe && FVector::Dist(Subsystem->GetLastShotFrom(), Me) <= Settings.ShotRadiusCm;
	}
	P.NearbyFear = Subsystem->NearbyFear(this, Me, Settings.ContagionRadiusCm);

	const GTPThreat::EState Old = Brain.State;
	GTPThreat::Step(Brain, P, Dt, Settings);
	if (Brain.State != Old)
	{
		OnStateChanged(Old, Brain.State);
	}
}

void UGTPThreatResponseComponent::OnHitReaction(EGTPHitReaction Reaction, const FGTPHitInfo& Hit, AActor* Body)
{
	const GTPThreat::EState Old = Brain.State;
	GTPThreat::OnHit(Brain, Reaction == EGTPHitReaction::Knockdown, Reaction == EGTPHitReaction::Death, Settings);
	if (Brain.State != Old)
	{
		OnStateChanged(Old, Brain.State);
	}
}

void UGTPThreatResponseComponent::OnStateChanged(GTPThreat::EState Old, GTPThreat::EState New)
{
	GTP_TRACE(LogGTPThreat, TEXT("%s: threat state %d -> %d (fear %.2f)"), *GetOwner()->GetName(), static_cast<int32>(Old), static_cast<int32>(New), Brain.Fear);
	if (New == GTPThreat::EState::HandsUp || New == GTPThreat::EState::Complying)
	{
		if (Old != GTPThreat::EState::HandsUp && Old != GTPThreat::EState::Complying)
		{
			PlayClip(TEXT("AM_HandsUp_Enter"));
			// The idle (3 s) replays while the hands stay up.
			GetWorld()->GetTimerManager().SetTimer(IdleTimer, FTimerDelegate::CreateUObject(this, &UGTPThreatResponseComponent::PlayIdle), 3.f, true, 0.6f);
		}
	}
	else if (UAnimInstance* Anim = GetOwner()->FindComponentByClass<USkeletalMeshComponent>() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>()->GetAnimInstance() : nullptr)
	{
		if (Old == GTPThreat::EState::HandsUp || Old == GTPThreat::EState::Complying)
		{
			GetWorld()->GetTimerManager().ClearTimer(IdleTimer);
			Anim->Montage_Stop(0.25f); // hands down
		}
	}
	if (New == GTPThreat::EState::Fleeing)
	{
		bArrivedReported = false;
	}
	SetComponentTickInterval((New == GTPThreat::EState::Backing || New == GTPThreat::EState::Fleeing) ? 0.f : ThinkInterval);
	OnThreatState.Broadcast(GetOwner(), static_cast<EGTPThreatState>(static_cast<int32>(Old)), static_cast<EGTPThreatState>(static_cast<int32>(New)));
}

void UGTPThreatResponseComponent::PlayIdle()
{
	PlayClip(TEXT("AM_HandsUp_Idle"));
}

void UGTPThreatResponseComponent::PlayClip(const TCHAR* Name)
{
	const TSoftObjectPtr<UAnimMontage>* Clip = Clips.Find(FName(Name));
	USkeletalMeshComponent* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	UAnimMontage* Montage = Clip ? Clip->LoadSynchronous() : nullptr;
	if (Montage && Mesh && Mesh->GetAnimInstance())
	{
		Mesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void UGTPThreatResponseComponent::MoveCharacter(float Dt)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UGTPThreatSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UGTPThreatSubsystem>() : nullptr;
	if (!Character || !Subsystem)
	{
		return; // pooled pedestrians are moved by the pedestrian system
	}
	const FVector Me = Character->GetActorLocation();
	FVector Direction = FVector::ZeroVector;
	float Scale = 1.f;
	if (Brain.State == GTPThreat::EState::Fleeing && bHasFleeTarget)
	{
		Direction = FleeTarget - Me;
		if (Direction.Size2D() <= FleeArrivalCm)
		{
			if (!bArrivedReported)
			{
				bArrivedReported = true;
				OnFledToTarget.Broadcast(Character);
			}
			return;
		}
	}
	else if (Subsystem->HasThreat())
	{
		Direction = Me - Subsystem->GetOrigin();
		if (Brain.State == GTPThreat::EState::Backing)
		{
			Scale = BackingSpeedScale;
		}
	}
	Direction.Z = 0.f;
	if (!Direction.IsNearlyZero())
	{
		Character->AddMovementInput(Direction.GetSafeNormal(), Scale);
	}
}
