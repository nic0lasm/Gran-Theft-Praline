#include "GTPCombatSounds.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GTPCombatAudio.h"

namespace
{
	/** Share of the reload at which the new magazine clicks in (the old one drops at the start). */
	constexpr float MagInShare = 0.55f;
	/** Bullets that miss: the whizz is tested along this length (cm). */
	constexpr float MissRange = 10000.f;
	constexpr float LookupInterval = 0.5f;
}

void UGTPCombatSounds::Start(UWorld* World)
{
	WorldPtr = World;
	UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(World);
	if (!Combat)
	{
		return; // -Combat=0
	}
	CombatPtr = Combat;
	ShotHandle = Combat->OnShotFired.AddUObject(this, &UGTPCombatSounds::OnShotFired);
	DamageHandle = Combat->OnDamageDealt.AddUObject(this, &UGTPCombatSounds::OnDamageDealt);
	BindPlayerWeapon();
}

void UGTPCombatSounds::Stop()
{
	if (UGTPCombatSubsystem* Combat = CombatPtr.Get())
	{
		Combat->OnShotFired.Remove(ShotHandle);
		Combat->OnDamageDealt.Remove(DamageHandle);
	}
	if (UGTPWeaponComponent* Weapon = WeaponPtr.Get())
	{
		Weapon->OnWeaponEvent.Remove(WeaponHandle);
	}
	CombatPtr.Reset();
	WeaponPtr.Reset();
	WorldPtr.Reset();
	MagInLeft = -1.f;
}

void UGTPCombatSounds::Tick(float DeltaTime)
{
	if (!CombatPtr.IsValid())
	{
		return;
	}
	LookupTimer -= DeltaTime;
	if (LookupTimer <= 0.f)
	{
		LookupTimer = LookupInterval;
		BindPlayerWeapon();
	}
	if (MagInLeft >= 0.f)
	{
		MagInLeft -= DeltaTime;
		if (MagInLeft < 0.f)
		{
			if (UGTPWeaponComponent* Weapon = WeaponPtr.Get())
			{
				++Handling;
				UGTPCombatAudio::PlayWeaponHandling(Weapon, Weapon->GetOwner(), EGTPWeaponSound::MagIn);
			}
		}
	}
}

void UGTPCombatSounds::BindPlayerWeapon()
{
	UGTPCombatSubsystem* Combat = CombatPtr.Get();
	const APawn* Pawn = Combat ? Combat->GetPlayerPawn() : nullptr;
	UGTPWeaponComponent* Weapon = Pawn ? Pawn->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
	if (Weapon == WeaponPtr.Get())
	{
		return;
	}
	if (UGTPWeaponComponent* Old = WeaponPtr.Get())
	{
		Old->OnWeaponEvent.Remove(WeaponHandle);
	}
	WeaponHandle.Reset();
	MagInLeft = -1.f;
	WeaponPtr = Weapon;
	if (Weapon)
	{
		WeaponHandle = Weapon->OnWeaponEvent.AddUObject(this, &UGTPCombatSounds::OnWeaponEvent);
	}
}

void UGTPCombatSounds::OnShotFired(AActor* Shooter, const FVector& Origin, const FVector& Direction, FName /*Weapon*/, const FHitResult& Hit, bool bHit)
{
	UWorld* World = WorldPtr.Get();
	if (!World)
	{
		return;
	}
	++Shots;
	// The whizz only counts the part of the line the bullet really flew.
	const FVector Impact = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : FVector::ZeroVector;
	const float Range = bHit && Hit.bBlockingHit ? FVector::Dist(Origin, Impact) : MissRange;
	UGTPCombatAudio::PlayShot(World, Shooter, Origin, Direction, Range);
	if (Hit.bBlockingHit)
	{
		++Impacts;
		UGTPCombatAudio::PlayBulletImpact(World, Impact, UGTPCombatAudio::ImpactKindOf(World, Hit));
	}
}

void UGTPCombatSounds::OnDamageDealt(AActor* /*Instigator*/, AActor* Victim, float Damage, GTPDamage::EZone /*Zone*/, bool bMelee, float /*HealthAfter*/)
{
	// A bullet in a body already sounded as a Flesh impact in OnShotFired.
	if (!bMelee || !Victim || !WorldPtr.IsValid())
	{
		return;
	}
	++Blows;
	UGTPCombatAudio::PlayHit(WorldPtr.Get(), Victim, Damage, true);
}

void UGTPCombatSounds::OnWeaponEvent(UGTPWeaponComponent* Weapon, EGTPWeaponEvent Event)
{
	if (!Weapon)
	{
		return;
	}
	AActor* Holder = Weapon->GetOwner();
	switch (Event)
	{
	case EGTPWeaponEvent::DryFire:
		++Handling;
		UGTPCombatAudio::PlayWeaponHandling(Weapon, Holder, EGTPWeaponSound::DryFire);
		break;
	case EGTPWeaponEvent::ReloadStarted:
		++Handling;
		UGTPCombatAudio::PlayWeaponHandling(Weapon, Holder, EGTPWeaponSound::MagOut);
		MagInLeft = Weapon->GetSpec().ReloadSeconds * MagInShare;
		break;
	case EGTPWeaponEvent::ReloadFinished:
		if (MagInLeft >= 0.f)
		{
			// A reload shorter than expected: the magazine still goes in before the slide.
			++Handling;
			UGTPCombatAudio::PlayWeaponHandling(Weapon, Holder, EGTPWeaponSound::MagIn);
		}
		MagInLeft = -1.f;
		++Handling;
		UGTPCombatAudio::PlayWeaponHandling(Weapon, Holder, EGTPWeaponSound::Slide);
		break;
	case EGTPWeaponEvent::ReloadCancelled:
		MagInLeft = -1.f;
		break;
	default:
		break;
	}
}

FString UGTPCombatSounds::Describe() const
{
	if (!CombatPtr.IsValid())
	{
		return TEXT("combat=off");
	}
	return FString::Printf(TEXT("combat shots %d impacts %d blows %d handling %d weapon %s"),
		Shots, Impacts, Blows, Handling, WeaponPtr.IsValid() ? TEXT("bound") : TEXT("none"));
}
