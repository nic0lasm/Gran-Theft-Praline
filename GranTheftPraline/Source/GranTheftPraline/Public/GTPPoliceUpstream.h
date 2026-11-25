#pragma once

// Where a police car is made so that it has a road to the player: upstream of him, on the lanes that lead to the
// one he drives on. Written without engine types (tools/vehicles/wanted_sim/police_upstream_test.cpp).
// Distances are in the lanes' own unit.
//
// Until gate 91 a car was made on any lane of a ring around the player, heading wherever that lane goes: in a
// quarter of one-way streets it then had the block to drive round, and the player was gone. Gates 83 to 91: the
// nearest unit 98 to 165 m away on average, 47 % of the time within 80 m at best and 0 % at gate 91, with units
// that moved 65 to 89 % of their time. A car made behind him on his own road has only to drive it.

#include <cmath>

namespace GTPPoliceUpstream
{
	struct FPlace
	{
		int Lane = -1;
		int Point = 0;
		/** The straight line to the player, and the way back to him along the lanes. */
		float Straight = 0.f;
		float Along = 0.f;
	};

	/**
	 * Walks back from the player's place on his lane (Lane, Point), lane after lane, and gives the first place
	 * between Nearest and Farthest from him in a straight line that Accept takes.
	 *
	 * TLanes: int Num(); int NumPoints(int Lane); float X(int Lane, int Point); float Y(int Lane, int Point);
	 *         int NumNext(int Lane); int Next(int Lane, int Index).
	 * Pick(Count): which of the Count lanes that lead to the one walked is taken (0 to Count - 1).
	 * Accept(Lane, Point): the place is free and out of sight. Asked every Spacing at most, MaxLooks times at most.
	 *
	 * A place keeps the rules of the ring: a lane of four points and more that leads somewhere, not at its ends.
	 * The walk ends at a lane nothing leads to, after three times Farthest along the lanes, or after as many lanes
	 * as there are (a block driven round).
	 */
	template <typename TLanes, typename TPick, typename TAccept>
	bool Find(const TLanes& Lanes, int Lane, int Point, float PlayerX, float PlayerY, float Nearest, float Farthest, float Spacing, int MaxLooks,
		TPick&& Pick, TAccept&& Accept, FPlace& Out)
	{
		const int Count = Lanes.Num();
		if (Lane < 0 || Lane >= Count || Lanes.NumPoints(Lane) < 1 || !(Nearest < Farthest))
		{
			return false;
		}
		Point = Point < 0 ? 0 : (Point >= Lanes.NumPoints(Lane) ? Lanes.NumPoints(Lane) - 1 : Point);
		float Along = 0.f;
		float LastLook = -Spacing;
		int Looks = 0;
		float AtX = Lanes.X(Lane, Point), AtY = Lanes.Y(Lane, Point);
		for (int Hops = 0; Hops <= Count; ++Hops)
		{
			const int Points = Lanes.NumPoints(Lane);
			for (; Point >= 0; --Point)
			{
				const float X = Lanes.X(Lane, Point), Y = Lanes.Y(Lane, Point);
				Along += std::sqrt((X - AtX) * (X - AtX) + (Y - AtY) * (Y - AtY));
				AtX = X;
				AtY = Y;
				if (Along > Farthest * 3.f)
				{
					return false;
				}
				const float Straight = std::sqrt((X - PlayerX) * (X - PlayerX) + (Y - PlayerY) * (Y - PlayerY));
				if (Straight < Nearest || Straight > Farthest || Points < 4 || Point < 1 || Point > Points - 3 || Lanes.NumNext(Lane) == 0
					|| Along - LastLook < Spacing)
				{
					continue;
				}
				LastLook = Along;
				if (Accept(Lane, Point))
				{
					Out.Lane = Lane;
					Out.Point = Point;
					Out.Straight = Straight;
					Out.Along = Along;
					return true;
				}
				if (++Looks >= MaxLooks)
				{
					return false;
				}
			}
			// The lanes that lead to this one.
			int Leading = 0;
			for (int Other = 0; Other < Count; ++Other)
			{
				for (int Index = 0; Other != Lane && Index < Lanes.NumNext(Other); ++Index)
				{
					if (Lanes.Next(Other, Index) == Lane)
					{
						++Leading;
						break;
					}
				}
			}
			if (Leading == 0)
			{
				return false;
			}
			int Taken = Pick(Leading);
			Taken = Taken < 0 ? 0 : (Taken >= Leading ? Leading - 1 : Taken);
			int Before = -1;
			for (int Other = 0; Other < Count && Before < 0; ++Other)
			{
				for (int Index = 0; Other != Lane && Index < Lanes.NumNext(Other); ++Index)
				{
					if (Lanes.Next(Other, Index) == Lane)
					{
						if (Taken-- == 0)
						{
							Before = Other;
						}
						break;
					}
				}
			}
			if (Before < 0 || Lanes.NumPoints(Before) < 1)
			{
				return false;
			}
			Lane = Before;
			Point = Lanes.NumPoints(Lane) - 1;
		}
		return false;
	}

	/**
	 * The speed (km/h) a car made upstream rolls at from its first frame: the player's own, never more than the car
	 * is allowed in its chase nor than StartKmh, and none at all for a player who hardly moves.
	 */
	inline float RollKmh(float PlayerKmh, float ChaseKmh, float StartKmh)
	{
		const float Kmh = PlayerKmh < ChaseKmh ? PlayerKmh : ChaseKmh;
		return Kmh < 10.f ? 0.f : (Kmh < StartKmh ? Kmh : StartKmh);
	}
}
