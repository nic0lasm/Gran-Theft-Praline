#include "GTPCombatAudio.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPAudioWorld.h"
#include "Materials/MaterialInterface.h"
#include "Components/PrimitiveComponent.h"
#include "TimerManager.h"
#include "WheeledVehiclePawn.h"

namespace
{
	constexpr float SpeedOfSound = 34300.f; // cm/s

	bool ListenerOf(const UWorld* World, FVector& OutListener, const AActor*& OutPawn)
	{
		const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		if (!Player)
		{
			return false;
		}
		FVector Front, Right;
		Player->GetAudioListenerPosition(OutListener, Front, Right);
		OutPawn = Player->GetPawn();
		return true;
	}

	/** Plays now, or after Delay seconds (sound travel, echoes) through the world timer. */
	void PlayAtAfter(UWorld* World, UGTPAudioMix* Mix, USoundBase* Sound, EGTPAudioCategory Category, const FVector& Location, float Volume, float Pitch, float Delay)
	{
		if (!Sound)
		{
			return;
		}
		if (Delay < 0.02f)
		{
			Mix->PlayAt(World, Sound, Category, Location, Volume, Pitch);
			return;
		}
		TWeakObjectPtr<UWorld> WeakWorld(World);
		TWeakObjectPtr<UGTPAudioMix> WeakMix(Mix);
		TWeakObjectPtr<USoundBase> WeakSound(Sound);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, WeakMix, WeakSound, Category, Location, Volume, Pitch]()
		{
			if (WeakWorld.IsValid() && WeakMix.IsValid() && WeakSound.IsValid())
			{
				WeakMix->PlayAt(WeakWorld.Get(), WeakSound.Get(), Category, Location, Volume, Pitch);
			}
		}), Delay, false);
	}

	/** Delay and level of the street reflections by ambience zone (narrow street: early and strong). */
	void EchoFor(EGTPAmbienceZone Zone, float& OutDelay, float& OutLevel)
	{
		switch (Zone)
		{
		case EGTPAmbienceZone::Pedestrian: OutDelay = 0.06f; OutLevel = 0.6f; break; // ~20 m between facades
		case EGTPAmbienceZone::Quay: OutDelay = 0.25f; OutLevel = 0.3f; break;       // facades on one side only
		case EGTPAmbienceZone::Bridge: OutDelay = 0.4f; OutLevel = 0.15f; break;     // open water, far banks
		case EGTPAmbienceZone::Square: OutDelay = 0.35f; OutLevel = 0.25f; break;    // facades 60 to 150 m away
		default: OutDelay = 0.09f; OutLevel = 0.5f; break;                            // street with traffic, ~30 m
		}
	}
}

void UGTPCombatAudio::PlayShot(const UObject* WorldContext, AActor* Shooter, FVector Origin, FVector Direction, float Range)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGTPAudioMix* Mix = UGTPAudioMix::Get(WorldContext);
	if (!World || !Mix || !UGTPAudioMix::IsGameAudioEnabled())
	{
		return;
	}
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	FVector Listener = Origin;
	const AActor* ListenerPawn = nullptr;
	ListenerOf(World, Listener, ListenerPawn);
	const float Distance = FVector::Dist(Listener, Origin);
	const float Travel = Distance / SpeedOfSound;
	const float Pitch = FMath::FRandRange(0.96f, 1.04f);

	const bool bClose = Distance <= Settings->CloseShotDistance;
	// Sound travels: a shot 170 m away is heard half a second after its flash.
	PlayAtAfter(World, Mix, Mix->PickWeapon(bClose ? EGTPWeaponSound::ShotClose : EGTPWeaponSound::ShotFar), EGTPAudioCategory::Weapons,
		Origin, Settings->ShotVolume, Pitch, bClose ? 0.f : Travel);

	float EchoDelay = 0.09f;
	float EchoLevel = 0.5f;
	EchoFor(Mix->SampleZone(Origin), EchoDelay, EchoLevel);
	PlayAtAfter(World, Mix, Mix->PickWeapon(EGTPWeaponSound::Tail), EGTPAudioCategory::Weapons, Origin, Settings->ShotVolume * EchoLevel, Pitch,
		Travel + EchoDelay);

	// Whizz: the bullet line passes near the listener (not the shooter's own ears).
	const FVector Dir = Direction.GetSafeNormal();
	if (!Dir.IsNearlyZero() && ListenerPawn != Shooter)
	{
		const float Along = FVector::DotProduct(Listener - Origin, Dir);
		if (Along > 300.f && Along < Range)
		{
			const FVector Closest = Origin + Dir * Along;
			if (FVector::Dist(Closest, Listener) < Settings->WhizzRadius)
			{
				Mix->PlayAt(World, Mix->PickWeapon(EGTPWeaponSound::Whizz), EGTPAudioCategory::Weapons, Closest, 1.f, FMath::FRandRange(0.9f, 1.1f));
			}
		}
	}

	if (UGTPAudioWorld* AudioWorld = World->GetSubsystem<UGTPAudioWorld>())
	{
		AudioWorld->DuckForGunshot();
		AudioWorld->OnLoudSound.Broadcast(Origin, Settings->ShotScareRadius, Shooter);
	}
}

void UGTPCombatAudio::PlayBulletImpact(const UObject* WorldContext, FVector Location, EGTPBulletImpact Kind)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGTPAudioMix* Mix = UGTPAudioMix::Get(WorldContext);
	if (!World || !Mix || !UGTPAudioMix::IsGameAudioEnabled())
	{
		return;
	}
	Mix->PlayAt(World, Mix->PickBulletImpact(Kind), EGTPAudioCategory::Impacts, Location, FMath::FRandRange(0.8f, 1.f), FMath::FRandRange(0.92f, 1.08f));
}

EGTPBulletImpact UGTPCombatAudio::ImpactKindOf(const UObject* WorldContext, const FHitResult& Hit)
{
	const AActor* Actor = Hit.GetActor();
	if (Cast<APawn>(Actor) && !Cast<AWheeledVehiclePawn>(Actor))
	{
		return EGTPBulletImpact::Flesh;
	}
	if (UGTPAudioMix::IsPedestrian(Actor))
	{
		return EGTPBulletImpact::Flesh;
	}
	if (Cast<AWheeledVehiclePawn>(Actor))
	{
		return EGTPBulletImpact::Car;
	}
	if (const UPrimitiveComponent* Component = Hit.GetComponent())
	{
		const UMaterialInterface* Material = Component->GetMaterial(0);
		if (Material && Material->GetName().Contains(TEXT("Glass")))
		{
			return EGTPBulletImpact::Glass;
		}
	}
	const UGTPAudioMix* Mix = UGTPAudioMix::Get(WorldContext);
	switch (Mix ? Mix->ResolveSurface(Hit) : EGTPSurface::Stone)
	{
	case EGTPSurface::Metal: return EGTPBulletImpact::Sparks;
	case EGTPSurface::Wood: return EGTPBulletImpact::Wood;
	case EGTPSurface::Water: return EGTPBulletImpact::Water;
	case EGTPSurface::Grass:
	case EGTPSurface::Gravel: return EGTPBulletImpact::Dirt;
	default: return EGTPBulletImpact::Dust;
	}
}

void UGTPCombatAudio::PlayHit(const UObject* WorldContext, AActor* Victim, float Damage, bool bMelee)
{
	if (!Victim)
	{
		return;
	}
	const FVector Location = Victim->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	if (bMelee)
	{
		PlayMelee(WorldContext, Location, Damage >= HeavyDamage ? EGTPMeleeSound::Heavy : EGTPMeleeSound::Light);
	}
	else
	{
		PlayBulletImpact(WorldContext, Location, EGTPBulletImpact::Flesh);
	}
}

void UGTPCombatAudio::PlayMelee(const UObject* WorldContext, FVector Location, EGTPMeleeSound Sound)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGTPAudioMix* Mix = UGTPAudioMix::Get(WorldContext);
	if (!World || !Mix || !UGTPAudioMix::IsGameAudioEnabled())
	{
		return;
	}
	Mix->PlayAt(World, Mix->PickMelee(Sound), EGTPAudioCategory::Impacts, Location, FMath::FRandRange(0.85f, 1.f), FMath::FRandRange(0.92f, 1.08f));
}

void UGTPCombatAudio::PlayWeaponHandling(const UObject* WorldContext, AActor* Holder, EGTPWeaponSound Sound)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGTPAudioMix* Mix = UGTPAudioMix::Get(WorldContext);
	if (!World || !Mix || !Holder || !UGTPAudioMix::IsGameAudioEnabled())
	{
		return;
	}
	Mix->PlayAt(World, Mix->PickWeapon(Sound), EGTPAudioCategory::Weapons, Holder->GetActorLocation() + FVector(0.f, 0.f, 100.f), 0.8f, FMath::FRandRange(0.97f, 1.03f));
}
