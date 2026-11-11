#include "GTPCombatMelee.h"

#include "GTPCombat.h"
#include "GTPCombatAudio.h"
#include "GTPHealth.h"
#include "GTPPedestrians.h"
#include "GTPThreat.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GTPLog.h"

UGTPCombatMeleeComponent::UGTPCombatMeleeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGTPCombatMeleeComponent::BeginPlay()
{
	Super::BeginPlay();
}

UGTPHealthComponent* UGTPCombatMeleeComponent::GetHealth() const
{
	return UGTPHealthComponent::Find(GetOwner(), true);
}

float UGTPCombatMeleeComponent::GetStamina() const
{
	const UGTPHealthComponent* Health = GetHealth();
	return Health ? Health->GetStamina() : 100.f;
}

void UGTPCombatMeleeComponent::Attack()
{
	const UGTPHealthComponent* Own = GetHealth();
	if (Own && Own->IsDead())
	{
		return;
	}
	if (!Fighter.IsBusy() && TryTakedown())
	{
		return;
	}
	float Spent = 0.f;
	const GTPMelee::EPhase Before = Fighter.GetPhase();
	Fighter.Input(GTPMelee::EInput::Attack, GetStamina(), Spent);
	if (Spent > 0.f)
	{
		GetHealth()->SpendStamina(Spent);
	}
	if (Before != GTPMelee::EPhase::Windup && Fighter.GetPhase() == GTPMelee::EPhase::Windup)
	{
		PlayClip(TEXT("AM_Punch_1"));
	}
	Wake();
}

void UGTPCombatMeleeComponent::SetGuard(bool bOn)
{
	float Spent = 0.f;
	Fighter.Input(bOn ? GTPMelee::EInput::GuardOn : GTPMelee::EInput::GuardOff, GetStamina(), Spent);
	if (bOn && IsGuarding())
	{
		PlayClip(TEXT("AM_Guard"));
	}
	Wake();
}

void UGTPCombatMeleeComponent::Dodge(FVector WorldDirection)
{
	float Spent = 0.f;
	Fighter.Input(GTPMelee::EInput::Dodge, GetStamina(), Spent);
	if (Fighter.GetPhase() != GTPMelee::EPhase::Dodge || Spent <= 0.f)
	{
		return;
	}
	UGTPHealthComponent* Health = GetHealth();
	Health->SpendStamina(Spent);
	// The health rules know nothing of the dodge window; the fighter does (Receive returns Dodged).
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	FVector Dir = WorldDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero() && Character)
	{
		Dir = -Character->GetActorForwardVector();
	}
	if (Character)
	{
		Character->LaunchCharacter(Dir * DodgeSpeedCmS, true, false);
		const FVector Local = Character->GetActorTransform().InverseTransformVectorNoScale(Dir);
		const TCHAR* Clip = FMath::Abs(Local.X) >= FMath::Abs(Local.Y) ? (Local.X >= 0.0 ? TEXT("AM_Dodge_F") : TEXT("AM_Dodge_B"))
			: (Local.Y >= 0.0 ? TEXT("AM_Dodge_R") : TEXT("AM_Dodge_L"));
		PlayClip(Clip);
		UGTPCombatAudio::PlayMelee(this, Character->GetActorLocation(), EGTPMeleeSound::Swish);
	}
	Wake();
}

GTPMelee::FReceiveResult UGTPCombatMeleeComponent::Receive(float Damage, bool bHeavy, const FVector& FromLocation)
{
	const AActor* Owner = GetOwner();
	const FVector ToAttacker = (FromLocation - Owner->GetActorLocation()).GetSafeNormal2D();
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(ToAttacker, Forward)), -1.f, 1.f)));
	const GTPMelee::FReceiveResult R = Fighter.Receive(Damage, bHeavy, Angle, GetStamina());
	if (R.StaminaCost > 0.f)
	{
		GetHealth()->SpendStamina(FMath::Min(R.StaminaCost, GetStamina()));
	}
	if (R.Outcome == GTPMelee::EReceive::Blocked || R.Outcome == GTPMelee::EReceive::GuardBroken)
	{
		PlayClip(TEXT("AM_Block"));
		UGTPCombatAudio::PlayMelee(this, Owner->GetActorLocation(), EGTPMeleeSound::Block);
	}
	SetComponentTickEnabled(true);
	return R;
}

AActor* UGTPCombatMeleeComponent::FindStrikeTarget(float RangeCm) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return nullptr;
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPMeleeSweep), false, Owner);
	World->OverlapMultiByChannel(Overlaps, Owner->GetActorLocation(), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(RangeCm), Params);
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	AActor* Best = nullptr;
	double BestDist = 0.0;
	for (const FOverlapResult& O : Overlaps)
	{
		AActor* Other = O.GetActor();
		if (!Other || Other == Owner || !(Cast<ACharacter>(Other) || Cast<AGTPPedestrian>(Other)))
		{
			continue;
		}
		const UGTPHealthComponent* Health = Other->FindComponentByClass<UGTPHealthComponent>();
		if (Health && Health->IsDead())
		{
			continue;
		}
		const FVector To = (Other->GetActorLocation() - Owner->GetActorLocation());
		const double Dist = To.Size2D();
		const double Cos = FVector::DotProduct(To.GetSafeNormal2D(), Forward);
		if (Cos < FMath::Cos(FMath::DegreesToRadians(StrikeHalfAngleDeg)))
		{
			continue;
		}
		if (!Best || Dist < BestDist)
		{
			Best = Other;
			BestDist = Dist;
		}
	}
	return Best;
}

bool UGTPCombatMeleeComponent::TryTakedown()
{
	const GTPMelee::FSettings& S = Fighter.GetSettings();
	AActor* Target = FindStrikeTarget(S.TakedownRangeCm);
	AActor* Owner = GetOwner();
	if (!Target || !Owner)
	{
		return false;
	}
	const FVector ToAttacker = (Owner->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	const FVector Back = -Target->GetActorForwardVector().GetSafeNormal2D();
	const float AngleFromBack = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(ToAttacker, Back)), -1.f, 1.f)));
	const UGTPCombatMeleeComponent* TargetMelee = Target->FindComponentByClass<UGTPCombatMeleeComponent>();
	const bool bStunned = TargetMelee && TargetMelee->IsStunned();
	const UGTPThreatResponseComponent* Threat = Target->FindComponentByClass<UGTPThreatResponseComponent>();
	const bool bAware = (Threat && Threat->GetState() != EGTPThreatState::Calm) || (TargetMelee && TargetMelee->IsBusy());
	const float Distance = static_cast<float>(FVector::Dist2D(Owner->GetActorLocation(), Target->GetActorLocation()));
	if (!GTPMelee::CanTakedown(S, Distance, AngleFromBack, bStunned, bAware, false))
	{
		return false;
	}
	UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this);
	const UGTPHealthComponent* TargetHealth = UGTPHealthComponent::Find(Target, true);
	if (!Combat || !TargetHealth)
	{
		return false;
	}
	PlayClip(TEXT("AM_Takedown"));
	FGTPCombatHit Hit;
	Hit.Instigator = Owner;
	Hit.Victim = Target;
	Hit.Location = Target->GetActorLocation();
	Hit.Direction = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
	Hit.bMelee = true;
	Hit.bHeavy = true;
	// Knocked out, not killed: the body falls and lies still with almost no health.
	Hit.FixedDamage = FMath::Max(0.f, TargetHealth->GetHealth() - TakedownHealthLeft);
	Combat->ApplyHit(Hit);
	GTP_TRACE(LogGTPCombat, TEXT("combat: takedown of %s"), *Target->GetName());
	return true;
}

void UGTPCombatMeleeComponent::LandBlow(const GTPMelee::FStep& Step)
{
	AActor* Owner = GetOwner();
	AActor* Target = FindStrikeTarget(Fighter.GetSettings().RangeCm);
	if (!Target)
	{
		UGTPCombatAudio::PlayMelee(this, Owner->GetActorLocation(), EGTPMeleeSound::Swish);
		return;
	}
	float Damage = Step.BlowDamage;
	bool bHeavy = Step.bHeavy;
	if (UGTPCombatMeleeComponent* Other = Target->FindComponentByClass<UGTPCombatMeleeComponent>())
	{
		const GTPMelee::FReceiveResult R = Other->Receive(Damage, bHeavy, Owner->GetActorLocation());
		if (R.Outcome == GTPMelee::EReceive::Dodged)
		{
			UGTPCombatAudio::PlayMelee(this, Owner->GetActorLocation(), EGTPMeleeSound::Swish);
			return;
		}
		Damage = R.Damage;
		bHeavy = R.bKnockdown;
	}
	if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
	{
		FGTPCombatHit Hit;
		Hit.Instigator = Owner;
		Hit.Victim = Target;
		Hit.Location = Target->GetActorLocation();
		Hit.Direction = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
		Hit.bMelee = true;
		Hit.bHeavy = bHeavy;
		Hit.FixedDamage = Damage;
		Combat->ApplyHit(Hit);
	}
}

void UGTPCombatMeleeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	float Spent = 0.f;
	const int32 ComboBefore = Fighter.GetComboIndex();
	const GTPMelee::EPhase Before = Fighter.GetPhase();
	const GTPMelee::FStep Step = Fighter.Tick(DeltaTime, GetStamina(), Spent);
	if (Spent > 0.f)
	{
		GetHealth()->SpendStamina(Spent);
	}
	if (Before == GTPMelee::EPhase::Recovery && Fighter.GetPhase() == GTPMelee::EPhase::Windup && Fighter.GetComboIndex() != ComboBefore)
	{
		PlayClip(*FString::Printf(TEXT("AM_Punch_%d"), Fighter.GetComboIndex() + 1));
	}
	if (Step.bBlowLands)
	{
		LandBlow(Step);
	}
	if (Fighter.GetPhase() == GTPMelee::EPhase::Idle)
	{
		SetComponentTickEnabled(false);
	}
}

void UGTPCombatMeleeComponent::PlayClip(FName Name)
{
	const TSoftObjectPtr<UAnimMontage>* Clip = Clips.Find(Name);
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Clip || Clip->IsNull() || !Character || !Character->GetMesh())
	{
		return;
	}
	if (UAnimMontage* Montage = Clip->LoadSynchronous())
	{
		if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(Montage);
		}
	}
}
