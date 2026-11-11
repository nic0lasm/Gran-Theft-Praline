// Far traffic (docs/performance/vehicle-cost.md section 6): cars past the ghost
// distance drawn as moving instances, with no actor, no physics and no driver.
// They follow the lanes, keep their distance to the car ahead on the same lane
// and stop at red lights. Plain C++, no engine types, so tools/vehicles/far_test
// checks it with g++.
#pragma once

#include "GTPDriveLogic.h"

#include <cmath>
#include <vector>

namespace GTPFar
{
	/** One lane as a polyline in metres, with the distance from its start at each point. */
	struct FLanePath
	{
		std::vector<FGTPDrivePoint> Points;
		std::vector<float> From;
		float Length = 0.f;
	};

	inline FLanePath MakeLanePath(const std::vector<FGTPDrivePoint>& Points)
	{
		FLanePath Path;
		Path.Points = Points;
		Path.From.reserve(Points.size());
		float Total = 0.f;
		for (size_t Index = 0; Index < Points.size(); ++Index)
		{
			if (Index > 0)
			{
				Total += std::hypot(Points[Index].X - Points[Index - 1].X, Points[Index].Y - Points[Index - 1].Y);
			}
			Path.From.push_back(Total);
		}
		Path.Length = Total;
		return Path;
	}

	/** Distance from the start of the lane of a car at point Index plus Along metres. */
	inline float DistanceOnLane(const FLanePath& Path, int Index, float Along)
	{
		if (Path.From.empty())
		{
			return 0.f;
		}
		const int Last = static_cast<int>(Path.From.size()) - 1;
		const int Clamped = Index < 0 ? 0 : (Index > Last ? Last : Index);
		const float D = Path.From[Clamped] + Along;
		return D < Path.Length ? D : Path.Length;
	}

	/**
	 * Free distance (m) from a car at Position on a lane to the nearest of Others
	 * (positions on the same lane) ahead of it; negative if none.
	 */
	inline float GapAhead(float Position, const std::vector<float>& Others)
	{
		float Best = -1.f;
		for (const float Other : Others)
		{
			const float Gap = Other - Position;
			if (Gap > 0.01f && (Best < 0.f || Gap < Best))
			{
				Best = Gap;
			}
		}
		return Best;
	}

	/**
	 * Speed (m/s) a far car wants: the lane limit, lowered so it can stop
	 * MinGap behind the car ahead (Gap, negative = none) and at the stop line
	 * (StopAt, negative = go on), braking at Decel.
	 */
	inline float WantSpeed(float Limit, float Gap, float StopAt, float MinGap, float Decel)
	{
		float Want = Limit;
		const auto Cap = [&Want, Decel](float Room)
		{
			const float V = Room > 0.f ? std::sqrt(2.f * Decel * Room) : 0.f;
			if (V < Want)
			{
				Want = V;
			}
		};
		if (Gap >= 0.f)
		{
			Cap(Gap - MinGap);
		}
		if (StopAt >= 0.f)
		{
			Cap(StopAt);
		}
		return Want > 0.f ? Want : 0.f;
	}

	/** What to do with a far car this frame. */
	enum class EAction
	{
		Keep,      // carry on as an instance
		Promote,   // swap with a driven car: close to the player now
		Relocate,  // out of sight: start again somewhere far
	};

	/**
	 * Handover rules, by distance to the player (cm): inside Handover a far car
	 * becomes a driven car when one can be freed (bCanPromote); out of sight, it
	 * starts again elsewhere when it comes inside Hide, goes past MaxDistance
	 * or stands at a dead end.
	 */
	inline EAction Decide(float ToPlayer, bool bSeen, bool bCanPromote, bool bDeadEnd, float Handover, float Hide, float MaxDistance)
	{
		if (ToPlayer < Handover && bCanPromote)
		{
			return EAction::Promote;
		}
		if (!bSeen && (ToPlayer < Hide || ToPlayer > MaxDistance || bDeadEnd))
		{
			return EAction::Relocate;
		}
		return EAction::Keep;
	}

	/**
	 * The driven car to swap with a far car that comes close: the farthest one
	 * past MinDistance (cm) and farther than the far car itself. -1 if none.
	 */
	inline int PickSwap(const std::vector<float>& DrivenDistances, float MinDistance, float FarCarDistance)
	{
		int Best = -1;
		float BestDistance = MinDistance > FarCarDistance ? MinDistance : FarCarDistance;
		for (size_t Index = 0; Index < DrivenDistances.size(); ++Index)
		{
			if (DrivenDistances[Index] > BestDistance)
			{
				BestDistance = DrivenDistances[Index];
				Best = static_cast<int>(Index);
			}
		}
		return Best;
	}

	/**
	 * Whether a lane's bounding box (MinX..MaxX, MinY..MaxY) can hold a point between
	 * RMin and RMax of (CX, CY): its nearest point is within RMax and its farthest corner
	 * at least RMin away. Picks the lanes worth trying for a spawn in a ring around the
	 * player, so a quarter-wide network does not waste its tries on lanes 1 km away.
	 */
	inline bool BoxTouchesRing(float MinX, float MinY, float MaxX, float MaxY, float CX, float CY, float RMin, float RMax)
	{
		const float NX = CX < MinX ? MinX : (CX > MaxX ? MaxX : CX);
		const float NY = CY < MinY ? MinY : (CY > MaxY ? MaxY : CY);
		if (std::hypot(NX - CX, NY - CY) > RMax)
		{
			return false;
		}
		const float FX = std::fabs(CX - MinX) > std::fabs(CX - MaxX) ? MinX : MaxX;
		const float FY = std::fabs(CY - MinY) > std::fabs(CY - MaxY) ? MinY : MaxY;
		return std::hypot(FX - CX, FY - CY) >= RMin;
	}
}
