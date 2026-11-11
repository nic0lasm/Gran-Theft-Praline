// Interior streaming rules (ADR-0012 §1): which interior levels should be loaded for a
// player position. An interior loads when the player comes within the preload radius of
// its door, stays loaded until they pass the unload radius (hysteresis, so walking along
// the window never reloads it), and at most MaxLoaded interiors are kept, nearest first.
// Plain C++, no engine types, so tools/interiors/core_test checks it with g++.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace GTPInterior
{
	struct FSite
	{
		/** Door position in cm. */
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;
		bool bLoaded = false;
	};

	struct FStreamingRules
	{
		float LoadRadius = 4000.f;    // cm
		float UnloadRadius = 7000.f;  // cm
		int MaxLoaded = 2;
	};

	inline float Distance(const FSite& Site, float X, float Y, float Z)
	{
		const float DX = Site.X - X, DY = Site.Y - Y, DZ = Site.Z - Z;
		return std::sqrt(DX * DX + DY * DY + DZ * DZ);
	}

	/** Wanted state of every site for a player at (X, Y, Z). */
	inline std::vector<bool> Plan(const std::vector<FSite>& Sites, float X, float Y, float Z, const FStreamingRules& Rules)
	{
		struct FCandidate
		{
			size_t Index;
			float Dist;
		};
		std::vector<FCandidate> Candidates;
		for (size_t Index = 0; Index < Sites.size(); ++Index)
		{
			const float Dist = Distance(Sites[Index], X, Y, Z);
			const float Limit = Sites[Index].bLoaded ? std::max(Rules.UnloadRadius, Rules.LoadRadius) : Rules.LoadRadius;
			if (Dist <= Limit)
			{
				Candidates.push_back({Index, Dist});
			}
		}
		std::stable_sort(Candidates.begin(), Candidates.end(), [](const FCandidate& A, const FCandidate& B) { return A.Dist < B.Dist; });
		std::vector<bool> Wanted(Sites.size(), false);
		for (size_t Rank = 0; Rank < Candidates.size() && int(Rank) < Rules.MaxLoaded; ++Rank)
		{
			Wanted[Candidates[Rank].Index] = true;
		}
		return Wanted;
	}

	/** Density levels and tier defaults (docs/gameplay/reference-interior.md section 3). */
	constexpr int MaxDensity = 3;
	inline int DefaultDensity(bool bM1Tier) { return bM1Tier ? 2 : 3; }
	inline int ClampDensity(int Density) { return Density < 0 ? 0 : (Density > MaxDensity ? MaxDensity : Density); }
	/** A component built for MinDensity is shown at Density when MinDensity <= Density. */
	inline bool ShownAt(int MinDensity, int Density) { return MinDensity <= Density; }
}
