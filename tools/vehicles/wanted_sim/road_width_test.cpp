// Checks the roads a flight takes (GTPRoadWidth.h): the carriageway around a lane, the lanes that never end, the exit
// taken at a junction, the speed on a road and the lanes a car may be put on.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/road_width_test.cpp -o /tmp/road_width_test && /tmp/road_width_test
// With a lanes file (tools/city/traffic.py, as the game reads it), it also says what a flight finds there, with the
// game's own rules (GTPRoadWidth.h, GTPDriveLogic.h): the line "traffic: lanes for a flight" of the game, then a flight
// of 2500 m replayed from the lane nearest a point (Unreal cm, default the carjack of -GTARun on the quai Jean Moulin),
// with the junction choice until gate 75 and the one of now:
//   /tmp/road_width_test <Map>.lanes.csv [x_cm,y_cm]
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

	/** What a flight finds on a lanes file: the game's line, then a flight replayed with the old and the new choice. */
	int Survey(const char* Path, float StartX, float StartY)
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
		const std::vector<float> Carriageway = Carriageways(RoadKeys, Widths, Points);
		const auto NextOf = [&Lanes](int Lane) -> const std::vector<int>& { return Lanes[static_cast<std::size_t>(Lane)].Next; };
		const auto LengthOf = [&Lanes](int Lane) { return Lanes[static_cast<std::size_t>(Lane)].Length; };
		const std::vector<bool> Alive = NeverEnding(Count, NextOf);
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
		for (int Rule = 0; Rule < 2; ++Rule)
		{
			int Lane = Start;
			float Driven = 0.f;
			float ByWidth[4] = {0.f, 0.f, 0.f, 0.f};
			std::string Names;
			std::string LastName = "?";
			const char* End = "goes on";
			while (Driven < 2500.f)
			{
				const FFileLane& Here = Lanes[static_cast<std::size_t>(Lane)];
				Driven += Here.Length;
				ByWidth[static_cast<int>(Classify(Carriageway[static_cast<std::size_t>(Lane)]))] += Here.Length;
				const std::string Name = Here.Name.empty() ? "-" : Here.Name;
				if (Name != LastName)
				{
					Names += (Names.empty() ? "" : " > ") + Name;
					LastName = Name;
				}
				if (Here.Next.empty() || Here.Points.size() < 2)
				{
					End = "DEAD END (the script puts the car elsewhere)";
					break;
				}
				const FPoint A = Here.Points[Here.Points.size() - 2], B = Here.Points.back();
				const float HeadingLength = std::hypot(B.X - A.X, B.Y - A.Y);
				int Best = -1;
				float BestScore = -1.e9f;
				for (const int Candidate : Here.Next)
				{
					const std::vector<FPoint>& To = Lanes[static_cast<std::size_t>(Candidate)].Points;
					if (To.size() < 2)
					{
						continue;
					}
					const float Dx = To[1].X - To[0].X, Dy = To[1].Y - To[0].Y;
					const float Dot = ((B.X - A.X) * Dx + (B.Y - A.Y) * Dy) / std::max(HeadingLength * std::hypot(Dx, Dy), 1.e-6f);
					const float Open = GTPDrive::OpenRoad(Candidate, 6, 250.f, NextOf, LengthOf);
					const float Score = Rule == 0 ? Open * 10.f + Dot
						: ExitScore(Alive[static_cast<std::size_t>(Candidate)], Open, Carriageway[static_cast<std::size_t>(Candidate)], Dot);
					if (Score > BestScore)
					{
						BestScore = Score;
						Best = Candidate;
					}
				}
				if (Best < 0)
				{
					End = "no exit";
					break;
				}
				Lane = Best;
			}
			const float Sum = std::max(ByWidth[0] + ByWidth[1] + ByWidth[2] + ByWidth[3], 1.f);
			std::printf("%s: %.0f m, %s; main axis %.0f %%, middle %.0f %%, narrow %.0f %%, unknown %.0f %%\n  %s\n",
				Rule == 0 ? "until gate 75" : "now", static_cast<double>(Driven), End,
				static_cast<double>(100.f * ByWidth[static_cast<int>(EClass::Main)] / Sum), static_cast<double>(100.f * ByWidth[static_cast<int>(EClass::Middle)] / Sum),
				static_cast<double>(100.f * ByWidth[static_cast<int>(EClass::Narrow)] / Sum), static_cast<double>(100.f * ByWidth[static_cast<int>(EClass::Unknown)] / Sum),
				Names.c_str());
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
		// Gate 75: at a junction, the narrow street straight on (it goes on), the main axis to the right (it goes on),
		// the main axis straight on that ends 60 m further at the edge of the map. The main axis to the right.
		const std::vector<float> Scores = {
			ExitScore(true, 250.f, 4.f, 1.f),
			ExitScore(true, 250.f, 9.f, 0.f),
			ExitScore(false, 60.f, 12.f, 1.f),
		};
		CHECK(Best(Scores) == 1);
		// The same main axis to the right, but it ends: the narrow street that goes on, not a dead end.
		CHECK(Best({ExitScore(true, 250.f, 4.f, 1.f), ExitScore(false, 250.f, 9.f, 0.f)}) == 0);
		// No exit goes on for ever: open road first, then the width once open for 80 m, then the straightest.
		CHECK(Best({ExitScore(false, 250.f, 4.f, 1.f), ExitScore(false, 120.f, 9.f, 0.f)}) == 1);
		CHECK(Best({ExitScore(false, 250.f, 4.f, 1.f), ExitScore(false, 70.f, 9.f, 1.f)}) == 0);
		// Two main axes that go on: the straightest.
		CHECK(Best({ExitScore(true, 250.f, 9.f, -0.1f), ExitScore(true, 250.f, 8.f, 0.9f)}) == 1);
		// A middle street that goes on rather than a narrow one; a main axis rather than a middle street.
		CHECK(Best({ExitScore(true, 250.f, 4.f, 1.f), ExitScore(true, 250.f, 6.f, 0.f)}) == 1);
		CHECK(Best({ExitScore(true, 250.f, 6.f, 1.f), ExitScore(true, 250.f, 7.f, 0.f)}) == 1);
		// Nothing known of the widths (a network made by hand): the old order among exits that end, open road then the
		// straightest; an exit that never ends before them.
		CHECK(Best({ExitScore(false, 250.f, 0.f, 0.f), ExitScore(false, 200.f, 0.f, 1.f)}) == 0);
		CHECK(Best({ExitScore(false, 250.f, 0.f, 0.f), ExitScore(false, 250.f, 0.f, 1.f)}) == 1);
		CHECK(Best({ExitScore(false, 79.f, 0.f, 1.f), ExitScore(false, 80.f, 0.f, -1.f)}) == 1);
		CHECK(Best({ExitScore(false, 250.f, 0.f, 1.f), ExitScore(true, 30.f, 0.f, -1.f)}) == 1);
	}
	{
		// Speeds: 70 on a main axis, 60 in a middle street or unknown, 45 in a narrow street.
		CHECK(Near(FlightKmh(12.f), 70.f));
		CHECK(Near(FlightKmh(6.f), 60.f));
		CHECK(Near(FlightKmh(0.f), 60.f));
		CHECK(Near(FlightKmh(3.f), 45.f));
	}
	{
		// Where a car may be put, pass after pass.
		CHECK(RelocationFits(0, true, 250.f, 250.f, 9.f));
		CHECK(RelocationFits(0, true, 250.f, 250.f, 0.f));
		CHECK(!RelocationFits(0, true, 250.f, 250.f, 6.f));
		CHECK(!RelocationFits(0, false, 250.f, 250.f, 9.f));
		CHECK(RelocationFits(1, true, 250.f, 250.f, 6.f));
		CHECK(!RelocationFits(1, true, 250.f, 250.f, 4.f));
		CHECK(RelocationFits(2, true, 10.f, 250.f, 4.f));
		CHECK(!RelocationFits(2, false, 250.f, 250.f, 9.f));
		CHECK(RelocationFits(3, false, 250.f, 250.f, 4.f));
		CHECK(!RelocationFits(3, false, 200.f, 250.f, 9.f));
		CHECK(RelocationFits(RelocationPasses - 1, false, 0.f, 250.f, 0.f));
	}
	std::printf(Failures ? "FAILED (%d)\n" : "road_width_test: ok\n", Failures);
	if (Failures == 0 && Argc >= 2)
	{
		// The carjack of -GTARun on the quai Jean Moulin (gate 75's log), unless a point is given.
		float X = 38847.f, Y = -5027.f;
		if (Argc >= 3 && std::sscanf(Argv[2], "%f,%f", &X, &Y) != 2)
		{
			std::printf("start point: x_cm,y_cm\n");
			return 2;
		}
		return Survey(Argv[1], X, Y);
	}
	return Failures ? 1 : 0;
}
