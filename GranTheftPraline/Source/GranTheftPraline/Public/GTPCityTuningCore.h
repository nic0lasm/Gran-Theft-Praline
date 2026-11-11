#pragma once

// Cull distance arithmetic of UGTPCityRenderTuning, without Unreal types (checked by
// tools/city/tests/city_tuning_test.cpp). Distances are Unreal cm; an end distance of 0 means
// "never culled" in Unreal and is left as it is.

#include <algorithm>
#include <cmath>

namespace GTPCityTuning
{
// Culling starts at this share of the end distance, as generated (place_props.py, place_facades.py).
constexpr float StartShare = 0.8f;
// Furniture and detail distances of the cell levels, percent of the generated ones (budget
// compensation, optimisation thread 11/11: 70 % = GPU p95 24.05 ms at cap 40, 0.05 over the gate).
constexpr int DefaultCellDetailPct = 60;
// A detail is never cut nearer than this, whatever the percentage.
constexpr int MinEndCm = 500;

/** Percentage from the command line, bounded to 10..100 (anything else is clamped). */
inline int ClampPct(int Pct)
{
	return std::min(100, std::max(10, Pct));
}

/** End distance scaled by Pct percent; 0 (never culled) and non-positive ends stay unchanged. */
inline int ScaledEndCm(int EndCm, int Pct)
{
	if (EndCm <= 0)
	{
		return EndCm;
	}
	const long Scaled = std::lround(double(EndCm) * ClampPct(Pct) / 100.0);
	return int(std::max<long>(std::min<long>(MinEndCm, EndCm), Scaled));
}

/** Start of the fade for an end distance. */
inline int StartCm(int EndCm)
{
	return int(std::lround(double(EndCm) * StartShare));
}
}  // namespace GTPCityTuning
