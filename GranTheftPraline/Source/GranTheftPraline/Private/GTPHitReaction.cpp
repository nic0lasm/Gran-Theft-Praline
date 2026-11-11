#include "GTPHitReaction.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GTPLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPHit, Log, All);

bool UGTPHitReactionComponent::IsEnabled()
{
	int32 Value = 1;
	FParse::Value(FCommandLine::Get(), TEXT("-HitReactions="), Value);
	return Value != 0;
}

UGTPRagdollComponent* UGTPHitReactionComponent::GetRagdoll()
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

EGTPHitReaction UGTPHitReactionComponent::ReactToHit(const FGTPHitInfo& Hit)
{
	if (!IsEnabled() || !GetOwner() || !GetWorld())
	{
		return EGTPHitReaction::None;
	}
	GTPHit::FHitInfo In;
	In.Damage = Hit.Damage;
	In.HealthAfter = Hit.HealthAfter;
	In.DirX = static_cast<float>(Hit.Direction.X);
	In.DirY = static_cast<float>(Hit.Direction.Y);
	In.FacingYawDeg = static_cast<float>(GetOwner()->GetActorRotation().Yaw);
	In.bMelee = Hit.bMelee;
	In.Zone = Hit.Bone.IsNone() ? GTPHit::EZone::Torso : GTPHit::ZoneFromBone(std::string(TCHAR_TO_UTF8(*Hit.Bone.ToString().ToLower())));

	const GTPHit::FReaction R = GTPHit::Evaluate(In, State, GetWorld()->GetTimeSeconds(), Settings);
	const EGTPHitReaction Result = static_cast<EGTPHitReaction>(static_cast<int32>(R.Kind));
	if (R.Kind == GTPHit::EReaction::None)
	{
		return Result;
	}
	GTP_TRACE(LogGTPHit, TEXT("hit reaction on %s: %d (zone %d, direction %d)"), *GetOwner()->GetName(), static_cast<int32>(R.Kind),
		static_cast<int32>(R.Zone), static_cast<int32>(R.Direction));

	switch (R.Kind)
	{
	case GTPHit::EReaction::Flinch:
		PlayClip(GTPHit::ClipName(R.Kind, R.Direction));
		break;
	case GTPHit::EReaction::Stagger:
	{
		PlayClip(GTPHit::ClipName(R.Kind, R.Direction));
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		if (Character && !bStaggering)
		{
			bStaggering = true;
			SavedWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
			Character->GetCharacterMovement()->MaxWalkSpeed = SavedWalkSpeed * StaggerSpeedScale;
			GetWorld()->GetTimerManager().SetTimer(StaggerTimer, this, &UGTPHitReactionComponent::EndStagger, StaggerSeconds, false);
		}
		break;
	}
	case GTPHit::EReaction::Knockdown:
	case GTPHit::EReaction::Death:
	{
		const bool bDeath = R.Kind == GTPHit::EReaction::Death;
		// A death keeps the ragdoll down for good (health 0); a knockdown gets up.
		GetRagdoll()->BeginRagdoll(FVector(R.LaunchX, R.LaunchY, R.LaunchZ), bDeath ? EGTPImpactSeverity::Launch : EGTPImpactSeverity::Knock,
			bDeath ? 0.f : FMath::Max(1.f, Hit.HealthAfter), R.SpinRadS);
		break;
	}
	default:
		break;
	}
	OnReaction.Broadcast(Result, Hit, GetOwner());
	return Result;
}

void UGTPHitReactionComponent::PlayClip(const char* Name)
{
	if (!Name || !*Name)
	{
		return;
	}
	const TSoftObjectPtr<UAnimMontage>* Clip = Clips.Find(FName(UTF8_TO_TCHAR(Name)));
	USkeletalMeshComponent* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	UAnimMontage* Montage = Clip ? Clip->LoadSynchronous() : nullptr;
	if (Montage && Mesh && Mesh->GetAnimInstance())
	{
		Mesh->GetAnimInstance()->Montage_Play(Montage);
	}
}

void UGTPHitReactionComponent::EndStagger()
{
	bStaggering = false;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = SavedWalkSpeed;
	}
}
