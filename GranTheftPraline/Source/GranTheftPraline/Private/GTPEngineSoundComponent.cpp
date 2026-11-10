#include "GTPEngineSoundComponent.h"

#include "GTPLog.h"
#include "GTPVehicle.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPEngineSound, Log, All);

namespace
{
	constexpr int32 EngineSampleRate = 48000;
}

int32 UGTPEngineSoundWave::GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded)
{
	// Called on the audio render thread; the synth only reads atomics written by the game thread.
	int16* Out = reinterpret_cast<int16*>(PCMData);
	if (!Synth.IsValid() || SamplesNeeded <= 0)
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
	Super::EndPlay(EndPlayReason);
}

void UGTPEngineSoundComponent::StartEngine()
{
	if (!bEngineOn && EngineWave)
	{
		bEngineOn = true;
		SetVolumeMultiplier(EngineVolume);
		Play();
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
	Synth->SetTarget(Vehicle->GetEngineRpm(), Vehicle->GetThrottleAmount());
	if (!bLoggedFirstAudio && EngineWave && EngineWave->GetRenderedSamples() > 0)
	{
		// Proof for the soak test that the mixer pulls the synth, once per car.
		bLoggedFirstAudio = true;
		GTP_TRACE(LogGTPEngineSound, TEXT("engine sound: %s playing, %lld samples rendered, %.0f rpm"), *Vehicle->GetName(),
			static_cast<long long>(EngineWave->GetRenderedSamples()), Vehicle->GetEngineRpm());
	}
}
