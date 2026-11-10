#include "GTPSynthSoundWave.h"

void UGTPSynthSoundWave::Setup(TFunction<void(float*, int32)>&& InRender)
{
	Render = MoveTemp(InRender);
	SetSampleRate(SynthSampleRate);
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	SoundGroup = SOUNDGROUP_Default;
	bLooping = false;
}

int32 UGTPSynthSoundWave::GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded)
{
	int16* Out = reinterpret_cast<int16*>(PCMData);
	if (!Render || SamplesNeeded <= 0)
	{
		FMemory::Memzero(PCMData, FMath::Max(0, SamplesNeeded) * sizeof(int16));
		return FMath::Max(0, SamplesNeeded) * sizeof(int16);
	}
	Scratch.SetNumUninitialized(SamplesNeeded, EAllowShrinking::No);
	Render(Scratch.GetData(), SamplesNeeded);
	for (int32 Index = 0; Index < SamplesNeeded; ++Index)
	{
		Out[Index] = static_cast<int16>(FMath::Clamp(Scratch[Index], -1.f, 1.f) * 32767.f);
	}
	return SamplesNeeded * sizeof(int16);
}
