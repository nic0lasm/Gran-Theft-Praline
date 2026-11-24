// Checks the roads a flight takes (GTPRoadWidth.h): the carriageway around a lane, the lanes that never end, the worth
// of the way on from an exit, the exit taken at a junction, the speed on a road and the lanes a car may be put on.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/road_width_test.cpp -o /tmp/road_width_test && /tmp/road_width_test
// With a lanes file (tools/city/traffic.py, as the game reads it), it also says what a flight finds there, with the
// game's own rules (GTPRoadWidth.h, GTPDriveLogic.h): the line "traffic: lanes for a flight" of the game, then a flight
// of 2500 m replayed from the lane nearest a point (Unreal cm, default the carjack of -GTARun on the quai Jean Moulin),
// with the junction choice until gate 75 and the one of now. "all" in place of the point: one flight from every lane
// that never ends, added up. Lanes a fixed obstacle blocks can be given, and the times a flight drives onto one are
// counted: known from the start, or with "learn=" met first, as in the game (AGTPTrafficNetwork::MarkLaneBlocked marks
// the lane when the car is held up there and the car is put elsewhere; here the flight goes on from that lane's end):
//   /tmp/road_width_test <Map>.lanes.csv [x_cm,y_cm|all] [[learn=]blocked lanes, comma separated]
#include "GTPDriveLogic.h"
#include "GTPRoadWidth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

namespace
{
	using namespace GTPRoadWidth;

	/** A straight lane from (Ax, Ay) to (Bx, By), a point every 4 m as tools/city/traffic.py writes them. */
	std::vector<FPoint> Line(float Ax, float Ay, float Bx, float By)
	{
		const float Length = std::hypot(Bx - Ax, By - Ay);
		const int Steps = static_cast<int>(std::ceil(Length / 4.f));
		std::vector<FPoint> Out;
		for (int Step = 0; Step <= Steps; ++Step)
		{
			const float T = Steps > 0 ? static_cast<float>(Step) / static_cast<float>(Steps) : 0.f;
			Out.push_back({Ax + (Bx - Ax) * T, Ay + (By - Ay) * T});
		}
		return Out;
	}

	bool Near(float A, float B) { return std::fabs(A - B) < 0.01f; }

	/** One lane of a lanes file: lane_id,road_id,"name",speed_kmh,width_m,points_ue_cm,next[,...]. Points in metres. */
	struct FFileLane
	{
		std::string Road;
		std::string Name;
		float Width = 0.f;
		std::vector<FPoint> Points;
		std::vector<int> Next;
		float Length = 0.f;
	};

	std::vector<std::string> SplitCsv(const std::string& Row)
	{
		std::vector<std::string> Fields(1);
		bool bQuoted = false;
		for (const char Char : Row)
		{
			if (Char == '"')
			{
				bQuoted = !bQuoted;
			}
			else if (Char == ',' && !bQuoted)
			{
				Fields.emplace_back();
			}
			else if (Char != '\r')
			{
				Fields.back().push_back(Char);
			}
		}
		return Fields;
	}

	std::vector<FFileLane> ReadLanes(const char* Path)
	{
		std::vector<FFileLane> Lanes;
		std::ifstream File(Path);
		std::string Row;
		std::getline(File, Row);
		while (std::getline(File, Row))
		{
			const std::vector<std::string> Fields = SplitCsv(Row);
			if (Fields.size() < 7)
			{
				continue;
			}
			FFileLane Lane;
			Lane.Road = Fields[1];
			Lane.Name = Fields[2];
			Lane.Width = std::strtof(Fields[4].c_str(), nullptr);
			std::stringstream PointsIn(Fields[5]);
			std::string Pair;
			while (std::getline(PointsIn, Pair, ';'))
			{
				float X = 0.f, Y = 0.f;
				if (std::sscanf(Pair.c_str(), "%f %f", &X, &Y) == 2)
				{
					Lane.Points.push_back({X / 100.f, Y / 100.f});
				}
			}
			std::stringstream NextIn(Fields[6]);
			int Next = 0;
			while (NextIn >> Next)
			{
				Lane.Next.push_back(Next);
			}
			for (std::size_t Index = 1; Index < Lane.Points.size(); ++Index)
			{
				Lane.Length += std::hypot(Lane.Points[Index].X - Lane.Points[Index - 1].X, Lane.Points[Index].Y - Lane.Points[Index - 1].Y);
			}
			Lanes.push_back(Lane);
		}
		for (FFileLane& Lane : Lanes)
		{
			std::vector<int> Kept;
			for (const int Next : Lane.Next)
			{
				if (Next >= 0 && static_cast<std::size_t>(Next) < Lanes.size())
				{
					Kept.push_back(Next);
				}
			}
			Lane.Next = Kept;
		}
		return Lanes;
	}

	/** One flight replayed on a lanes file: metres by width class, the lanes it took, how it ended. */
	struct FFlight
	{
		float Driven = 0.f;
		float ByWidth[4] = {0.f, 0.f, 0.f, 0.f};
		int ObstaclesMet = 0;
		int MostNodes = 0;
		bool bDeadEnd = false;
		std::string Names;
	};

	/**
	 * A flight of 2500 m from Start. Rule 0: the junction choice until gate 75 (open road 250 m, 6 junctions deep, then the
	 * straightest). Rule 1: the choice of now (ExitScore over WayQuality, the last RecentCount lanes driven counted for half).
	 * Obstacles: lanes a fixed obstacle blocks; with bLearn the flight knows one only once it has met it (rule 1 then keeps
	 * off it), otherwise from the start.
	 */
	FFlight Fly(const std::vector<FFileLane>& Lanes, const std::vector<float>& Carriageway, const std::vector<bool>& Obstacles,
		bool bLearn, int Start, int Rule)
	{
		const auto NextOf = [&Lanes](int Lane) -> const std::vector<int>& { return Lanes[static_cast<std::size_t>(Lane)].Next; };
		std::vector<bool> Blocked = bLearn ? std::vector<bool>(Obstacles.size(), false) : Obstacles;
		const auto FindAlive = [&Lanes, &NextOf, &Blocked]()
		{
			return NeverEnding(static_cast<int>(Lanes.size()), NextOf, [&Blocked](int Lane) { return Blocked[static_cast<std::size_t>(Lane)]; });
		};
		std::vector<bool> Alive = FindAlive();
		const auto LengthOf = [&Lanes](int Lane) { return Lanes[static_cast<std::size_t>(Lane)].Length; };
		constexpr std::size_t RecentCount = 25;
		std::vector<int> Driven;
		const auto WeightOf = [&Carriageway, &Blocked, &Driven](int Lane)
		{
			const std::size_t At = static_cast<std::size_t>(Lane);
			if (Blocked[At])
			{
				return 0.f;
			}
			const std::size_t From = Driven.size() > RecentCount ? Driven.size() - RecentCount : 0;
			const bool bRecent = std::find(Driven.begin() + static_cast<std::ptrdiff_t>(From), Driven.end(), Lane) != Driven.end();
			return MetreWeight(Carriageway[At]) * (bRecent ? 0.5f : 1.f);
		};
		FFlight Out;
		std::string LastName = "?";
		int Lane = Start;
		while (Out.Driven < 2500.f)
		{
			const FFileLane& Here = Lanes[static_cast<std::size_t>(Lane)];
			Driven.push_back(Lane);
			if (Obstacles[static_cast<std::size_t>(Lane)])
			{
				++Out.ObstaclesMet;
				if (Rule == 1 && !Blocked[static_cast<std::size_t>(Lane)])
				{
					Blocked[static_cast<std::size_t>(Lane)] = true;
					Alive = FindAlive();
				}
			}
			Out.Driven += Here.Length;
			Out.ByWidth[static_cast<int>(Classify(Carriageway[static_cast<std::size_t>(Lane)]))] += Here.Length;
			const std::string Name = Here.Name.empty() ? "-" : Here.Name;
			if (Name != LastName)
			{
				Out.Names += (Out.Names.empty() ? "" : " > ") + Name;
				LastName = Name;
			}
			if (Here.Next.empty() || Here.Points.size() < 2)
			{
				Out.bDeadEnd = true;
				break;
			}
			const FPoint A = Here.Points[Here.Points.size() - 2], B = Here.Points.back();
			const float HeadingLength = std::hypot(B.X - A.X, B.Y - A.Y);
			int Best = -1;
			float BestScore = -1.e10f;
			for (const int Candidate : Here.Next)
			{
				const std::size_t At = static_cast<std::size_t>(Candidate);
				const std::vector<FPoint>& To = Lanes[At].Points;
				if (To.size() < 2)
				{
					continue;
				}
				const float Dx = To[1].X - To[0].X, Dy = To[1].Y - To[0].Y;
				const float Dot = ((B.X - A.X) * Dx + (B.Y - A.Y) * Dy) / std::max(HeadingLength * std::hypot(Dx, Dy), 1.e-6f);
				float Score = 0.f;
				if (Rule == 0)
				{
					Score = GTPDrive::OpenRoad(Candidate, 6, 250.f, NextOf, LengthOf) * 10.f + Dot;
				}
				else
				{
					int NodesLeft = LookNodes;
					const float Quality = WayQuality(Candidate, LookDepth, LookMetres, NextOf, LengthOf, WeightOf, NodesLeft);
					Out.MostNodes = std::max(Out.MostNodes, LookNodes - NodesLeft);
					Score = ExitScore(Blocked[At], Alive[At], Quality, Dot);
				}
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = Candidate;
				}
			}
			if (Best < 0)
			{
				Out.bDeadEnd = true;
				break;
			}
			Lane = Best;
		}
		return Out;
	}

	/** What a flight finds on a lanes file: the game's line, then flights replayed with the old and the new choice. */
	int Survey(const char* Path, bool bAll, float StartX, float StartY, const std::vector<int>& BlockedLanes, bool bLearn)
	{
		const std::vector<FFileLane> Lanes = ReadLanes(Path);
		if (Lanes.empty())
		{
			std::printf("no lane in %s\n", Path);
			return 1;
		}
		const int Count = static_cast<int>(Lanes.size());
		std::map<std::string, int> RoadKeyOf;
		std::vector<int> RoadKeys;
		std::vector<float> Widths;
		std::vector<std::vector<FPoint>> Points;
		for (const FFileLane& Lane : Lanes)
		{
			RoadKeys.push_back(Lane.Road.empty() ? -1 : RoadKeyOf.emplace(Lane.Road, static_cast<int>(RoadKeyOf.size())).first->second);
			Widths.push_back(Lane.Width);
			Points.push_back(Lane.Points);
		}
		std::vector<bool> Blocked(Lanes.size(), false);
		for (const int Lane : BlockedLanes)
		{
			if (Lane >= 0 && Lane < Count)
			{
				Blocked[static_cast<std::size_t>(Lane)] = true;
			}
		}
		const std::vector<float> Carriageway = Carriageways(RoadKeys, Widths, Points);
		const auto NextOf = [&Lanes](int Lane) -> const std::vector<int>& { return Lanes[static_cast<std::size_t>(Lane)].Next; };
		const std::vector<bool> Alive = NeverEnding(Count, NextOf, [&Blocked, bLearn](int Lane) { return !bLearn && Blocked[static_cast<std::size_t>(Lane)]; });
		int NeverEnd = 0;
		int ByClass[4] = {0, 0, 0, 0};
		for (std::size_t Lane = 0; Lane < Lanes.size(); ++Lane)
		{
			NeverEnd += Alive[Lane] ? 1 : 0;
			++ByClass[static_cast<int>(Classify(Carriageway[Lane]))];
		}
		std::printf("traffic: lanes for a flight: %d of %d never end (no dead end ahead); carriageway: %d main axis (%.0f m and over), %d middle, %d narrow (under %.0f m), %d unknown\n",
			NeverEnd, Count, ByClass[static_cast<int>(EClass::Main)], static_cast<double>(MainMetres), ByClass[static_cast<int>(EClass::Middle)],
			ByClass[static_cast<int>(EClass::Narrow)], static_cast<double>(NarrowMetres), ByClass[static_cast<int>(EClass::Unknown)]);
		std::printf("lanes a fixed obstacle blocks: %d, %s\n", static_cast<int>(std::count(Blocked.begin(), Blocked.end(), true)),
			bLearn ? "met first (the game learns them)" : "known from the start");
		std::vector<int> Starts;
		if (bAll)
		{
			for (int Lane = 0; Lane < Count; ++Lane)
			{
				if (Alive[static_cast<std::size_t>(Lane)])
				{
					Starts.push_back(Lane);
				}
			}
		}
		else
		{
			// The lane nearest the start point.
			int Start = 0;
			float StartDistance = 1.e9f;
			for (std::size_t Lane = 0; Lane < Lanes.size(); ++Lane)
			{
				for (const FPoint& Point : Lanes[Lane].Points)
				{
					const float Distance = std::hypot(Point.X - StartX / 100.f, Point.Y - StartY / 100.f);
					if (Distance < StartDistance && !Lanes[Lane].Next.empty())
					{
						StartDistance = Distance;
						Start = static_cast<int>(Lane);
					}
				}
			}
			std::printf("flight from lane %d (%s), %.0f m from the start point\n", Start, Lanes[static_cast<std::size_t>(Start)].Name.c_str(), static_cast<double>(StartDistance));
			Starts.push_back(Start);
		}
		for (int Rule = 0; Rule < 2; ++Rule)
		{
			FFlight Sum;
			int DeadEnds = 0;
			for (const int Start : Starts)
			{
				const FFlight One = Fly(Lanes, Carriageway, Blocked, bLearn, Start, Rule);
				Sum.Driven += One.Driven;
				for (int Class = 0; Class < 4; ++Class)
				{
					Sum.ByWidth[Class] += One.ByWidth[Class];
				}
				Sum.ObstaclesMet += One.ObstaclesMet;
				Sum.MostNodes = std::max(Sum.MostNodes, One.MostNodes);
				DeadEnds += One.bDeadEnd ? 1 : 0;
				Sum.Names = One.Names;
			}
			const float Total = std::max(Sum.ByWidth[0] + Sum.ByWidth[1] + Sum.ByWidth[2] + Sum.ByWidth[3], 1.f);
			std::printf("%s: %d flight(s), %.0f m, %d dead end(s), %d obstacle(s) met, at most %d lanes looked at for an exit; main axis %.0f %%, middle %.0f %%, narrow %.0f %%, unknown %.0f %%\n",
				Rule == 0 ? "until gate 75" : "now", static_cast<int>(Starts.size()), static_cast<double>(Sum.Driven), DeadEnds, Sum.ObstaclesMet, Sum.MostNodes,
				static_cast<double>(100.f * Sum.ByWidth[static_cast<int>(EClass::Main)] / Total), static_cast<double>(100.f * Sum.ByWidth[static_cast<int>(EClass::Middle)] / Total),
				static_cast<double>(100.f * Sum.ByWidth[static_cast<int>(EClass::Narrow)] / Total), static_cast<double>(100.f * Sum.ByWidth[static_cast<int>(EClass::Unknown)] / Total));
			if (!bAll)
			{
				std::printf("  %s\n", Sum.Names.c_str());
			}
		}
		return 0;
	}

	/** Index of the highest score (the exit a flight takes). */
	int Best(const std::vector<float>& Scores)
	{
		int Out = -1;
		for (std::size_t Index = 0; Index < Scores.size(); ++Index)
		{
			if (Out < 0 || Scores[Index] > Scores[static_cast<std::size_t>(Out)])
			{
				Out = static_cast<int>(Index);
			}
		}
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	using namespace GTPRoadWidth;
	{
		// The classes: 7 m and over a main axis, under 5 m a narrow street, nothing known for 0.
		CHECK(Classify(0.f) == EClass::Unknown);
		CHECK(Classify(4.9f) == EClass::Narrow);
		CHECK(Classify(5.f) == EClass::Middle);
		CHECK(Classify(6.9f) == EClass::Middle);
		CHECK(Classify(7.f) == EClass::Main);
		CHECK(Classify(12.f) == EClass::Main);
	}
	{
		// Carriageways. Road 0: a two-way street 7 m wide, one lane of 3.5 m each way, 3 m apart: 7 m for both.
		// Road 1: a one-way street of 4 m (a narrow street, whatever the width of its single lane).
		// Road 2: two pieces of the same two-way road 300 m apart, 2.5 m lanes: 5 m each, not 10.
		// Road -1 or no width: unknown.
		const std::vector<int> Roads = {0, 0, 1, 2, 2, 2, 2, -1, 1};
		const std::vector<float> Widths = {3.5f, 3.5f, 4.f, 2.5f, 2.5f, 2.5f, 2.5f, 3.f, 0.f};
		const std::vector<std::vector<FPoint>> Points = {
			Line(0.f, 0.f, 100.f, 0.f), Line(100.f, 3.f, 0.f, 3.f),
			Line(0.f, 50.f, 80.f, 50.f),
			Line(0.f, 200.f, 60.f, 200.f), Line(60.f, 203.f, 0.f, 203.f),
			Line(360.f, 200.f, 420.f, 200.f), Line(420.f, 203.f, 360.f, 203.f),
			Line(0.f, 400.f, 50.f, 400.f),
			Line(0.f, 500.f, 50.f, 500.f),
		};
		const std::vector<float> Out = Carriageways(Roads, Widths, Points);
		CHECK(Out.size() == Points.size());
		CHECK(Near(Out[0], 7.f) && Near(Out[1], 7.f));
		CHECK(Classify(Out[0]) == EClass::Main);
		CHECK(Near(Out[2], 4.f) && Classify(Out[2]) == EClass::Narrow);
		CHECK(Near(Out[3], 5.f) && Near(Out[4], 5.f) && Near(Out[5], 5.f) && Near(Out[6], 5.f));
		CHECK(Near(Out[7], 0.f) && Near(Out[8], 0.f));
		// A file read wrong (columns missing): nothing known, nothing made up.
		CHECK((Carriageways({0}, {}, Points) == std::vector<float>(Points.size(), 0.f)));
	}
	{
		// Lanes that never end. 0 -> 1 -> 2 -> 0 is a loop; 3 leads into it; 4 -> 5 ends; 6 leads to 4 and to the
		// loop; 7 leads only to lanes that end; 8 leads to a lane that is not there.
		const std::vector<std::vector<int>> Next = {{1}, {2}, {0}, {0}, {5}, {}, {4, 1}, {4, 5}, {42}};
		const std::vector<bool> Alive = NeverEnding(static_cast<int>(Next.size()),
			[&Next](int Lane) -> const std::vector<int>& { return Next[static_cast<std::size_t>(Lane)]; });
		CHECK((Alive == std::vector<bool>{true, true, true, true, false, false, true, false, false}));
		CHECK(NeverEnding(0, [&Next](int Lane) -> const std::vector<int>& { return Next[static_cast<std::size_t>(Lane)]; }).empty());
	}
	{
		// A lane a fixed obstacle blocks ends, and so do the lanes that only lead to it: the loop 0 -> 1 -> 2 -> 0 with 1
		// blocked, 3 that leads into it; 4 -> 5 -> 4 goes on.
		const std::vector<std::vector<int>> Next = {{1}, {2}, {0}, {0}, {5}, {4}};
		const std::vector<bool> Alive = NeverEnding(static_cast<int>(Next.size()),
			[&Next](int Lane) -> const std::vector<int>& { return Next[static_cast<std::size_t>(Lane)]; }, [](int Lane) { return Lane == 1; });
		CHECK((Alive == std::vector<bool>{false, false, false, false, true, true}));
	}
	{
		// The worth of the way on. 0 forks: 1 a narrow street of 100 m into 3, a main axis of 400 m; 2 a main axis of
		// 100 m into 4, a narrow street of 400 m. Width-weighted over 500 m: 1 is worth 25 + 400, 2 is worth 100 + 100.
		const std::vector<std::vector<int>> Next = {{1, 2}, {3}, {4}, {3}, {4}, {}, {5}};
		const std::vector<float> Length = {50.f, 100.f, 100.f, 400.f, 400.f, 30.f, 600.f};
		const std::vector<float> Width = {7.f, 4.f, 9.f, 9.f, 4.f, 9.f, 9.f};
		std::vector<bool> Blocked(Next.size(), false);
		std::vector<bool> Recent(Next.size(), false);
		const auto NextOf = [&Next](int Lane) -> const std::vector<int>& { return Next[static_cast<std::size_t>(Lane)]; };
		const auto LengthOf = [&Length](int Lane) { return Length[static_cast<std::size_t>(Lane)]; };
		const auto WeightOf = [&Width, &Blocked, &Recent](int Lane)
		{
			const std::size_t At = static_cast<std::size_t>(Lane);
			return Blocked[At] ? 0.f : MetreWeight(Width[At]) * (Recent[At] ? 0.5f : 1.f);
		};
		const auto Quality = [&](int Lane)
		{
			int Nodes = LookNodes;
			return WayQuality(Lane, LookDepth, LookMetres, NextOf, LengthOf, WeightOf, Nodes);
		};
		CHECK(Near(Quality(1), 425.f));
		CHECK(Near(Quality(2), 200.f));
		// The narrow street that leads to a main axis, not the main axis that leads to a narrow street.
		CHECK(Best({ExitScore(false, true, Quality(1), -0.5f), ExitScore(false, true, Quality(2), 1.f)}) == 0);
		// A dead end is worth what it drives (5: 30 m); a long lane only up to the budget (6: 500 of 600 m).
		CHECK(Near(Quality(5), 30.f));
		CHECK(Near(Quality(6), 500.f));
		// Lanes driven lately count for half: 3 driven, 1 is worth 25 + 200.
		Recent[3] = true;
		CHECK(Near(Quality(1), 225.f));
		Recent[3] = false;
		// A fixed obstacle on 3 stops the way there: 1 is worth its 25 m.
		Blocked[3] = true;
		CHECK(Near(Quality(1), 25.f));
		CHECK(Near(Quality(3), 0.f));
		Blocked[3] = false;
		// The lanes looked at are bounded: none when the bound is spent, and a dense network ends.
		int None = 0;
		CHECK(Near(WayQuality(1, LookDepth, LookMetres, NextOf, LengthOf, WeightOf, None), 0.f));
		const std::vector<std::vector<int>> Dense(64, std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8});
		int Nodes = LookNodes;
		const float DenseWorth = WayQuality(0, LookDepth, LookMetres,
			[&Dense](int Lane) -> const std::vector<int>& { return Dense[static_cast<std::size_t>(Lane)]; }, [](int) { return 1.f; }, [](int) { return 1.f; }, Nodes);
		CHECK(Nodes == 0 && DenseWorth > 0.f && DenseWorth <= 11.f);
	}
	{
		// The exit taken: never a blocked exit when another exists, even a dead end; an exit that never ends before the
		// best way on; then the best way on; then the straightest.
		CHECK(Best({ExitScore(true, true, 500.f, 1.f), ExitScore(false, false, 10.f, -1.f)}) == 1);
		CHECK(Best({ExitScore(false, false, 500.f, 1.f), ExitScore(false, true, 10.f, -1.f)}) == 1);
		CHECK(Best({ExitScore(false, true, 300.f, 1.f), ExitScore(false, true, 301.f, -1.f)}) == 1);
		CHECK(Best({ExitScore(false, true, 300.f, -0.2f), ExitScore(false, true, 300.f, 0.9f)}) == 1);
		CHECK(Best({ExitScore(false, false, 120.f, 1.f), ExitScore(false, false, 80.f, 1.f)}) == 0);
		// Gate 75: the narrow street straight on that goes on for 500 m, the main axis to the right that goes on: the
		// main axis.
		CHECK(Best({ExitScore(false, true, 500.f * MetreWeight(4.f), 1.f), ExitScore(false, true, 500.f * MetreWeight(9.f), 0.f)}) == 1);
	}
	{
		// Speeds: 70 on a main axis, 60 in a middle street or unknown, 45 in a narrow street.
		CHECK(Near(FlightKmh(12.f), 70.f));
		CHECK(Near(FlightKmh(6.f), 60.f));
		CHECK(Near(FlightKmh(0.f), 60.f));
		CHECK(Near(FlightKmh(3.f), 45.f));
	}
	{
		// Where a car may be put, pass after pass; never on a blocked lane.
		CHECK(RelocationFits(0, false, true, 250.f, 250.f, 9.f));
		CHECK(RelocationFits(0, false, true, 250.f, 250.f, 0.f));
		CHECK(!RelocationFits(0, false, true, 250.f, 250.f, 6.f));
		CHECK(!RelocationFits(0, false, false, 250.f, 250.f, 9.f));
		CHECK(RelocationFits(1, false, true, 250.f, 250.f, 6.f));
		CHECK(!RelocationFits(1, false, true, 250.f, 250.f, 4.f));
		CHECK(RelocationFits(2, false, true, 10.f, 250.f, 4.f));
		CHECK(!RelocationFits(2, false, false, 250.f, 250.f, 9.f));
		CHECK(RelocationFits(3, false, false, 250.f, 250.f, 4.f));
		CHECK(!RelocationFits(3, false, false, 200.f, 250.f, 9.f));
		CHECK(RelocationFits(RelocationPasses - 1, false, false, 0.f, 250.f, 0.f));
		for (int Pass = 0; Pass < RelocationPasses; ++Pass)
		{
			CHECK(!RelocationFits(Pass, true, true, 250.f, 250.f, 9.f));
		}
	}
	std::printf(Failures ? "FAILED (%d)\n" : "road_width_test: ok\n", Failures);
	if (Failures == 0 && Argc >= 2)
	{
		// The carjack of -GTARun on the quai Jean Moulin (gate 75's log), unless a point is given.
		float X = 38847.f, Y = -5027.f;
		const bool bAll = Argc >= 3 && std::string(Argv[2]) == "all";
		if (Argc >= 3 && !bAll && std::sscanf(Argv[2], "%f,%f", &X, &Y) != 2)
		{
			std::printf("start point: x_cm,y_cm or all\n");
			return 2;
		}
		std::vector<int> BlockedLanes;
		std::string List = Argc >= 4 ? Argv[3] : "";
		const bool bLearn = List.rfind("learn=", 0) == 0;
		if (bLearn)
		{
			List = List.substr(6);
		}
		if (!List.empty())
		{
			std::stringstream In(List);
			std::string Item;
			while (std::getline(In, Item, ','))
			{
				BlockedLanes.push_back(std::atoi(Item.c_str()));
			}
		}
		return Survey(Argv[1], bAll, X, Y, BlockedLanes, bLearn);
	}
	return Failures ? 1 : 0;
}
