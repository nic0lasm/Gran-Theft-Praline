#include "GTPHornComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GTPAudioMix.h"
#include "GTPSynthSoundWave.h"
#include "Misc/Crc.h"

UGTPHornComponent::UGTPHornComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bAutoActivate = false;
	bAllowSpatialization = true;
}

UGTPHornComponent* UGTPHornComponent::SetHorn(AActor* Vehicle, bool bOn)
{
	if (!Vehicle || Vehicle->IsActorBeingDestroyed())
	{
		return nullptr;
	}
	UGTPHornComponent* Horn = Vehicle->FindComponentByClass<UGTPHornComponent>();
	if (!Horn)
	{
		if (!bOn || !UGTPAudioMix::IsGameAudioEnabled() || !UGTPAudioMix::IsSynthAudioEnabled())
		{
			return nullptr;
		}
		Horn = NewObject<UGTPHornComponent>(Vehicle, MakeUniqueObjectName(Vehicle, UGTPHornComponent::StaticClass(), TEXT("GTPHorn")), RF_Transient);
		if (USceneComponent* Root = Vehicle->GetRootComponent())
		{
			Horn->SetupAttachment(Root);
		}
		// Behind the front bumper.
		Horn->SetRelativeLocation(FVector(180.f, 0.f, 50.f));
		// Its own pitch, from the car's name so a replay sounds the same.
		Horn->Pitch = 0.92f + 0.16f * static_cast<float>(FCrc::StrCrc32(*Vehicle->GetName()) % 1000u) / 1000.f;
		Vehicle->AddInstanceComponent(Horn);
		Horn->RegisterComponent();
	}
	Horn->SetOn(bOn);
	return Horn;
}

void UGTPHornComponent::EnsureWave()
{
	if (HornWave)
	{
		return;
	}
	if (UGTPAudioMix* Mix = UGTPAudioMix::Get(this))
	{
		SoundClassOverride = Mix->GetClass(EGTPAudioCategory::Vehicles);
		AttenuationSettings = Mix->GetAttenuation(EGTPAudioCategory::Sirens);
		ConcurrencySet.Add(Mix->GetConcurrency(EGTPAudioCategory::Sirens));
	}
	Synth = MakeShared<FGTPHornSynth, ESPMode::ThreadSafe>();
	Synth->Init(UGTPSynthSoundWave::SynthSampleRate, FGTPHornSettings());
	Synth->SetPitchScale(Pitch);
	TSharedPtr<FGTPHornSynth, ESPMode::ThreadSafe> Shared = Synth;
	HornWave = NewObject<UGTPSynthSoundWave>(this);
	HornWave->Setup([Shared](float* Out, int32 Count) { Shared->Render(Out, Count); });
	SetSound(HornWave);
}

void UGTPHornComponent::SetOn(bool bOn)
{
	if (bOn == bHornOn)
	{
		return;
	}
	EnsureWave();
	bHornOn = bOn;
	Synth->SetOn(bOn);
	if (bOn)
	{
		StopCountdown = 0.f;
		if (!IsPlaying())
		{
			Play();
		}
		SetComponentTickEnabled(false);
	}
	else
	{
		// Let the release (60 ms) finish before the voice is released.
		StopCountdown = 0.25f;
		SetComponentTickEnabled(true);
	}
}

void UGTPHornComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	StopCountdown -= DeltaTime;
	if (!bHornOn && StopCountdown <= 0.f)
	{
		Stop();
		SetComponentTickEnabled(false);
	}
}

void UGTPHornComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();
	if (HornWave)
	{
		HornWave->Silence();
	}
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}
