#pragma once

// Procedural sounds that change continuously with the game state, written
// without engine types so they can be rendered and checked outside Unreal
// (tools/audio/audio_logic_test.cpp, tools/audio/check_synths.py):
// - FGTPSirenSynth: French police « deux tons » and a chase yelp, with the
//   Doppler shift applied inside the oscillator (no resampling artefacts);
// - FGTPSkidSynth: tyre squeal driven by how much the tyres slide.
// Targets are atomics written by the game thread and read by the audio thread.

#include <atomic>
#include <cmath>
#include <cstdint>

/** Second-order band-pass (RBJ cookbook, constant 0 dB peak gain). */
struct FGTPBandPass
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

struct FGTPSirenSettings
{
	/** « Deux tons » of the French police and gendarmerie: la 435 Hz, ré 580 Hz. */
	float LowTone = 435.f;
	float HighTone = 580.f;
	/** Length of each tone (s). */
	float ToneSeconds = 0.55f;
	/** Yelp sweep, Hz and sweeps per second. */
	float YelpLow = 650.f;
	float YelpHigh = 1450.f;
	float YelpRate = 3.2f;
	/** Glide between the two tones (s): a real horn does not jump instantly. */
	float GlideSeconds = 0.025f;
	float Gain = 0.5f;
};

/** Mode values: 0 off, 1 two-tone, 2 yelp (same order as EGTPSirenMode). */
class FGTPSirenSynth
{
public:
	void Init(int InSampleRate, const FGTPSirenSettings& InSettings)
	{
		SampleRate = InSampleRate > 0 ? InSampleRate : 48000;
		Settings = InSettings;
		Freq = Settings.LowTone;
		Phase = ModePhase = Level = 0.f;
		LastMode = 0;
		Horn.Set(static_cast<float>(SampleRate), 1250.f, 0.9f);
		Glide = 1.f - std::exp(-1.f / (Settings.GlideSeconds * static_cast<float>(SampleRate)));
		Fade = 1.f - std::exp(-1.f / (0.05f * static_cast<float>(SampleRate)));
	}

	void SetMode(int InMode) { Mode.store(InMode, std::memory_order_relaxed); }
	int GetMode() const { return Mode.load(std::memory_order_relaxed); }
	/** Doppler factor (1 = none), from GTPDopplerFactor. */
	void SetPitchScale(float Scale) { PitchScale.store(Scale, std::memory_order_relaxed); }

	/** Frequency the siren is aiming at right now, before Doppler (Hz); 0 when off. */
	float TargetFrequency() const
	{
		const int CurrentMode = Mode.load(std::memory_order_relaxed);
		if (CurrentMode == 1)
		{
			return ModePhase < 0.5f ? Settings.LowTone : Settings.HighTone;
		}
		if (CurrentMode == 2)
		{
			// Fast rise, slower fall, like an electronic yelp.
			const float T = ModePhase;
			const float Shape = T < 0.35f ? T / 0.35f : 1.f - (T - 0.35f) / 0.65f;
			return Settings.YelpLow + (Settings.YelpHigh - Settings.YelpLow) * Shape;
		}
		return 0.f;
	}

	void Render(float* Out, int Count)
	{
		const float Dt = 1.f / static_cast<float>(SampleRate);
		const int CurrentMode = Mode.load(std::memory_order_relaxed);
		const float Scale = PitchScale.load(std::memory_order_relaxed);
		const float CycleSeconds = CurrentMode == 2 ? 1.f / Settings.YelpRate : 2.f * Settings.ToneSeconds;
		if (CurrentMode != LastMode)
		{
			// Every pattern starts from its beginning: the low tone, the bottom of the sweep.
			if (LastMode == 0 || CurrentMode != 0)
			{
				ModePhase = 0.f;
			}
			LastMode = CurrentMode;
		}
		for (int Index = 0; Index < Count; ++Index)
		{
			ModePhase += Dt / CycleSeconds;
			ModePhase -= std::floor(ModePhase);
			const float Goal = TargetFrequency();
			if (Goal > 0.f)
			{
				Freq += (Goal - Freq) * (CurrentMode == 1 ? Glide : 1.f);
			}
			const float GoalLevel = CurrentMode == 0 ? 0.f : 1.f;
			Level += (GoalLevel - Level) * Fade;

			Phase += Freq * Scale * Dt;
			Phase -= std::floor(Phase);
			// Odd harmonics up to ~6 kHz: a square wave without aliasing.
			float Wave = 0.f;
			const float Nyquist = 0.5f * static_cast<float>(SampleRate);
			for (int Harmonic = 1; Harmonic <= 13; Harmonic += 2)
			{
				if (Freq * Scale * Harmonic > Nyquist * 0.8f)
				{
					break;
				}
				Wave += std::sin(6.2831853f * Phase * Harmonic) / static_cast<float>(Harmonic);
			}
			// Horn loudspeaker: keep part of the raw tone, add its formant.
			const float Coloured = 0.55f * Wave + 1.4f * Horn.Process(Wave);
			Out[Index] = std::tanh(Coloured * Settings.Gain * Level * 1.5f);
		}
	}

	float GetLevel() const { return Level; }

private:
	FGTPSirenSettings Settings;
	FGTPBandPass Horn;
	int SampleRate = 48000;
	std::atomic<int> Mode{0};
	std::atomic<float> PitchScale{1.f};
	int LastMode = 0;
	float Freq = 435.f;
	float Phase = 0.f;
	float ModePhase = 0.f;
	float Level = 0.f;
	float Glide = 0.01f;
	float Fade = 0.001f;
};

struct FGTPSkidSettings
{
	/** Squeal pitch at a light and at a full slide (Hz). */
	float LowPitch = 620.f;
	float HighPitch = 980.f;
	float Gain = 0.45f;
	/** Smoothing of the slide amount (s). */
	float SmoothingSeconds = 0.05f;
};

/** Tyre squeal: a wavering tone plus hiss, scaled by the slide amount in [0, 1]. */
class FGTPSkidSynth
{
public:
	void Init(int InSampleRate, const FGTPSkidSettings& InSettings)
	{
		SampleRate = InSampleRate > 0 ? InSampleRate : 48000;
		Settings = InSettings;
		Amount = Phase = Wobble = WobbleTarget = 0.f;
		WobbleCountdown = 0;
		Hiss.Set(static_cast<float>(SampleRate), 2600.f, 0.7f);
		Body.Set(static_cast<float>(SampleRate), 900.f, 2.5f);
		Smoothing = 1.f - std::exp(-1.f / (Settings.SmoothingSeconds * static_cast<float>(SampleRate)));
	}

	void SetAmount(float InAmount)
	{
		TargetAmount.store(InAmount < 0.f ? 0.f : (InAmount > 1.f ? 1.f : InAmount), std::memory_order_relaxed);
	}

	void Render(float* Out, int Count)
	{
		const float Dt = 1.f / static_cast<float>(SampleRate);
		const float Goal = TargetAmount.load(std::memory_order_relaxed);
		for (int Index = 0; Index < Count; ++Index)
		{
			Amount += (Goal - Amount) * Smoothing;
			// Stick-slip: the pitch wanders a little every few milliseconds.
			if (--WobbleCountdown <= 0)
			{
				WobbleCountdown = SampleRate / 90 + static_cast<int>(NextRandom() * SampleRate / 60);
				WobbleTarget = NextRandom() * 2.f - 1.f;
			}
			Wobble += (WobbleTarget - Wobble) * 0.002f;
			const float Pitch = (Settings.LowPitch + (Settings.HighPitch - Settings.LowPitch) * Amount) * (1.f + 0.04f * Wobble);
			Phase += Pitch * Dt;
			Phase -= std::floor(Phase);
			const float Tone = std::sin(6.2831853f * Phase) + 0.3f * std::sin(6.2831853f * Phase * 2.f);
			const float Noise = NextRandom() * 2.f - 1.f;
			const float Sample = 0.6f * Body.Process(Tone + 0.5f * Noise) + 0.5f * Hiss.Process(Noise);
			// Squeal only above a light slide; below that, a faint scrub.
			const float Level = Amount * Amount;
			Out[Index] = std::tanh(Sample * Settings.Gain * Level * 2.f);
		}
	}

	float GetAmount() const { return Amount; }

private:
	float NextRandom()
	{
		Seed ^= Seed << 13;
		Seed ^= Seed >> 17;
		Seed ^= Seed << 5;
		return static_cast<float>(Seed >> 8) / 16777216.f;
	}

	FGTPSkidSettings Settings;
	FGTPBandPass Hiss;
	FGTPBandPass Body;
	int SampleRate = 48000;
	std::atomic<float> TargetAmount{0.f};
	float Amount = 0.f;
	float Phase = 0.f;
	float Wobble = 0.f;
	float WobbleTarget = 0.f;
	int WobbleCountdown = 0;
	float Smoothing = 0.001f;
	uint32_t Seed = 0x9E3779B9u;
};
