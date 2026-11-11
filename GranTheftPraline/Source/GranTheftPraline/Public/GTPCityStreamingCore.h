#pragma once

// Which city cells to stream in, without Unreal types (checked by
// tools/city/tests/city_streaming_test.cpp). UGTPCityStreaming (GTPCityStreaming.h)
// applies the plan: loads or unloads the detailed level of each cell and hides its far
// proxy while that level is visible (ADR-0010).
//
// Rules:
// - a cell is wanted when the player is within LoadRadius of its box (0 inside);
// - a loaded cell is kept until the player is beyond UnloadRadius (hysteresis, so standing
//   on a boundary does not load and unload the same level every second);
// - at most MaxDetailed cells, the nearest first (2 x 2 by default, ADR-0010 condition 1);
// - at most MaxLoadsPerUpdate new requests per update, the nearest first, so a teleport
//   does not queue every level at once.
// Coordinates: Unreal cm, X east, Y south; only X and Y count.

#include <algorithm>
#include <cmath>
#include <vector>

namespace GTPCity
{
struct FCellBox
{
	float X0 = 0.f;
	float Y0 = 0.f;
	float X1 = 0.f;
	float Y1 = 0.f;
};

struct FStreamingRules
{
	float LoadRadius = 6000.f;    // 60 m: the neighbouring cell comes in before the player sees its facades up close
	float UnloadRadius = 9000.f;  // 90 m
	int MaxDetailed = 4;
	int MaxLoadsPerUpdate = 1;
};

/** Distance from (X, Y) to the box, 0 inside. */
inline float DistanceToBox(const FCellBox& B, float X, float Y)
{
	const float DX = std::max({B.X0 - X, 0.f, X - B.X1});
	const float DY = std::max({B.Y0 - Y, 0.f, Y - B.Y1});
	return std::sqrt(DX * DX + DY * DY);
}

struct FStreamingPlan
{
	std::vector<int> Load;    // cells to request now
	std::vector<int> Unload;  // cells to release now
	std::vector<int> Keep;    // cells wanted and already loaded (or loading)
};

/** Loaded[i]: the detailed level of cell i is loaded or being loaded. */
inline FStreamingPlan PlanStreaming(const std::vector<FCellBox>& Cells, const std::vector<bool>& Loaded,
                                    float X, float Y, const FStreamingRules& Rules)
{
	struct FCand
	{
		int Index;
		float Dist;
	};
	std::vector<FCand> Wanted;
	FStreamingPlan Plan;
	const int N = static_cast<int>(Cells.size());
	for (int I = 0; I < N; ++I)
	{
		const float D = DistanceToBox(Cells[I], X, Y);
		const bool IsLoaded = I < static_cast<int>(Loaded.size()) && Loaded[I];
		if (D <= Rules.LoadRadius || (IsLoaded && D <= Rules.UnloadRadius))
		{
			Wanted.push_back({I, D});
		}
	}
	std::stable_sort(Wanted.begin(), Wanted.end(), [](const FCand& A, const FCand& B) { return A.Dist < B.Dist; });
	if (static_cast<int>(Wanted.size()) > Rules.MaxDetailed)
	{
		Wanted.resize(std::max(Rules.MaxDetailed, 0));
	}
	std::vector<bool> Keep(N, false);
	int Loads = 0;
	for (const FCand& C : Wanted)
	{
		const bool IsLoaded = C.Index < static_cast<int>(Loaded.size()) && Loaded[C.Index];
		if (IsLoaded)
		{
			Keep[C.Index] = true;
			Plan.Keep.push_back(C.Index);
		}
		else if (Loads < Rules.MaxLoadsPerUpdate)
		{
			Keep[C.Index] = true;
			Plan.Load.push_back(C.Index);
			++Loads;
		}
	}
	for (int I = 0; I < N; ++I)
	{
		if (I < static_cast<int>(Loaded.size()) && Loaded[I] && !Keep[I])
		{
			Plan.Unload.push_back(I);
		}
	}
	return Plan;
}
} // namespace GTPCity
