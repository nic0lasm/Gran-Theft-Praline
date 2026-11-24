#pragma once

// The roads a flight takes (the chase script of -GTARun and -PoliceChaseDemo, GTPDemoRoute.cpp), written without engine
// types so it can be tested outside Unreal (tools/vehicles/wanted_sim/road_width_test.cpp). Units: metres.
//
// Gate 75's filmed flight averaged 24 to 34 km/h. The script put the car on a narrow street at 29 s, the car was wedged
// between two walls there from 39 to 46 s, and the script put it elsewhere again. It knew nothing of the width of a street,
// chose the lane to put the car on at random, and only looked 250 m ahead for a dead end. Here:
// - the carriageway around a lane: the lanes file (tools/city/traffic.py) gives each lane its BD TOPO road and its share
//   of that road's width; the lanes of the same road near the middle of a lane add up (both directions, side by side).
//   On the quarter's 1043 lanes, 349 of the 401 lanes of BD TOPO importance 1 to 3 (quays, bridges, rue de la Barre) are
//   7 m wide or more, and 326 of the 642 lanes of importance 4 and 5 are under 5 m;
// - the lanes that never end: from them some way goes on for ever, round a block or along a loop of the network, without
//   meeting a dead end at the edge of the map;
// - the score of an exit at a junction, the speed of a flight on a road, the lanes a car may be put on.

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace GTPRoadWidth
{
	/** A carriageway this wide or more is a main axis (m). */
	constexpr float MainMetres = 7.f;
	/** A carriageway under this is a narrow street (m). */
	constexpr float NarrowMetres = 5.f;
	/** The lanes of a road add up for a lane when one of their points is this near the lane's middle point (m). */
	constexpr float SameRoadMetres = 20.f;

	enum class EClass : int
	{
		Unknown,
		Narrow,
		Middle,
		Main
	};

	inline EClass Classify(float CarriagewayMetres)
	{
		if (!(CarriagewayMetres > 0.f))
		{
			return EClass::Unknown;
		}
		return CarriagewayMetres >= MainMetres ? EClass::Main : (CarriagewayMetres >= NarrowMetres ? EClass::Middle : EClass::Narrow);
	}

	inline const char* ClassName(EClass Class)
	{
		switch (Class)
		{
		case EClass::Main: return "main axis";
		case EClass::Middle: return "middle street";
		case EClass::Narrow: return "narrow street";
		default: return "unknown width";
		}
	}

	struct FPoint
	{
		float X = 0.f;
		float Y = 0.f;
	};

	/**
	 * Width of the carriageway around each lane (m): its own width plus that of the other lanes of the same road (RoadKeys,
	 * -1 = none) with a point within SameRoadMetres of its middle point. 0 for a lane with no road or no width (a network
	 * made by hand: everything stays unknown).
	 */
	inline std::vector<float> Carriageways(const std::vector<int>& RoadKeys, const std::vector<float>& Widths, const std::vector<std::vector<FPoint>>& Points)
	{
		const std::size_t Count = Points.size();
		std::vector<float> Out(Count, 0.f);
		if (RoadKeys.size() != Count || Widths.size() != Count)
		{
			return Out;
		}
		std::unordered_map<int, std::vector<std::size_t>> ByRoad;
		for (std::size_t Lane = 0; Lane < Count; ++Lane)
		{
			if (RoadKeys[Lane] >= 0 && Widths[Lane] > 0.f && !Points[Lane].empty())
			{
				ByRoad[RoadKeys[Lane]].push_back(Lane);
			}
		}
		const float Near2 = SameRoadMetres * SameRoadMetres;
		for (const auto& Road : ByRoad)
		{
			for (const std::size_t Lane : Road.second)
			{
				const FPoint Middle = Points[Lane][Points[Lane].size() / 2];
				float Sum = 0.f;
				for (const std::size_t Other : Road.second)
				{
					bool bNear = Other == Lane;
					for (std::size_t Index = 0; !bNear && Index < Points[Other].size(); ++Index)
					{
						const float Dx = Points[Other][Index].X - Middle.X;
						const float Dy = Points[Other][Index].Y - Middle.Y;
						bNear = Dx * Dx + Dy * Dy <= Near2;
					}
					if (bNear)
					{
						Sum += Widths[Other];
					}
				}
				Out[Lane] = Sum;
			}
		}
		return Out;
	}

	/**
	 * The lanes that never end: from each, some way goes on for ever without meeting a dead end. A lane with no exit
	 * ends; so does a lane whose exits all end, and so on until nothing changes. NextOf(Lane): the lanes it leads to
	 * (out-of-range ones are skipped).
	 */
	template <typename FNextOf>
	std::vector<bool> NeverEnding(int LaneCount, const FNextOf& NextOf)
	{
		const std::size_t Count = LaneCount > 0 ? static_cast<std::size_t>(LaneCount) : 0;
		std::vector<bool> Alive(Count, true);
		// Exits still alive per lane, and the lanes that lead to each lane.
		std::vector<int> AliveExits(Count, 0);
		std::vector<std::vector<std::size_t>> LeadsHere(Count);
		std::vector<std::size_t> Ended;
		for (std::size_t Lane = 0; Lane < Count; ++Lane)
		{
			for (const int Next : NextOf(static_cast<int>(Lane)))
			{
				if (Next >= 0 && Next < LaneCount)
				{
					++AliveExits[Lane];
					LeadsHere[static_cast<std::size_t>(Next)].push_back(Lane);
				}
			}
			if (AliveExits[Lane] == 0)
			{
				Alive[Lane] = false;
				Ended.push_back(Lane);
			}
		}
		while (!Ended.empty())
		{
			const std::size_t Lane = Ended.back();
			Ended.pop_back();
			for (const std::size_t From : LeadsHere[Lane])
			{
				if (Alive[From] && --AliveExits[From] == 0)
				{
					Alive[From] = false;
					Ended.push_back(From);
				}
			}
		}
		return Alive;
	}

	/** Open road an exit that ends needs before its width counts (m): a main axis that ends within it is no way out. */
	constexpr float OpenForWidthMetres = 80.f;
	/** An exit that never ends is worth more than any open road ahead of one that ends (at most 2500 points). */
	constexpr float NeverEndingPoints = 100000.f;

	/** Points for the width of a road that is a way out: a main axis first, a narrow street last. */
	inline float WidthPoints(float CarriagewayMetres)
	{
		switch (Classify(CarriagewayMetres))
		{
		case EClass::Main: return 3000.f;
		case EClass::Narrow: return 0.f;
		default: return 1500.f;
		}
	}

	/**
	 * Score of an exit at a junction for a flight (FGTPLaneFollower::bPreferStraight); the highest is taken.
	 * - An exit that never ends first (100000 points), otherwise 10 points a metre of open road ahead (OpenMetres, capped by
	 *   the caller): the flight never turns into a dead end when another way exists.
	 * - Then the width of its road, once it is a way out (never ending, or open for 80 m): 3000 points for a main axis,
	 *   1500 for a middle street or an unknown width, none for a narrow street. Among the exits that never end, a main
	 *   axis, then a middle street, then a narrow one.
	 * - Then the straightest (HeadingDot, -1 to 1).
	 * With no width known (a network made by hand), the exits that never end come first, then the order is the old one
	 * (open road, then the straightest).
	 */
	inline float ExitScore(bool bNeverEnds, float OpenMetres, float CarriagewayMetres, float HeadingDot)
	{
		const bool bWayOut = bNeverEnds || OpenMetres >= OpenForWidthMetres;
		return (bNeverEnds ? NeverEndingPoints : OpenMetres * 10.f) + (bWayOut ? WidthPoints(CarriagewayMetres) : 0.f) + HeadingDot;
	}

	/**
	 * Speed of a flight on a clear road (km/h), a credible city flight: 70 on a main axis, 60 in a middle street or on a road
	 * of unknown width, 45 in a narrow street (gate 75: 72 km/h in a street 6 m wide between the facades, against a wall
	 * at the next bend). Bends, obstacles and the end of the known path lower it further (GTPDriveLogic.h).
	 */
	inline float FlightKmh(float CarriagewayMetres)
	{
		switch (Classify(CarriagewayMetres))
		{
		case EClass::Main: return 70.f;
		case EClass::Narrow: return 45.f;
		default: return 60.f;
		}
	}

	/** Passes of RelocationFits, tried in order until a lane fits. */
	constexpr int RelocationPasses = 5;

	/**
	 * May a flight be put on a lane at this pass (GTPDemoRoute::RelocateChase)? Pass 0: a main axis that never ends (an
	 * unknown width counts as one: on a network made by hand the first pass takes any lane that never ends). Pass 1: a
	 * middle street that never ends. Pass 2: any lane that never ends. Pass 3: a lane with OpenWanted metres of open road
	 * (the first pass until gate 75). Pass 4: any lane (the caller has kept those that go on).
	 */
	inline bool RelocationFits(int Pass, bool bNeverEnds, float OpenMetres, float OpenWanted, float CarriagewayMetres)
	{
		const EClass Class = Classify(CarriagewayMetres);
		switch (Pass)
		{
		case 0: return bNeverEnds && (Class == EClass::Main || Class == EClass::Unknown);
		case 1: return bNeverEnds && Class != EClass::Narrow;
		case 2: return bNeverEnds;
		case 3: return OpenMetres >= OpenWanted;
		default: return true;
		}
	}
}
