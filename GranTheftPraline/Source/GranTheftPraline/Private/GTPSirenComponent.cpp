#include "GTPSirenComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPSurfaceLogic.h"
#include "GTPSynthSoundWave.h"

UGTPSirenComponent::UGTPSirenComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bAutoActivate = false;
	bAllowSpatialization = true;
}

UGTPSirenComponent* UGTPSirenComponent::FindOrAdd(AActor* Vehicle)
{
	if (!Vehicle || Vehicle->IsActorBeingDestroyed() || !UGTPAudioMix::IsGameAudioEnabled())
	{
		return nullptr;
	}
	if (UGTPSirenComponent* Existing = Vehicle->FindComponentByClass<UGTPSirenComponent>())
	{
		return Existing;
	}
	UGTPSirenComponent* Siren = NewObject<UGTPSirenComponent>(Vehicle, MakeUniqueObjectName(Vehicle, UGTPSirenComponent::StaticClass(), TEXT("GTPSiren")), RF_Transient);
	if (USceneComponent* Root = Vehicle->GetRootComponent())
	{
		Siren->SetupAttachment(Root);
	}
	// Light bar height: the siren speaker sits on the roof.
	Siren->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
	// An instance component: the actor owns it, saves it from GC and unregisters it with itself.
	Vehicle->AddInstanceComponent(Siren);
	Siren->RegisterComponent();
	return Siren;
}

UGTPSirenComponent* UGTPSirenComponent::SetSiren(AActor* Vehicle, EGTPSirenMode NewMode)
{
	UGTPSirenComponent* Siren = FindOrAdd(Vehicle);
	if (Siren)
	{
		Siren->SetMode(NewMode);
	}
	return Siren;
}

void UGTPSirenComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureWave();
}

void UGTPSirenComponent::EnsureWave()
{
	if (SirenWave)
	{
		return;
	}
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	if (UGTPAudioMix* Mix = UGTPAudioMix::Get(this))
	{
		SoundClassOverride = Mix->GetClass(EGTPAudioCategory::Sirens);
		AttenuationSettings = Mix->GetAttenuation(EGTPAudioCategory::Sirens);
		ConcurrencySet.Add(Mix->GetConcurrency(EGTPAudioCategory::Sirens));
	}
	Synth = MakeShared<FGTPSirenSynth, ESPMode::ThreadSafe>();
	Synth->Init(UGTPSynthSoundWave::SynthSampleRate, FGTPSirenSettings());
	TSharedPtr<FGTPSirenSynth, ESPMode::ThreadSafe> Shared = Synth;
	SirenWave = NewObject<UGTPSynthSoundWave>(this);
	SirenWave->Setup([Shared](float* Out, int32 Count) { Shared->Render(Out, Count); });
	SetSound(SirenWave);
	SetVolumeMultiplier(Settings->SirenVolume);
}

void UGTPSirenComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();
	if (SirenWave)
	{
		SirenWave->Silence();
	}
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UGTPSirenComponent::SetMode(EGTPSirenMode NewMode)
{
	if (NewMode == EGTPSirenMode::Count)
	{
		NewMode = EGTPSirenMode::Off;
	}
	EnsureWave();
	if (NewMode == Mode)
	{
		return;
	}
	Mode = NewMode;
	Synth->SetMode(static_cast<int>(NewMode));
	if (NewMode != EGTPSirenMode::Off)
	{
		StopCountdown = 0.f;
		bHasLastLocation = false;
		if (!IsPlaying())
		{
			Play();
		}
		SetComponentTickEnabled(true);
	}
	else
	{
		// Let the synth fade out (50 ms) before the voice is released.
		StopCountdown = 0.3f;
	}
}

void UGTPSirenComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Mode == EGTPSirenMode::Off)
	{
		StopCountdown -= DeltaTime;
		if (StopCountdown <= 0.f)
		{
			Stop();
			SetComponentTickEnabled(false);
		}
		return;
	}
	UpdateDoppler(DeltaTime);
}

void UGTPSirenComponent::UpdateDoppler(float DeltaTime)
{
	const FVector Location = GetComponentLocation();
	const FVector SourceVelocity = (bHasLastLocation && DeltaTime > KINDA_SMALL_NUMBER) ? (Location - LastLocation) / DeltaTime : FVector::ZeroVector;
	LastLocation = Location;
	bHasLastLocation = true;

	const APlayerController* Player = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!Player || !Synth.IsValid())
	{
		return;
	}
	FVector ListenerLocation, Front, Right;
	Player->GetAudioListenerPosition(ListenerLocation, Front, Right);
	const APawn* PlayerPawn = Player->GetPawn();
	const FVector ListenerVelocity = PlayerPawn ? PlayerPawn->GetVelocity() : FVector::ZeroVector;

	const FVector ToSource = (Location - ListenerLocation).GetSafeNormal();
	if (ToSource.IsNearlyZero())
	{
		return;
	}
	const float SourceAway = FVector::DotProduct(SourceVelocity, ToSource);
	const float ListenerToward = FVector::DotProduct(ListenerVelocity, ToSource);
	const float Target = GTPDopplerFactor(SourceAway, ListenerToward);
	// Smooth over ~0.1 s: teleports and frame hitches must not warble.
	SmoothedDoppler = FMath::FInterpTo(SmoothedDoppler, Target, DeltaTime, 10.f);
	Synth->SetPitchScale(SmoothedDoppler);
}
