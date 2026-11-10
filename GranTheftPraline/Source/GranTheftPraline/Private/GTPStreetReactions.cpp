#include "GTPStreetReactions.h"

#include "Engine/World.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPPedestrians.h"
#include "GTPWanted.h"

void UGTPStreetReactions::Start(UWorld* World)
{
	WorldPtr = World;
	if (UGTPPedestrianSubsystem* People = World ? World->GetSubsystem<UGTPPedestrianSubsystem>() : nullptr)
	{
		HitHandle = People->OnPedestrianHit.AddUObject(this, &UGTPStreetReactions::OnPedestrianHit);
		ScaredHandle = People->OnPedestrianScared.AddUObject(this, &UGTPStreetReactions::OnPedestrianScared);
	}
	const FVector2D Gap = GetDefault<UGTPAudioSettings>()->RadioChatterGap;
	ChatterTimer = FMath::FRandRange(Gap.X, Gap.Y);
}

void UGTPStreetReactions::Stop()
{
	if (UWorld* World = WorldPtr.Get())
	{
		if (UGTPPedestrianSubsystem* People = World->GetSubsystem<UGTPPedestrianSubsystem>())
		{
			People->OnPedestrianHit.Remove(HitHandle);
			People->OnPedestrianScared.Remove(ScaredHandle);
		}
	}
	WorldPtr.Reset();
}

void UGTPStreetReactions::OnPedestrianHit(const FVector& Location, AActor* /*Cause*/)
{
	UWorld* World = WorldPtr.Get();
	UGTPAudioMix* Mix = UGTPAudioMix::Get(World);
	if (!Mix)
	{
		return;
	}
	++HitCount;
	// Every person has a voice of their own: pitch spread covers low and high voices.
	Mix->PlayAt(World, Mix->PickVoice(EGTPVoiceKind::Hurt), EGTPAudioCategory::Voice, Location + FVector(0.f, 0.f, 150.f),
		GetDefault<UGTPAudioSettings>()->VoiceVolume, FMath::FRandRange(0.82f, 1.28f));
	LastScreamTime = World->GetTimeSeconds();
}

void UGTPStreetReactions::OnPedestrianScared(const FVector& Location, AActor* /*Cause*/)
{
	UWorld* World = WorldPtr.Get();
	UGTPAudioMix* Mix = UGTPAudioMix::Get(World);
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	if (!Mix)
	{
		return;
	}
	++ScaredCount;
	const double Now = World->GetTimeSeconds();
	if (Now - LastScreamTime < Settings->ScreamCooldown || FMath::FRand() > Settings->ScreamChance)
	{
		return;
	}
	LastScreamTime = Now;
	const EGTPVoiceKind Kind = FMath::FRand() < 0.65f ? EGTPVoiceKind::Scream : EGTPVoiceKind::Gasp;
	Mix->PlayAt(World, Mix->PickVoice(Kind), EGTPAudioCategory::Voice, Location + FVector(0.f, 0.f, 150.f),
		Settings->VoiceVolume * FMath::FRandRange(0.7f, 1.f), FMath::FRandRange(0.82f, 1.28f));
}

void UGTPStreetReactions::Tick(float DeltaTime)
{
	UpdatePoliceRadio(DeltaTime);
}

void UGTPStreetReactions::UpdatePoliceRadio(float DeltaTime)
{
	UWorld* World = WorldPtr.Get();
	UGTPAudioMix* Mix = UGTPAudioMix::Get(World);
	const UGTPWantedComponent* Wanted = World ? UGTPWantedComponent::Find(World) : nullptr;
	if (!Mix || !Wanted)
	{
		return;
	}
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	const int32 Stars = Wanted->GetStars();
	if (Stars > LastStars)
	{
		UGTPAudioMix::PlayUI(World, EGTPUISound::StarUp);
		Mix->Play2D(World, Mix->PickRadio(EGTPRadioCall::Alert), EGTPAudioCategory::Radio, Settings->RadioVolume, 1.f);
		PendingAlert = 1.4f;
		ChatterTimer = FMath::FRandRange(Settings->RadioChatterGap.X, Settings->RadioChatterGap.Y);
	}
	else if (Stars == 0 && LastStars > 0)
	{
		UGTPAudioMix::PlayUI(World, EGTPUISound::StarDown);
		Mix->Play2D(World, Mix->PickRadio(EGTPRadioCall::Clear), EGTPAudioCategory::Radio, Settings->RadioVolume, 1.f);
		PendingAlert = -1.f;
	}
	LastStars = Stars;
	if (Stars == 0)
	{
		return;
	}
	if (PendingAlert >= 0.f)
	{
		PendingAlert -= DeltaTime;
		if (PendingAlert < 0.f)
		{
			Mix->Play2D(World, Mix->PickRadio(EGTPRadioCall::Chatter), EGTPAudioCategory::Radio, Settings->RadioVolume, 1.f);
		}
		return;
	}
	ChatterTimer -= DeltaTime;
	if (ChatterTimer <= 0.f)
	{
		// More stars, busier channel.
		const float Busy = 1.f / FMath::Max(1.f, 0.6f + 0.4f * Stars);
		ChatterTimer = FMath::FRandRange(Settings->RadioChatterGap.X, Settings->RadioChatterGap.Y) * Busy;
		Mix->Play2D(World, Mix->PickRadio(EGTPRadioCall::Chatter), EGTPAudioCategory::Radio, Settings->RadioVolume, FMath::FRandRange(0.97f, 1.03f));
	}
}

FString UGTPStreetReactions::Describe() const
{
	return FString::Printf(TEXT("people hit %d, scared %d; stars %d"), HitCount, ScaredCount, LastStars);
}
