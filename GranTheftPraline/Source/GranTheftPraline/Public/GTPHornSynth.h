#pragma once

// Car horn, synthesised like the siren (GTPAudioSynths.h, same band-pass) so no recorded asset is needed:
// two electric horns a major third apart (about 410 and 510 Hz, the usual European pair),
// buzzy odd harmonics through a small horn formant, quick attack and release.
// No engine types: rendered and checked outside Unreal (tools/vehicles/react_test).
// The game thread writes On and the pitch through atomics, the audio thread renders.

#include <atomic>
#include <cmath>

/** Second-order band-pass, constant 0 dB peak (RBJ cookbook), for the horn formant. */
struct FGTPHornFormant
{
	void Set(float SampleRate, float Centre, float Q)
	{
		const float W = 6.2831853f * Centre / SampleRate;
		const float Alpha = std::sin(W) / (2.f * Q);
		const float A0 = 1.f + Alpha;
		B0 = Alpha / A0;
		B2 = -Alpha / A0;
		A1 = -2.f * std::cos(W) / A0;
		A2 = (1.f - Alpha) / A0;
	}

	float Process(float X)
	{
		const float Y = B0 * X + B2 * X2 - A1 * Y1 - A2 * Y2;
		X2 = X1;
		X1 = X;
		Y2 = Y1;
		Y1 = Y;
		return Y;
	}

	float B0 = 0.f, B2 = 0.f, A1 = 0.f, A2 = 0.f;
	float X1 = 0.f, X2 = 0.f, Y1 = 0.f, Y2 = 0.f;
};

struct FGTPHornSettings
{
	float LowTone = 410.f;
	float HighTone = 512.f;
	/** Attack and release (s). */
	float AttackSeconds = 0.012f;
	float ReleaseSeconds = 0.06f;
	float Gain = 0.45f;
};

class FGTPHornSynth
{
public:
	void Init(int InSampleRate, const FGTPHornSettings& InSettings)
	{
		SampleRate = InSampleRate > 0 ? InSampleRate : 48000;
		Settings = InSettings;
		PhaseLow = PhaseHigh = Level = 0.f;
		Formant.Set(static_cast<float>(SampleRate), 2400.f, 1.1f);
		Attack = 1.f - std::exp(-1.f / (Settings.AttackSeconds * static_cast<float>(SampleRate)));
		Release = 1.f - std::exp(-1.f / (Settings.ReleaseSeconds * static_cast<float>(SampleRate)));
	}

	void SetOn(bool bInOn) { bOn.store(bInOn, std::memory_order_relaxed); }
	bool IsOn() const { return bOn.load(std::memory_order_relaxed); }
	/** Per-car pitch (0.9 to 1.1) and Doppler, multiplied together. */
	void SetPitchScale(float Scale) { PitchScale.store(Scale, std::memory_order_relaxed); }
	float GetLevel() const { return Level; }

	void Render(float* Out, int Count)
	{
		const float Dt = 1.f / static_cast<float>(SampleRate);
		const float Goal = bOn.load(std::memory_order_relaxed) ? 1.f : 0.f;
		const float Scale = PitchScale.load(std::memory_order_relaxed);
		const float Nyquist = 0.5f * static_cast<float>(SampleRate);
		for (int Index = 0; Index < Count; ++Index)
		{
			Level += (Goal - Level) * (Goal > Level ? Attack : Release);
			PhaseLow += Settings.LowTone * Scale * Dt;
			PhaseLow -= std::floor(PhaseLow);
			PhaseHigh += Settings.HighTone * Scale * Dt;
			PhaseHigh -= std::floor(PhaseHigh);
			float Wave = 0.f;
			for (int Harmonic = 1; Harmonic <= 11; Harmonic += 2)
			{
				const float Weight = 1.f / static_cast<float>(Harmonic);
				if (Settings.LowTone * Scale * static_cast<float>(Harmonic) < Nyquist * 0.8f)
				{
					Wave += std::sin(6.2831853f * PhaseLow * static_cast<float>(Harmonic)) * Weight;
				}
				if (Settings.HighTone * Scale * static_cast<float>(Harmonic) < Nyquist * 0.8f)
				{
					Wave += std::sin(6.2831853f * PhaseHigh * static_cast<float>(Harmonic)) * Weight;
				}
			}
			const float Coloured = 0.5f * Wave + 1.2f * Formant.Process(Wave);
			Out[Index] = std::tanh(Coloured * Settings.Gain * Level);
		}
	}

private:
	FGTPHornSettings Settings;
	FGTPHornFormant Formant;
	int SampleRate = 48000;
	std::atomic<bool> bOn{false};
	std::atomic<float> PitchScale{1.f};
	float PhaseLow = 0.f;
	float PhaseHigh = 0.f;
	float Level = 0.f;
	float Attack = 0.01f;
	float Release = 0.001f;
};
