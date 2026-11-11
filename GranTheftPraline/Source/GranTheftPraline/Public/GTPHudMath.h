#pragma once

// Small maths of the combat HUD, no Unreal types (tested by tools/ui/tests/cpp).

#include <algorithm>
#include <cmath>

namespace GTPUI
{
/**
 * Reticle radius in pixels for a cone half-angle: the radius the cone has on screen at
 * any distance. FovDegrees is the horizontal field of view, ScreenWidth in pixels.
 */
inline float ReticleRadiusPixels(float SpreadDegrees, float FovDegrees, float ScreenWidth, float MinPixels = 5.f, float MaxPixels = 220.f)
{
	const float Half = std::tan(std::clamp(FovDegrees, 10.f, 170.f) * 0.5f * 3.14159265f / 180.f);
	const float Spread = std::tan(std::clamp(SpreadDegrees, 0.f, 80.f) * 3.14159265f / 180.f);
	return std::clamp(Spread / Half * ScreenWidth * 0.5f, MinPixels, MaxPixels);
}

/** Opacity of the hit marker: full at the hit, gone after Duration; 0 when there was no hit. */
inline float HitMarkerAlpha(float SecondsSinceHit, float Duration = 0.15f)
{
	if (SecondsSinceHit < 0.f || SecondsSinceHit >= Duration)
	{
		return 0.f;
	}
	return 1.f - SecondsSinceHit / Duration;
}

/** Colour of a health arc: green when healthy, amber, red when low (0..1 in, RGB out). */
struct FRgb
{
	float R, G, B;
};
inline FRgb HealthColor(float Health01)
{
	const float T = std::clamp(Health01, 0.f, 1.f);
	if (T > 0.5f)
	{
		const float K = (T - 0.5f) * 2.f; // 1 = full: green; 0 = half: amber
		return {0.95f - 0.6f * K, 0.75f + 0.1f * K, 0.2f + 0.1f * K};
	}
	const float K = T * 2.f; // 0 = empty: red; 1 = half: amber
	return {0.9f + 0.05f * K, 0.15f + 0.6f * K, 0.15f + 0.05f * K};
}
} // namespace GTPUI
