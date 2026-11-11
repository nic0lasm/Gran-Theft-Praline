#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"

#include <atomic>
#include "GTPSynthSoundWave.generated.h"

/**
 * Streams a mono procedural synth (GTPAudioSynths.h) to the audio mixer as
 * 16-bit PCM. The render function runs on the audio render thread: it must
 * only read state the game thread writes through atomics, and it keeps the
 * synth alive through its own shared pointer.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPSynthSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	static constexpr int32 SynthSampleRate = 48000;

	/** Sets the render function and the mono 48 kHz endless format. Call before playing. */
	void Setup(TFunction<void(float*, int32)>&& InRender);

	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) override;

	/**
	 * From EndPlay: the render function is no longer called (zeros only), in case
	 * the mixer still pulls a block after the owner left the game.
	 */
	void Silence() { bSilenced.store(true, std::memory_order_release); }

private:
	std::atomic<bool> bSilenced{false};
	TFunction<void(float*, int32)> Render;
	TArray<float> Scratch;
};
