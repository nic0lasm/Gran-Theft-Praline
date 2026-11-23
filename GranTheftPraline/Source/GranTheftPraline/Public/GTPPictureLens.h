#pragma once

// Lens of the picture (art bible section 9: a sober photo lens, not a body camera), written
// without engine types so it is checked outside Unreal (tools/environment/picture_lens_test.cpp).
// UGTPEnvironmentSubsystem reads the settings from DefaultGame.ini and puts the result on its
// post-process component, above the map's own volume.
//
// - Bloom: weak by day, on real light sources only; a little more at night, when the street
//   lamps and the shop windows are the sources.
// - Motion blur: moderate on foot and capped at 2 % of the screen (the engine's own 0.5 and 5 %
//   smeared the street as soon as the camera turned, gate 69 video), more marked when driving fast.
// - Vignette: light. Lens flares, chromatic aberration: none (body camera effects are excluded).
// - Sharpening: a little, in the tonemapper, for what the temporal upscaling softens.

struct FGTPLensSettings
{
	float Bloom = 0.3f;
	float NightBloom = 0.45f;
	float Vignette = 0.2f;
	float MotionBlur = 0.3f;
	float DrivingMotionBlur = 0.45f;
	float MotionBlurMaxPercent = 2.f;
	float DrivingMotionBlurMaxPercent = 3.f;
	float Sharpen = 0.4f;
};

struct FGTPLensState
{
	float Bloom = 0.f;
	float Vignette = 0.f;
	float MotionBlur = 0.f;
	float MotionBlurMaxPercent = 0.f;
	float Sharpen = 0.f;
};

namespace GTPPictureLens
{
	// Camera speeds between which the motion blur goes from the walking amount to the driving one.
	constexpr float DrivingStartKmh = 50.f;
	constexpr float DrivingFullKmh = 110.f;
	// A camera that covers more than this in one frame was moved (respawn, cut), not driven.
	constexpr float CutKmh = 400.f;
	// Seconds for the smoothed camera speed to follow a change (a kerb must not pump the blur).
	constexpr float SpeedSmoothingSeconds = 0.5f;

	inline float Clamp01(float Value)
	{
		return Value < 0.f ? 0.f : (Value > 1.f ? 1.f : Value);
	}

	inline float Lerp(float A, float B, float Alpha)
	{
		return A + (B - A) * Alpha;
	}

	/** 0 on foot and in town traffic, 1 at motorway speed. */
	inline float DrivingShare(float SpeedKmh)
	{
		return Clamp01((SpeedKmh - DrivingStartKmh) / (DrivingFullKmh - DrivingStartKmh));
	}

	/**
	 * Smoothed camera speed in km/h after one frame.
	 * Smoothed: the value of the frame before; MovedCm: what the camera covered this frame;
	 * a cut (more than CutKmh) and a frame without duration leave the value as it is.
	 */
	inline float FollowSpeed(float Smoothed, float MovedCm, float DeltaSeconds)
	{
		if (DeltaSeconds <= 0.f || MovedCm < 0.f)
		{
			return Smoothed;
		}
		const float Kmh = MovedCm / DeltaSeconds * 0.036f;
		if (Kmh > CutKmh)
		{
			return Smoothed;
		}
		const float Alpha = Clamp01(DeltaSeconds / SpeedSmoothingSeconds);
		return Lerp(Smoothed, Kmh, Alpha);
	}

	/** NightFactor: 0 by day, 1 when the city lights are fully on. */
	inline FGTPLensState Evaluate(const FGTPLensSettings& Settings, float NightFactor, float SpeedKmh)
	{
		const float Night = Clamp01(NightFactor);
		const float Driving = DrivingShare(SpeedKmh);
		FGTPLensState Out;
		Out.Bloom = Lerp(Settings.Bloom, Settings.NightBloom, Night);
		Out.Vignette = Settings.Vignette;
		Out.MotionBlur = Lerp(Settings.MotionBlur, Settings.DrivingMotionBlur, Driving);
		Out.MotionBlurMaxPercent = Lerp(Settings.MotionBlurMaxPercent, Settings.DrivingMotionBlurMaxPercent, Driving);
		Out.Sharpen = Settings.Sharpen;
		return Out;
	}
}
