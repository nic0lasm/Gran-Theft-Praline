#pragma once

// A police car that waits for the man it is after: made in a side street ahead of him, out of his sight, its nose
// to the road he drives, it stands there until he comes by and pulls out behind him. Written without engine types
// (tools/vehicles/wanted_sim/police_intercept_test.cpp). Distances are in the lanes' own unit, speeds in km/h.
//
// Until gate 102 every car sent after a player who drives was made behind him and had his road to drive.
// Gates 99 to 102: a unit within 80 m of him 10, 43, 8 and 5 % of the time, the nearest one 146 to 169 m away on
// average, with units that drive (40 km/h on average after a player at 59, no back-off, none given up at 102). A car
// that follows another through the same bends at the same speeds stays the same number of SECONDS behind it: made
// 100 m behind a car that leaves its kerb at 20 km/h, it is ten seconds behind, and ten seconds are 160 m once that
// car does 59 km/h. No top speed makes them up between two corners 140 m apart
// (tools/vehicles/wanted_sim/police_intercept_test.cpp replays the flight of gates 93 and 102).
// A car that pulls out of a side street two seconds after he has passed is two seconds behind him.

#include <cmath>

namespace GTPPoliceIntercept
{
	struct FSettings
	{
		/** The car stands this far from the end of its side street (lanes' unit: cm in the game). */
		float Back = 2500.f;
		/** A side street no shorter than this (lanes' unit): the car needs its 25 m to come out behind him and not in front. */
		float MinLength = 3000.f;
		/** A lane is a side street when it meets his way at more than this angle (cosine of it, 26 degrees): not another
		 *  lane of his own road. And at less than that one (134 degrees): not a street the car would have to turn back from. */
		float MaxCos = 0.9f;
		float MinCos = -0.7f;
		/** It pulls out when he is nearer than what he drives in JoinSeconds, and never before he is JoinMinM away (m, s). */
		float JoinSeconds = 2.5f;
		float JoinMinM = 27.f;
		/** He was nearer by this much and goes away: he turned off before its street, or he is past it (m). */
		float PassedM = 30.f;
		/** It waits no longer than this (s). */
		float MaxWaitS = 20.f;
		/** He stands still (slower than StandKmh for StandS): it does not wait for a man who no longer comes (km/h, s). */
		float StandKmh = 8.f;
		float StandS = 2.f;
	};

	struct FPlace
	{
		int Lane = -1;
		int Point = 0;
		/** The lane of his way the side street leads into. */
		int Joins = -1;
		/** The straight line from him to the place, and his way to the junction, along the lanes. */
		float Straight = 0.f;
		float Along = 0.f;
	};

	/**
	 * Walks forward from the player's place on his lane (Lane, Point) along the way he takes, junction after junction,
	 * and looks at each for a side street: another lane that leads into the lane he takes next, and meets his own at an
	 * angle. Gives the first place on one of them, S.Back before its end, between Nearest and Farthest from him in a
	 * straight line, that Accept takes.
	 *
	 * TLanes: int Num(); int NumPoints(int Lane); float X(int Lane, int Point); float Y(int Lane, int Point);
	 *         int NumNext(int Lane); int Next(int Lane, int Index).
	 * Way(Lane): the lane he takes after Lane when that is known (a flight on a circuit), anything else when it is
	 *            not: the exit that turns the least is then walked.
	 * Accept(Lane, Point): the place is free and out of sight. Asked MaxLooks times at most.
	 *
	 * A place keeps the rules of the ring: a lane of four points and more, not at its ends.
	 * The walk ends at a lane that leads nowhere, past Farthest and a half along his way, or after as many lanes as
	 * there are.
	 */
	template <typename TLanes, typename TWay, typename TAccept>
	bool Find(const TLanes& Lanes, int Lane, int Point, float PlayerX, float PlayerY, float Nearest, float Farthest, const FSettings& S, int MaxLooks,
		TWay&& Way, TAccept&& Accept, FPlace& Out)
	{
		const int Count = Lanes.Num();
		if (Lane < 0 || Lane >= Count || Lanes.NumPoints(Lane) < 2 || !(Nearest < Farthest))
		{
			return false;
		}
		const auto Length = [](float Dx, float Dy) { return std::sqrt(Dx * Dx + Dy * Dy); };
		// The heading of a lane where it ends, and where it starts.
		const auto EndHeading = [&Lanes, &Length](int L, float& Hx, float& Hy)
		{
			const int N = Lanes.NumPoints(L);
			Hx = Lanes.X(L, N - 1) - Lanes.X(L, N - 2);
			Hy = Lanes.Y(L, N - 1) - Lanes.Y(L, N - 2);
			const float Norm = Length(Hx, Hy);
			Hx = Norm > 1e-4f ? Hx / Norm : 0.f;
			Hy = Norm > 1e-4f ? Hy / Norm : 0.f;
		};
		const auto StartHeading = [&Lanes, &Length](int L, float& Hx, float& Hy)
		{
			Hx = Lanes.X(L, 1) - Lanes.X(L, 0);
			Hy = Lanes.Y(L, 1) - Lanes.Y(L, 0);
			const float Norm = Length(Hx, Hy);
			Hx = Norm > 1e-4f ? Hx / Norm : 0.f;
			Hy = Norm > 1e-4f ? Hy / Norm : 0.f;
		};
		Point = Point < 0 ? 0 : (Point >= Lanes.NumPoints(Lane) ? Lanes.NumPoints(Lane) - 1 : Point);
		// What is left of his lane.
		float Along = 0.f;
		for (int Index = Point; Index + 1 < Lanes.NumPoints(Lane); ++Index)
		{
			Along += Length(Lanes.X(Lane, Index + 1) - Lanes.X(Lane, Index), Lanes.Y(Lane, Index + 1) - Lanes.Y(Lane, Index));
		}
		int Looks = 0;
		for (int Hops = 0; Hops < Count; ++Hops)
		{
			if (Along > Farthest * 1.5f || Lanes.NumNext(Lane) == 0)
			{
				return false;
			}
			float WayX = 0.f, WayY = 0.f;
			EndHeading(Lane, WayX, WayY);
			// The lane he takes: the one named when it is an exit of this one, else the exit that turns the least.
			int Taken = -1;
			const int Named = Way(Lane);
			float BestCos = -2.f;
			for (int Index = 0; Index < Lanes.NumNext(Lane); ++Index)
			{
				const int Exit = Lanes.Next(Lane, Index);
				if (Exit < 0 || Exit >= Count || Lanes.NumPoints(Exit) < 2)
				{
					continue;
				}
				if (Exit == Named)
				{
					Taken = Exit;
					break;
				}
				float Hx = 0.f, Hy = 0.f;
				StartHeading(Exit, Hx, Hy);
				const float Cos = Hx * WayX + Hy * WayY;
				if (Cos > BestCos)
				{
					BestCos = Cos;
					Taken = Exit;
				}
			}
			if (Taken < 0)
			{
				return false;
			}
			// The other lanes that lead into the one he takes.
			for (int Other = 0; Other < Count; ++Other)
			{
				const int Points = Lanes.NumPoints(Other);
				if (Other == Lane || Other == Taken || Points < 4)
				{
					continue;
				}
				bool bLeads = false;
				for (int Index = 0; Index < Lanes.NumNext(Other) && !bLeads; ++Index)
				{
					bLeads = Lanes.Next(Other, Index) == Taken;
				}
				if (!bLeads)
				{
					continue;
				}
				float Hx = 0.f, Hy = 0.f;
				EndHeading(Other, Hx, Hy);
				const float Cos = Hx * WayX + Hy * WayY;
				if (Cos > S.MaxCos || Cos < S.MinCos)
				{
					// Another lane of his own road, or one that comes the other way.
					continue;
				}
				// S.Back before its end, never at its ends.
				float Whole = 0.f;
				int At = -1;
				for (int Index = Points - 1; Index > 0; --Index)
				{
					const float Step = Length(Lanes.X(Other, Index) - Lanes.X(Other, Index - 1), Lanes.Y(Other, Index) - Lanes.Y(Other, Index - 1));
					Whole += Step;
					if (At < 0 && Whole >= S.Back && Index - 1 >= 1 && Index - 1 <= Points - 3)
					{
						At = Index - 1;
					}
				}
				if (At < 0 || Whole < S.MinLength)
				{
					continue;
				}
				const float X = Lanes.X(Other, At), Y = Lanes.Y(Other, At);
				const float Straight = Length(X - PlayerX, Y - PlayerY);
				if (Straight < Nearest || Straight > Farthest)
				{
					continue;
				}
				if (Accept(Other, At))
				{
					Out.Lane = Other;
					Out.Point = At;
					Out.Joins = Taken;
					Out.Straight = Straight;
					Out.Along = Along;
					return true;
				}
				if (++Looks >= MaxLooks)
				{
					return false;
				}
			}
			// On along his way.
			for (int Index = 0; Index + 1 < Lanes.NumPoints(Taken); ++Index)
			{
				Along += Length(Lanes.X(Taken, Index + 1) - Lanes.X(Taken, Index), Lanes.Y(Taken, Index + 1) - Lanes.Y(Taken, Index));
			}
			Lane = Taken;
		}
		return false;
	}

	/** Why a car that waits pulls out, or that it still waits. */
	enum class EJoin
	{
		Waits,
		/** He is about to pass. */
		Comes,
		/** He was nearer and goes away: he turned off, or he is past. */
		Passed,
		/** It has waited long enough. */
		Late,
		/** He stands still: he gave up, or his car is stuck (gate 103: stopped for 41 s, no police car came). */
		Stands,
		/** Nobody is wanted any more. */
		Over,
	};

	/**
	 * Does a car that waits pull out now?
	 * AwayM: how far he is in a straight line; NearestM: the nearest he has been since it waits; QuarryKmh: his speed;
	 * WaitedS: how long it has waited; Stars: the level; StoodS: how long he has been slower than S.StandKmh.
	 */
	inline EJoin Joins(float AwayM, float NearestM, float QuarryKmh, float WaitedS, int Stars, const FSettings& S, float StoodS = 0.f)
	{
		if (Stars <= 0)
		{
			return EJoin::Over;
		}
		if (StoodS >= S.StandS)
		{
			return EJoin::Stands;
		}
		const float Lead = S.JoinSeconds * (QuarryKmh > 0.f ? QuarryKmh : 0.f) / 3.6f;
		if (AwayM <= (Lead > S.JoinMinM ? Lead : S.JoinMinM))
		{
			return EJoin::Comes;
		}
		if (AwayM > NearestM + S.PassedM)
		{
			return EJoin::Passed;
		}
		return WaitedS >= S.MaxWaitS ? EJoin::Late : EJoin::Waits;
	}

	inline const char* JoinName(EJoin Why)
	{
		static const char* const Names[] = {"waits", "he is about to pass", "he goes away from it", "it has waited long enough", "he stands still", "nobody is wanted any more"};
		return Names[static_cast<int>(Why)];
	}

	/** When a unit left behind is taken away for another to be made ahead of him (LeftBehind). */
	struct FLeftBehind
	{
		/** From this level on. */
		int FromStars = 2;
		/** Farther than this from him (m) for this long (s), while he drives faster than MinKmh. */
		float Metres = 180.f;
		float Seconds = 6.f;
		float MinKmh = 30.f;
		/** Not a unit younger than this (s): one made 200 m behind him is given the time to drive. */
		float MinAge = 12.f;
	};

	/** Is a unit left behind for LeftSeconds, made AgeSeconds ago, taken away now? Only while Others more drive after him and none waits. */
	inline bool TakenAway(float LeftSeconds, float AgeSeconds, int Others, int Waiting, const FLeftBehind& L)
	{
		return LeftSeconds >= L.Seconds && AgeSeconds >= L.MinAge && Others >= 1 && Waiting == 0;
	}

	/**
	 * The seconds a unit has been left behind, after one more frame: counted while it is farther than L.Metres from a
	 * player who drives, from L.FromStars on; back to nothing as soon as it is not. Past L.Seconds the unit is taken
	 * away out of his sight and another is made, as past the leash.
	 */
	inline float LeftBehindSeconds(float Seconds, float Dt, float AwayM, float QuarryKmh, int Stars, const FLeftBehind& L)
	{
		return Stars >= L.FromStars && AwayM > L.Metres && QuarryKmh >= L.MinKmh ? Seconds + Dt : 0.f;
	}
}
