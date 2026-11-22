#pragma once

// Where to park a car along a lane, for missions (AGTPTrafficNetwork::SpawnParkedVehicle).
// Plain C++ (no engine), checked by tools/vehicles/kerb_test.
#include <cmath>
#include <vector>

namespace GTPKerb
{
	struct FPoint2
	{
		float X = 0.f;
		float Y = 0.f;
	};

	/** A spot beside the segment A->B, at its middle, Offset to the right of travel (UE: +Y is right of +X). */
	struct FSpot
	{
		FPoint2 Position;
		float YawDegrees = 0.f;
	};

	inline bool SpotBeside(const FPoint2& A, const FPoint2& B, float Offset, FSpot& Out)
	{
		const float DX = B.X - A.X;
		const float DY = B.Y - A.Y;
		const float Length = std::hypot(DX, DY);
		if (Length < 1e-3f)
		{
			return false;
		}
		const float FX = DX / Length;
		const float FY = DY / Length;
		// Right of the forward vector in UE's left-handed top view: (-FY, FX).
		Out.Position.X = (A.X + B.X) * 0.5f - FY * Offset;
		Out.Position.Y = (A.Y + B.Y) * 0.5f + FX * Offset;
		Out.YawDegrees = std::atan2(FY, FX) * 57.29577951f;
		return true;
	}

	/** Distance from P to the segment A->B (same units). */
	inline float DistanceToSegment(const FPoint2& P, const FPoint2& A, const FPoint2& B)
	{
		const float DX = B.X - A.X;
		const float DY = B.Y - A.Y;
		const float LengthSq = DX * DX + DY * DY;
		float T = LengthSq > 1e-6f ? ((P.X - A.X) * DX + (P.Y - A.Y) * DY) / LengthSq : 0.f;
		T = T < 0.f ? 0.f : (T > 1.f ? 1.f : T);
		return std::hypot(P.X - (A.X + T * DX), P.Y - (A.Y + T * DY));
	}

	/**
	 * Segments of all lanes whose middle lies within Radius of Near, nearest first:
	 * (lane, first point) pairs. Lanes are polylines; a segment needs two points.
	 */
	struct FSegmentRef
	{
		int Lane = -1;
		int Point = -1;
		float Distance = 0.f;
	};

	inline std::vector<FSegmentRef> SegmentsNear(const std::vector<std::vector<FPoint2>>& Lanes, const FPoint2& Near, float Radius)
	{
		std::vector<FSegmentRef> Found;
		for (int L = 0; L < static_cast<int>(Lanes.size()); ++L)
		{
			const std::vector<FPoint2>& P = Lanes[L];
			for (int K = 0; K + 1 < static_cast<int>(P.size()); ++K)
			{
				const float MX = (P[K].X + P[K + 1].X) * 0.5f - Near.X;
				const float MY = (P[K].Y + P[K + 1].Y) * 0.5f - Near.Y;
				const float D = std::hypot(MX, MY);
				if (D <= Radius)
				{
					Found.push_back({L, K, D});
				}
			}
		}
		// Insertion sort: called once per mission request, a few hundred segments at most.
		for (size_t I = 1; I < Found.size(); ++I)
		{
			const FSegmentRef Item = Found[I];
			size_t J = I;
			while (J > 0 && Found[J - 1].Distance > Item.Distance)
			{
				Found[J] = Found[J - 1];
				--J;
			}
			Found[J] = Item;
		}
		return Found;
	}

	/**
	 * True when Spot is at least Clearance from every segment of Nearby (segments of Lanes, as SegmentsNear gives them).
	 * A spot to the right of one lane can stand on another one: the next lane the same way, a bend, a junction.
	 */
	inline bool IsClearOfLanes(const std::vector<std::vector<FPoint2>>& Lanes, const std::vector<FSegmentRef>& Nearby, const FPoint2& Spot, float Clearance)
	{
		for (const FSegmentRef& Ref : Nearby)
		{
			const std::vector<FPoint2>& Line = Lanes[Ref.Lane];
			if (DistanceToSegment(Spot, Line[Ref.Point], Line[Ref.Point + 1]) < Clearance)
			{
				return false;
			}
		}
		return true;
	}
}
