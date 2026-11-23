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

	/** Distance between the segments A->B and C->D (same units), 0 when they cross. */
	inline float SegmentDistance(const FPoint2& A, const FPoint2& B, const FPoint2& C, const FPoint2& D)
	{
		const auto Side = [](const FPoint2& O, const FPoint2& P, const FPoint2& Q)
		{
			return (P.X - O.X) * (Q.Y - O.Y) - (P.Y - O.Y) * (Q.X - O.X);
		};
		const float SC = Side(A, B, C);
		const float SD = Side(A, B, D);
		const float SA = Side(C, D, A);
		const float SB = Side(C, D, B);
		if (((SC > 0.f && SD < 0.f) || (SC < 0.f && SD > 0.f)) && ((SA > 0.f && SB < 0.f) || (SA < 0.f && SB > 0.f)))
		{
			return 0.f;
		}
		return std::fmin(std::fmin(DistanceToSegment(A, C, D), DistanceToSegment(B, C, D)),
			std::fmin(DistanceToSegment(C, A, B), DistanceToSegment(D, A, B)));
	}

	/**
	 * The corners of a car standing on Spot and facing Spot.YawDegrees, seen from above, in order round it: Front ahead
	 * of Spot, Back behind it, HalfWidth to each side.
	 */
	inline void Footprint(const FSpot& Spot, float Front, float Back, float HalfWidth, FPoint2 (&Out)[4])
	{
		const float Yaw = Spot.YawDegrees * 0.01745329252f;
		const float FX = std::cos(Yaw);
		const float FY = std::sin(Yaw);
		const float Along[4] = {Front, Front, -Back, -Back};
		const float Aside[4] = {HalfWidth, -HalfWidth, -HalfWidth, HalfWidth};
		for (int I = 0; I < 4; ++I)
		{
			// Right of the forward vector: (-FY, FX), as in SpotBeside.
			Out[I].X = Spot.Position.X + FX * Along[I] - FY * Aside[I];
			Out[I].Y = Spot.Position.Y + FY * Along[I] + FX * Aside[I];
		}
	}

	/**
	 * True when the body of a car on Spot (Footprint) stays at least Clearance from every segment of Nearby but those of
	 * OwnLane, the lane it is parked beside (LaneAroundSpot looks at that one). Its centre can be clear of a lane its body
	 * stands on: parked beside a lane that meets another one, it points into it (gate 62: a Clio parked across a lane,
	 * its centre 226 cm from that lane's centre line, its bonnet over it).
	 */
	inline bool IsBodyClearOfLanes(const std::vector<std::vector<FPoint2>>& Lanes, const std::vector<FSegmentRef>& Nearby, const FSpot& Spot,
		float Front, float Back, float HalfWidth, float Clearance, int OwnLane)
	{
		FPoint2 Corners[4];
		Footprint(Spot, Front, Back, HalfWidth, Corners);
		for (const FSegmentRef& Ref : Nearby)
		{
			if (Ref.Lane == OwnLane)
			{
				continue;
			}
			const std::vector<FPoint2>& Line = Lanes[Ref.Lane];
			// A segment wholly inside the body is within HalfWidth of an edge: under Clearance as long as Clearance is wider.
			for (int I = 0; I < 4; ++I)
			{
				if (SegmentDistance(Corners[I], Corners[(I + 1) % 4], Line[Ref.Point], Line[Ref.Point + 1]) < Clearance)
				{
					return false;
				}
			}
		}
		return true;
	}

	/** How the lane runs around a spot beside one of its segments (cm). */
	struct FLaneAround
	{
		/** Length of lane before the segment's middle, and after it. */
		float FromStart = 0.f;
		float ToEnd = 0.f;
		/** How far the lane's points within the span come to the right of the segment's line, towards the parked car (0 if none does). */
		float TowardKerb = 0.f;
	};

	/**
	 * The lane Line around its segment Point, its points within Span (cm, along the lane) of the segment's middle. A car on
	 * the lane aims a few metres ahead and cuts the corners: where the lane ends (a junction, where the next lane often
	 * starts two metres aside) or bends towards the kerb, it drives where a car parked beside a straight would stand
	 * (gate 62: of the nine parked cars traffic queued behind, eight were less than 10 m from a lane's end or start).
	 */
	inline FLaneAround LaneAroundSpot(const std::vector<FPoint2>& Line, int Point, float Span)
	{
		FLaneAround Out;
		const int N = static_cast<int>(Line.size());
		if (Point < 0 || Point + 1 >= N)
		{
			return Out;
		}
		const FPoint2& A = Line[Point];
		const FPoint2& B = Line[Point + 1];
		const float Length = std::hypot(B.X - A.X, B.Y - A.Y);
		if (Length < 1e-3f)
		{
			return Out;
		}
		const float FX = (B.X - A.X) / Length;
		const float FY = (B.Y - A.Y) / Length;
		const float MX = (A.X + B.X) * 0.5f;
		const float MY = (A.Y + B.Y) * 0.5f;
		std::vector<float> Along(N, 0.f);
		for (int I = 1; I < N; ++I)
		{
			Along[I] = Along[I - 1] + std::hypot(Line[I].X - Line[I - 1].X, Line[I].Y - Line[I - 1].Y);
		}
		const float Middle = (Along[Point] + Along[Point + 1]) * 0.5f;
		Out.FromStart = Middle;
		Out.ToEnd = Along[N - 1] - Middle;
		for (int I = 0; I < N; ++I)
		{
			if (std::fabs(Along[I] - Middle) <= Span)
			{
				const float Right = (Line[I].X - MX) * -FY + (Line[I].Y - MY) * FX;
				Out.TowardKerb = std::fmax(Out.TowardKerb, Right);
			}
		}
		return Out;
	}
}
