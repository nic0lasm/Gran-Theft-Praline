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
//   meeting a dead end at the edge of the map or a lane a fixed obstacle blocks;
// - the worth of the way on from an exit (its metres weighted by width, 500 m ahead), the score of an exit at a junction,
//   the speed of a flight on a road, the lanes a car may be put on.
// In the filmed flight of gate 75 fixed obstacles of the map stood on five lanes (a white block on the rue de la Bourse,
// the way from the quai Jean Moulin to the rue Gentil, the rue Claudia, the rue Antoine Salles): the game marks a lane
// blocked when a flight is held up there (AGTPTrafficNetwork::MarkLaneBlocked) and keeps the flights off it.

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
	 * ends, and so does a lane a fixed obstacle blocks (IsBlocked); so does a lane whose exits all end, and so on until
	 * nothing changes. NextOf(Lane): the lanes it leads to (out-of-range ones are skipped).
	 */
	template <typename FNextOf, typename FIsBlocked>
	std::vector<bool> NeverEnding(int LaneCount, const FNextOf& NextOf, const FIsBlocked& IsBlocked)
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
			if (AliveExits[Lane] == 0 || IsBlocked(static_cast<int>(Lane)))
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

	template <typename FNextOf>
	std::vector<bool> NeverEnding(int LaneCount, const FNextOf& NextOf)
	{
		return NeverEnding(LaneCount, NextOf, [](int) { return false; });
	}

	/** Weight of a metre of road by its width (WayQuality): a main axis 1, a middle street or an unknown width 0.7, a narrow street 0.25. */
	inline float MetreWeight(float CarriagewayMetres)
	{
		switch (Classify(CarriagewayMetres))
		{
		case EClass::Main: return 1.f;
		case EClass::Narrow: return 0.25f;
		default: return 0.7f;
		}
	}

	/** How far and how many junctions deep WayQuality looks (m, junctions), and the most lanes it visits for one exit. */
	constexpr float LookMetres = 500.f;
	constexpr int LookDepth = 10;
	constexpr int LookNodes = 20000;

	/**
	 * The best way on from Lane (Lane included) within Budget metres and Depth junctions, as width-weighted metres: the
	 * more of it on main axes, the more it is worth. WeightOf(Lane): the weight of a metre of that lane (MetreWeight, times
	 * a half for a lane driven lately so a flight does not circle one block); 0 for a lane a fixed obstacle blocks, which
	 * stops the way there. A way that ends at a dead end is worth only what it drove. NodesLeft bounds the lanes visited
	 * (a dense network): when it runs out, the lanes not yet seen count for nothing.
	 */
	template <typename FNextOf, typename FLengthOf, typename FWeightOf>
	float WayQuality(int Lane, int Depth, float Budget, const FNextOf& NextOf, const FLengthOf& LengthOf, const FWeightOf& WeightOf, int& NodesLeft)
	{
		if (NodesLeft <= 0)
		{
			return 0.f;
		}
		--NodesLeft;
		const float Weight = WeightOf(Lane);
		if (!(Weight > 0.f))
		{
			return 0.f;
		}
		const float Length = LengthOf(Lane);
		const float Own = (Length < Budget ? Length : Budget) * Weight;
		if (Length >= Budget || Depth <= 0)
		{
			return Own;
		}
		float Best = 0.f;
		for (const int Next : NextOf(Lane))
		{
			const float Value = WayQuality(Next, Depth - 1, Budget - Length, NextOf, LengthOf, WeightOf, NodesLeft);
			Best = Value > Best ? Value : Best;
		}
		return Own + Best;
	}

	/**
	 * Score of an exit at a junction for a flight (FGTPLaneFollower::bPreferStraight); the highest is taken.
	 * - Never an exit a fixed obstacle blocks when another exists.
	 * - An exit that never ends first: the flight does not turn into a dead end when another way exists.
	 * - Then the best way on (QualityMetres, WayQuality): on the 235 lanes of L_City_RueDeLaRepublique, from the carjack
	 *   of -GTARun, 72 % of 2.5 km on main axes and 8 % in narrow streets (49 % and none until gate 75, but round the
	 *   block of the rue Gentil whose way in a fixed obstacle stops: 15 times in 2.5 km).
	 * - Then the straightest (HeadingDot, -1 to 1): 10 points a weighted metre against 1 for a full change of heading.
	 */
	inline float ExitScore(bool bBlocked, bool bNeverEnds, float QualityMetres, float HeadingDot)
	{
		return (bBlocked ? -1.e9f : 0.f) + (bNeverEnds ? 1.e6f : 0.f) + QualityMetres * 10.f + HeadingDot;
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
	 * May a flight be put on a lane at this pass (GTPDemoRoute::RelocateChase)? Never on a lane a fixed obstacle blocks.
	 * Pass 0: a main axis that never ends (an unknown width counts as one: on a network made by hand the first pass takes
	 * any lane that never ends). Pass 1: a middle street that never ends. Pass 2: any lane that never ends. Pass 3: a lane
	 * with OpenWanted metres of open road (the first pass until gate 75). Pass 4: any lane (the caller has kept those that
	 * go on).
	 */
	inline bool RelocationFits(int Pass, bool bBlocked, bool bNeverEnds, float OpenMetres, float OpenWanted, float CarriagewayMetres)
	{
		if (bBlocked)
		{
			return false;
		}
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
