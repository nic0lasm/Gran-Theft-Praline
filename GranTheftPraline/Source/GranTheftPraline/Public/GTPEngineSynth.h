#pragma once

// Procedural engine sound, written without engine types so it can be built
// and checked outside Unreal (tools/vehicles/engine_synth_check.cpp).
//
// Model: a four-stroke engine fires Cylinders / 2 times per crankshaft turn.
// Each firing is a short decaying pressure pulse; the train of pulses gives
// the low "firing" tone and its harmonics. Load (throttle) makes the pulses
// sharper and louder, which brightens the sound; intake noise rises with
// load and revs. Small cycle-to-cycle jitter keeps it from sounding like a
// pure buzzer. All parameters are smoothed so gear changes do not click.

#include <atomic>
#include <cmath>
#include <cstdint>

struct FGTPEngineSynthSettings
{
	/** Cylinders of the engine (a Clio V TCe 90 has three). */
	int Cylinders = 3;
	float IdleRpm = 850.f;
	float MaxRpm = 6000.f;
	/** Output gain at idle and at full load, before the soft limiter. */
	float IdleGain = 0.22f;
	float FullLoadGain = 0.6f;
	/** Share of filtered noise (intake, mechanical) in the mix at full load. */
	float NoiseAmount = 0.18f;
	/** Random spread of each firing's strength (0 = perfectly regular). */
	float Jitter = 0.12f;
	/** Smoothing time of rpm and load changes, seconds. */
	float SmoothingSeconds = 0.06f;
	/** Firing harmonic the pressure pulse rings at (body and exhaust resonance). */
	float RingHarmonic = 4.f;
	/** Diesel clatter: a short bright knock at each firing, strongest at low load (0 = petrol). */
	float Diesel = 0.f;
	/** Turbo: whistle that spools up with load and revs, and a hiss when the throttle is lifted (0 = none). */
	float Turbo = 0.f;
	/** Whistle pitch, Hz at the maximum rpm with the turbo fully spooled. */
	float TurboPitchHz = 5200.f;
};

class FGTPEngineSynth
{
public:
	void Init(int InSampleRate, const FGTPEngineSynthSettings& InSettings)
	{
		SampleRate = InSampleRate > 0 ? InSampleRate : 48000;
		Settings = InSettings;
		if (Settings.Cylinders < 1)
		{
			Settings.Cylinders = 1;
		}
		Rpm = Settings.IdleRpm;
		Load = 0.f;
		TargetRpm.store(Settings.IdleRpm);
		TargetLoad.store(0.f);
		Phase = 0.f;
		PulseEnv = 0.f;
		PulseAmp = 1.f;
		LowPass = LowPass1 = HighPassIn = HighPassOut = NoiseLp = 0.f;
		KnockEnv = KnockHp = KnockIn = Spool = WhistlePhase = BlowOff = BlowLp = 0.f;
		Seed = 0x2545F491u;
		Smoothing = 1.f - std::exp(-1.f / (Settings.SmoothingSeconds * static_cast<float>(SampleRate)));
		const float Rate = static_cast<float>(SampleRate);
		KnockDecay = std::exp(-1.f / (0.004f * Rate));
		SpoolUp = 1.f - std::exp(-1.f / (0.9f * Rate));
		SpoolDown = 1.f - std::exp(-1.f / (0.5f * Rate));
		BlowOffDecay = std::exp(-1.f / (0.25f * Rate));
		LastLoad = 0.f;
	}

	/** Target values, read by the next Render call (safe to call from another thread). */
	void SetTarget(float InRpm, float InLoad)
	{
		TargetRpm.store(Clamp(InRpm, Settings.IdleRpm * 0.6f, Settings.MaxRpm * 1.05f), std::memory_order_relaxed);
		TargetLoad.store(Clamp(InLoad, 0.f, 1.f), std::memory_order_relaxed);
	}

	/** Firing frequency for an rpm value, in Hz. */
	float FiringFrequency(float InRpm) const
	{
		return InRpm / 60.f * static_cast<float>(Settings.Cylinders) * 0.5f;
	}

	/** Writes Count mono samples in [-1, 1]. */
	void Render(float* Out, int Count)
	{
		const float Dt = 1.f / static_cast<float>(SampleRate);
		const float GoalRpm = TargetRpm.load(std::memory_order_relaxed);
		const float GoalLoad = TargetLoad.load(std::memory_order_relaxed);
		for (int Index = 0; Index < Count; ++Index)
		{
			Rpm += (GoalRpm - Rpm) * Smoothing;
			Load += (GoalLoad - Load) * Smoothing;

			const float Freq = FiringFrequency(Rpm);
			Phase += Freq * Dt;
			if (Phase >= 1.f)
			{
				Phase -= std::floor(Phase);
				// New firing: strength varies a little from one to the next.
				PulseAmp = 1.f + Settings.Jitter * (NextRandom() * 2.f - 1.f);
				PulseEnv = PulseAmp;
				KnockEnv = PulseAmp;
			}

			// Pulse decay: sharper (shorter) under load, a fraction of the firing period.
			const float Period = 1.f / Freq;
			const float DecayTime = Period * (0.45f - 0.25f * Load);
			PulseEnv *= std::exp(-Dt / DecayTime);

			// Pressure pulse rings at a body resonance (~ 4th firing harmonic, capped).
			const float Ring = std::sin(6.2831853f * Phase * Settings.RingHarmonic);
			float Sample = PulseEnv * (0.65f + 0.35f * Ring);

			// Intake / mechanical noise, low-passed, louder with load and revs.
			const float Revs = (Rpm - Settings.IdleRpm) / (Settings.MaxRpm - Settings.IdleRpm);
			NoiseLp += (NextRandom() * 2.f - 1.f - NoiseLp) * 0.15f;
			Sample += NoiseLp * Settings.NoiseAmount * (0.3f + 0.7f * Load) * (0.4f + 0.6f * Clamp(Revs, 0.f, 1.f)) * 3.f;

			// Exhaust muffler: two-pole low-pass, opened up by load (also rounds the pulse attack).
			const float Cut = 0.12f + 0.3f * Load;
			LowPass1 += (Sample - LowPass1) * Cut;
			LowPass += (LowPass1 - LowPass) * Cut;

			// Remove the DC offset of the one-sided pulses.
			HighPassOut = 0.995f * (HighPassOut + LowPass - HighPassIn);
			HighPassIn = LowPass;

			float Sample2 = HighPassOut;

			// Diesel knock: bright noise burst of a few ms after each firing, after the muffler.
			if (Settings.Diesel > 0.f)
			{
				KnockEnv *= KnockDecay;
				const float Noise = NextRandom() * 2.f - 1.f;
				KnockHp = 0.6f * (KnockHp + Noise - KnockIn);
				KnockIn = Noise;
				Sample2 += KnockHp * KnockEnv * Settings.Diesel * (1.f - 0.5f * Load) * 1.2f;
			}

			// Turbo: spool follows load times revs slowly; lifting off a spooled turbo hisses.
			if (Settings.Turbo > 0.f)
			{
				const float Goal = Load * Clamp(Revs * 1.4f, 0.f, 1.f);
				const float Before = Spool;
				Spool += (Goal - Spool) * (Goal > Spool ? SpoolUp : SpoolDown);
				if (Before > 0.45f && Load < 0.15f && BlowOff < 0.05f && LastLoad >= 0.15f)
				{
					BlowOff = Before;
				}
				LastLoad = Load;
				WhistlePhase += Settings.TurboPitchHz * (0.35f + 0.65f * Clamp(Rpm / Settings.MaxRpm, 0.f, 1.f)) * Spool * Dt;
				WhistlePhase -= std::floor(WhistlePhase);
				Sample2 += std::sin(6.2831853f * WhistlePhase) * Spool * Spool * Settings.Turbo * 0.04f;
				BlowOff *= BlowOffDecay;
				const float Hiss = NextRandom() * 2.f - 1.f;
				BlowLp += (Hiss - BlowLp) * 0.5f;
				Sample2 += (Hiss - BlowLp) * BlowOff * Settings.Turbo * 0.25f;
			}

			const float Gain = Settings.IdleGain + (Settings.FullLoadGain - Settings.IdleGain) * Load;
			Out[Index] = SoftLimit(Sample2 * Gain * 2.f);
		}
	}

	float GetRpm() const { return Rpm; }
	float GetLoad() const { return Load; }

private:
	static float Clamp(float Value, float Low, float High)
	{
		return Value < Low ? Low : (Value > High ? High : Value);
	}

	static float SoftLimit(float Value)
	{
		return std::tanh(Value);
	}

	/** Uniform in [0, 1), xorshift32. */
	float NextRandom()
	{
		Seed ^= Seed << 13;
		Seed ^= Seed >> 17;
		Seed ^= Seed << 5;
		return static_cast<float>(Seed >> 8) / 16777216.f;
	}

	FGTPEngineSynthSettings Settings;
	int SampleRate = 48000;
	float Smoothing = 0.001f;
	/** Written by the game thread, read by the audio thread. */
	std::atomic<float> TargetRpm{850.f};
	std::atomic<float> TargetLoad{0.f};
	float Rpm = 850.f;
	float Load = 0.f;
	float Phase = 0.f;
	float PulseEnv = 0.f;
	float PulseAmp = 1.f;
	float LowPass1 = 0.f;
	float LowPass = 0.f;
	float HighPassIn = 0.f;
	float HighPassOut = 0.f;
	float NoiseLp = 0.f;
	float KnockEnv = 0.f;
	float KnockHp = 0.f;
	float KnockIn = 0.f;
	float KnockDecay = 0.99f;
	float Spool = 0.f;
	float SpoolUp = 0.0001f;
	float SpoolDown = 0.0001f;
	float WhistlePhase = 0.f;
	float BlowOff = 0.f;
	float BlowOffDecay = 0.999f;
	float BlowLp = 0.f;
	float LastLoad = 0.f;
	uint32_t Seed = 0x2545F491u;
};
