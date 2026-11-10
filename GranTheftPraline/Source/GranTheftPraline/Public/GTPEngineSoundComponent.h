#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "GTPEngineSynth.h"

#include <atomic>
#include "GTPEngineSoundComponent.generated.h"

class AGTPVehicle;

/** Streams the procedural engine synth to the audio mixer (16-bit mono). */
UCLASS()
class GRANTHEFTPRALINE_API UGTPEngineSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	void SetSynth(const TSharedPtr<FGTPEngineSynth, ESPMode::ThreadSafe>& InSynth) { Synth = InSynth; }

	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) override;

	/** Samples rendered so far by the audio thread (read by the game thread for the log). */
	int64 GetRenderedSamples() const { return RenderedSamples.load(std::memory_order_relaxed); }

private:
	std::atomic<int64> RenderedSamples{0};
	TSharedPtr<FGTPEngineSynth, ESPMode::ThreadSafe> Synth;
	TArray<float> Scratch;
};

/**
 * Engine sound of a car, synthesised from the Chaos engine rpm and the
 * throttle (GTPEngineSynth.h): no sound asset is needed, and the pitch
 * follows every gear change. Plays while someone sits at the wheel.
 * The art pass may replace it with recorded loops (docs/gameplay/vehicles.md).
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent), Config = Game)
class GRANTHEFTPRALINE_API UGTPEngineSoundComponent : public UAudioComponent
{
	GENERATED_BODY()

public:
	UGTPEngineSoundComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Cylinders of the engine: sets the firing pitch (Clio V TCe: 3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engine Sound")
	int32 Cylinders = 3;

	/** Overall level of the engine, multiplied with the mix. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Engine Sound")
	float EngineVolume = 0.8f;

	/** Distance at which the engine starts to fade, and is silent (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Sound")
	float FadeStartDistance = 400.f;

	UPROPERTY(Config, EditAnywhere, Category = "Engine Sound")
	float SilentDistance = 4500.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartEngine();
	void StopEngine();

	UPROPERTY(Transient)
	TObjectPtr<UGTPEngineSoundWave> EngineWave;

	TSharedPtr<FGTPEngineSynth, ESPMode::ThreadSafe> Synth;

	/** Engine profile of the car model (UGTPAudioSettings::EngineProfiles, ADR-0009): rpm mapping and gear-change cut. */
	float PhysicsIdleRpm = 0.f;
	float PhysicsMaxRpm = 0.f;
	float SoundIdleRpm = 0.f;
	float SoundMaxRpm = 0.f;
	float ShiftCutSeconds = 0.f;
	float ShiftCutLeft = 0.f;
	int32 LastGear = 0;
	FString ProfileLabel;
	bool bEngineOn = false;
	bool bLoggedFirstAudio = false;
};
