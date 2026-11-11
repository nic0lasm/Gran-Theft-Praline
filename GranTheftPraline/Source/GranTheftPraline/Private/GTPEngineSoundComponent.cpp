#include "GTPEngineSoundComponent.h"

#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPLog.h"
#include "GTPVehicle.h"
#include "ChaosWheeledVehicleMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPEngineSound, Log, All);

namespace
{
	constexpr int32 EngineSampleRate = 48000;
}

int32 UGTPEngineSoundWave::GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded)
{
	// Called on the audio render thread; the synth only reads atomics written by the game thread.
	int16* Out = reinterpret_cast<int16*>(PCMData);
	if (!Synth.IsValid() || SamplesNeeded <= 0 || bSilenced.load(std::memory_order_acquire))
	{
		FMemory::Memzero(PCMData, SamplesNeeded * sizeof(int16));
		return SamplesNeeded * sizeof(int16);
	}
	Scratch.SetNumUninitialized(SamplesNeeded, EAllowShrinking::No);
	Synth->Render(Scratch.GetData(), SamplesNeeded);
	for (int32 Index = 0; Index < SamplesNeeded; ++Index)
	{
		Out[Index] = static_cast<int16>(FMath::Clamp(Scratch[Index], -1.f, 1.f) * 32767.f);
	}
	RenderedSamples.fetch_add(SamplesNeeded, std::memory_order_relaxed);
	return SamplesNeeded * sizeof(int16);
}

UGTPEngineSoundComponent::UGTPEngineSoundComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = false;
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
}

void UGTPEngineSoundComponent::BeginPlay()
{
	Super::BeginPlay();

	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.bSpatialize = true;
	AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	AttenuationOverrides.AttenuationShapeExtents = FVector(FadeStartDistance, 0.f, 0.f);
	AttenuationOverrides.FalloffDistance = FMath::Max(100.f, SilentDistance - FadeStartDistance);
	AttenuationOverrides.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

	FGTPEngineSynthSettings Settings;
	Settings.Cylinders = FMath::Max(1, Cylinders);
	// Sound of the car model (ADR-0009); -Audio=0 keeps the plain synth of version 0.3.
	const FGTPEngineProfile* Profile = UGTPAudioMix::IsGameAudioEnabled() && GetOwner()
		? GetDefault<UGTPAudioSettings>()->FindEngineProfile(GetOwner()->GetClass()->GetName()) : nullptr;
	const AGTPVehicle* Vehicle = Cast<AGTPVehicle>(GetOwner());
	const UChaosWheeledVehicleMovementComponent* Move = Vehicle ? Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovementComponent()) : nullptr;
	if (Profile && Move)
	{
		Settings.Cylinders = FMath::Max(1, Profile->Cylinders);
		Settings.IdleRpm = Profile->IdleRpm;
		Settings.MaxRpm = FMath::Max(Profile->IdleRpm + 500.f, Profile->MaxRpm);
		Settings.IdleGain = Profile->IdleGain;
		Settings.FullLoadGain = Profile->FullLoadGain;
		Settings.NoiseAmount = Profile->NoiseAmount;
		Settings.Jitter = Profile->Jitter;
		Settings.RingHarmonic = Profile->RingHarmonic;
		Settings.Diesel = Profile->Diesel;
		Settings.Turbo = Profile->Turbo;
		Settings.TurboPitchHz = Profile->TurboPitchHz;
		PhysicsIdleRpm = Move->EngineSetup.EngineIdleRPM;
		PhysicsMaxRpm = FMath::Max(PhysicsIdleRpm + 500.f, Move->EngineSetup.MaxRPM);
		SoundIdleRpm = Settings.IdleRpm;
		SoundMaxRpm = Settings.MaxRpm;
		ShiftCutSeconds = Profile->ShiftCutSeconds;
		ProfileLabel = Profile->Label.IsEmpty() ? Profile->Match : Profile->Label;
	}
	Synth = MakeShared<FGTPEngineSynth, ESPMode::ThreadSafe>();
	Synth->Init(EngineSampleRate, Settings);

	EngineWave = NewObject<UGTPEngineSoundWave>(this);
	EngineWave->SetSampleRate(EngineSampleRate);
	EngineWave->NumChannels = 1;
	EngineWave->Duration = INDEFINITELY_LOOPING_DURATION;
	EngineWave->SoundGroup = SOUNDGROUP_Default;
	EngineWave->bLooping = false;
	EngineWave->SetSynth(Synth);
	SetSound(EngineWave);
}

void UGTPEngineSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopEngine();
	// Also stops an engine still fading out after the voice budget took it away.
	Stop();
	if (EngineWave)
	{
		EngineWave->Silence();
	}
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UGTPEngineSoundComponent::StartEngine()
{
	if (!bEngineOn && EngineWave && bAudible)
	{
		bEngineOn = true;
		SetVolumeMultiplier(EngineVolume);
		// A short fade: the budget brings engines back while they are audible.
		FadeIn(0.25f);
	}
}

void UGTPEngineSoundComponent::SetAudible(bool bInAudible)
{
	if (bAudible == bInAudible)
	{
		return;
	}
	bAudible = bInAudible;
	if (!bAudible && bEngineOn)
	{
		bEngineOn = false;
		FadeOut(0.3f, 0.f);
	}
}

void UGTPEngineSoundComponent::StopEngine()
{
	if (bEngineOn)
	{
		bEngineOn = false;
		Stop();
	}
}

void UGTPEngineSoundComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AGTPVehicle* Vehicle = Cast<AGTPVehicle>(GetOwner());
	if (!Vehicle || !Synth.IsValid())
	{
		return;
	}
	if (!Vehicle->IsEngineRunning())
	{
		StopEngine();
		return;
	}
	StartEngine();
	float Rpm = Vehicle->GetEngineRpm();
	float Load = Vehicle->GetThrottleAmount();
	if (SoundMaxRpm > 0.f)
	{
		// Physics rpm range -> the model's own range (a diesel revs lower than the shared curve).
		const float Alpha = (Rpm - PhysicsIdleRpm) / (PhysicsMaxRpm - PhysicsIdleRpm);
		Rpm = SoundIdleRpm + Alpha * (SoundMaxRpm - SoundIdleRpm);
		const int32 Gear = Vehicle->GetGear();
		if (Gear != LastGear && LastGear > 0 && Gear > 0)
		{
			ShiftCutLeft = ShiftCutSeconds;
		}
		LastGear = Gear;
		if (ShiftCutLeft > 0.f)
		{
			ShiftCutLeft -= DeltaTime;
			Load = 0.f;
		}
	}
	Synth->SetTarget(Rpm, Load);
	if (!bLoggedFirstAudio && EngineWave && EngineWave->GetRenderedSamples() > 0)
	{
		// Proof for the soak test that the mixer pulls the synth, once per car.
		bLoggedFirstAudio = true;
		GTP_TRACE(LogGTPEngineSound, TEXT("engine sound: %s playing, %lld samples rendered, %.0f rpm, profile %s"), *Vehicle->GetName(),
			static_cast<long long>(EngineWave->GetRenderedSamples()), Vehicle->GetEngineRpm(), ProfileLabel.IsEmpty() ? TEXT("none") : *ProfileLabel);
	}
}
