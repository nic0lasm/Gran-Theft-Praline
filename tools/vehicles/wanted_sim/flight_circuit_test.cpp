// Checks the circuit of a flight (GTPFlightCircuit.h): the rows of its file, the lane of each row, the lap, the way to
// the lap from the other lanes, the speed on its lanes.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/flight_circuit_test.cpp -o /tmp/flight_circuit_test && /tmp/flight_circuit_test
// With a lanes file (tools/city/traffic.py, as the game reads it) and a circuit file, it also says what the game will
// make of them (its line "traffic: flight circuit of ..."), then drives three laps with the game's own decisions
// (GTPDriveLogic.h, chase settings) and the lap's speeds: seconds a lap, mean speed of the first minute, top speed, how
// far the car strays from its lanes; on the lanes' own points and on the circuit's driving line, and last as the
// filmed flight does: from a standstill at the kerb, 3 m beside the approach, 40 km/h for 4 s. The car is the bicycle
// model of corner_drive_test.cpp, not the game's: it says how the decisions behave on this lap, not how the tyres do.
//   /tmp/flight_circuit_test <Map>.lanes.csv GranTheftPraline/Content/Test/Circuits/<Map>.circuit.csv
#include "GTPDriveLogic.h"
#include "GTPFlightCircuit.h"
#include "GTPRoadWidth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

namespace
{
	using namespace GTPFlightCircuit;

	struct FLane
	{
		std::string Road;
		float Width = 0.f;
		std::vector<GTPRoadWidth::FPoint> Points;
		std::vector<int> Next;
		float Length = 0.f;
		float Carriageway = 0.f;
	};

	FEnds EndsOf(const FLane& Lane)
	{
		return {Lane.Points.front().X, Lane.Points.front().Y, Lane.Points.back().X, Lane.Points.back().Y};
	}

	/** A straight lane, a point every 4 m as tools/city/traffic.py writes them. */
	FLane Straight(float Ax, float Ay, float Bx, float By, std::vector<int> Next)
	{
		FLane Lane;
		const float Length = std::hypot(Bx - Ax, By - Ay);
		const int Steps = static_cast<int>(std::ceil(Length / 4.f));
		for (int Step = 0; Step <= Steps; ++Step)
		{
			const float T = static_cast<float>(Step) / static_cast<float>(Steps);
			Lane.Points.push_back({Ax + (Bx - Ax) * T, Ay + (By - Ay) * T});
		}
		Lane.Next = std::move(Next);
		Lane.Length = Length;
		return Lane;
	}

	bool BuildOn(const std::vector<FLane>& Lanes, const std::vector<int>& Lap, FCircuit& Out, std::string& Error)
	{
		return Build(static_cast<int>(Lanes.size()), Lap,
			[&Lanes](int Lane) -> const std::vector<int>& { return Lanes[static_cast<std::size_t>(Lane)].Next; },
			[&Lanes](int Lane) { return Lanes[static_cast<std::size_t>(Lane)].Length; }, Out, Error);
	}

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

	/** A lanes file: lane_id,road_id,"name",speed_kmh,width_m,points_ue_cm,next[,...]. Points in metres. */
	std::vector<FLane> ReadLanes(const char* Path)
	{
		std::vector<FLane> Lanes;
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
			FLane Lane;
			Lane.Road = Fields[1];
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
		std::map<std::string, int> RoadKeyOf;
		std::vector<int> RoadKeys;
		std::vector<float> Widths;
		std::vector<std::vector<GTPRoadWidth::FPoint>> Points;
		for (const FLane& Lane : Lanes)
		{
			RoadKeys.push_back(Lane.Road.empty() ? -1 : RoadKeyOf.emplace(Lane.Road, static_cast<int>(RoadKeyOf.size())).first->second);
			Widths.push_back(Lane.Width);
			Points.push_back(Lane.Points);
		}
		const std::vector<float> Carriageways = GTPRoadWidth::Carriageways(RoadKeys, Widths, Points);
		for (std::size_t Index = 0; Index < Lanes.size(); ++Index)
		{
			Lanes[Index].Carriageway = Carriageways[Index];
		}
		return Lanes;
	}

	struct FDrive
	{
		float FirstMinuteKmh = 0.f;
		float PeakKmh = 0.f;
		float Stray = 0.f;
		int StrayLane = -1;
		float Seconds = 0.f;
		float LapSeconds[3] = {0.f, 0.f, 0.f};
		float Metres = 0.f;
	};

	/**
	 * Three laps with the game's decisions; BrakeMax and Drive: what the car's pedals give (m/s2). bFromTheft: as the
	 * filmed flight, from a standstill 3 m to the right of the approach's first point, 40 km/h for 4 s, the approach
	 * then the lap from the lane the approach leads to.
	 */
	FDrive DriveLaps(const std::vector<FLane>& Lanes, const FCircuit& Circuit, float BrakeMax, float DriveMax, bool bLines, bool bFromTheft = false)
	{
		FGTPDriveSettings S;
		GTPDrive::SetChaseDriving(S);
		std::vector<FGTPDrivePoint> Path;
		std::vector<int> LaneOfPoint;
		std::vector<int> LapOfPoint;
		const auto Add = [&](int Lane, int Lap)
		{
			const std::vector<FLinePoint>* Line = bLines ? Circuit.LineOf(Lane) : nullptr;
			if (Line)
			{
				for (const FLinePoint& Point : *Line)
				{
					Path.push_back({Point.X, Point.Y});
					LaneOfPoint.push_back(Lane);
					LapOfPoint.push_back(Lap);
				}
				return;
			}
			for (const GTPRoadWidth::FPoint& Point : Lanes[static_cast<std::size_t>(Lane)].Points)
			{
				Path.push_back({Point.X, Point.Y});
				LaneOfPoint.push_back(Lane);
				LapOfPoint.push_back(Lap);
			}
		};
		std::size_t FirstPlace = 0;
		if (bFromTheft && !Circuit.Approach.empty())
		{
			for (const int Lane : Circuit.Approach)
			{
				Add(Lane, 0);
			}
			FirstPlace = static_cast<std::size_t>(Circuit.IndexOf[static_cast<std::size_t>(Circuit.Next(Circuit.Approach.back()))]);
		}
		for (int Lap = 0; Lap < 4; ++Lap)
		{
			for (std::size_t Place = 0; Place < Circuit.Lanes.size(); ++Place)
			{
				Add(Circuit.Lanes[(FirstPlace + Place) % Circuit.Lanes.size()], Lap);
			}
		}
		float Yaw = std::atan2(Path[1].Y - Path[0].Y, Path[1].X - Path[0].X);
		// To the right of the heading (Y is south): the kerb side.
		float X = Path[0].X - (bFromTheft ? 3.f * std::sin(Yaw) : 0.f), Y = Path[0].Y + (bFromTheft ? 3.f * std::cos(Yaw) : 0.f);
		float V = bFromTheft ? 0.f : 40.f / 3.6f, Accel = 0.f, Steer = 0.f;
		int Closest = 0;
		FDrive Out;
		const float Dt = 1.f / 60.f;
		float MinuteMetres = 0.f;
		int LapNow = 0;
		while (LapNow < 3 && Out.Seconds < 600.f)
		{
			const std::size_t At = static_cast<std::size_t>(std::max(0, std::min(Closest, static_cast<int>(Path.size()) - 1)));
			const int Lane = LaneOfPoint[At];
			if (LapOfPoint[At] > LapNow)
			{
				Out.LapSeconds[LapNow] = Out.Seconds - (LapNow > 0 ? Out.LapSeconds[0] + (LapNow > 1 ? Out.LapSeconds[1] : 0.f) : 0.f);
				LapNow = LapOfPoint[At];
				continue;
			}
			const float Cruise = (bFromTheft && Out.Seconds < 4.f ? 40.f : CruiseKmh(Lanes[static_cast<std::size_t>(Lane)].Carriageway)) / 3.6f;
			const FGTPDriveCommand Command = GTPDrive::Decide(Path, X, Y, Yaw, V, Cruise, -1.f, S, Closest);
			float Throttle = Command.Throttle;
			// The chase script's firmer throttle (GTPDemoRoute.cpp).
			if (Throttle > 0.f && Command.Brake < 0.05f && V < Cruise - 4.f / 3.6f)
			{
				Throttle = std::max(Throttle, 0.7f);
			}
			const float Wanted = Throttle * DriveMax - Command.Brake * BrakeMax - 0.3f;
			Accel += (Wanted - Accel) * std::min(1.f, Dt / 0.2f);
			Steer += std::max(-Dt * 2.5f, std::min(Dt * 2.5f, Command.Steering - Steer));
			V = std::max(0.f, V + Accel * Dt);
			const float YawRate = V * std::tan(Steer * 0.785f) / 2.97f;
			Yaw += YawRate * Dt;
			X += V * std::cos(Yaw) * Dt;
			Y += V * std::sin(Yaw) * Dt;
			Out.Seconds += Dt;
			Out.Metres += V * Dt;
			MinuteMetres += Out.Seconds <= 60.f ? V * Dt : 0.f;
			Out.PeakKmh = std::max(Out.PeakKmh, V * 3.6f);
			// Metres from the path: the nearest of the segments around the closest point.
			float Off = 1.e9f;
			for (int Seg = std::max(0, Closest - 2); Seg <= Closest + 1 && Seg + 1 < static_cast<int>(Path.size()); ++Seg)
			{
				const FGTPDrivePoint& A = Path[static_cast<std::size_t>(Seg)];
				const FGTPDrivePoint& B = Path[static_cast<std::size_t>(Seg) + 1];
				const float Dx = B.X - A.X, Dy = B.Y - A.Y;
				const float L2 = Dx * Dx + Dy * Dy;
				const float T = L2 > 1.e-6f ? std::max(0.f, std::min(1.f, ((X - A.X) * Dx + (Y - A.Y) * Dy) / L2)) : 0.f;
				Off = std::min(Off, std::hypot(X - (A.X + Dx * T), Y - (A.Y + Dy * T)));
			}
			if (Off > Out.Stray)
			{
				Out.Stray = Off;
				Out.StrayLane = Lane;
			}
		}
		Out.FirstMinuteKmh = MinuteMetres / std::min(60.f, std::max(Out.Seconds, 1.f)) * 3.6f;
		return Out;
	}
}

int main(int Argc, char** Argv)
{
	// A block of four lanes driven clockwise (0 east, 1 south, 2 west, 3 north), a way in (4, 5), a way out that ends
	// (6), a lane nothing leads from to the block (7).
	std::vector<FLane> Lanes;
	Lanes.push_back(Straight(0.f, 0.f, 100.f, 0.f, {1, 6}));
	Lanes.push_back(Straight(103.f, 3.f, 103.f, 80.f, {2}));
	Lanes.push_back(Straight(100.f, 83.f, 0.f, 83.f, {3}));
	Lanes.push_back(Straight(-3.f, 80.f, -3.f, 3.f, {0}));
	Lanes.push_back(Straight(-200.f, 0.f, -100.f, 0.f, {5}));
	Lanes.push_back(Straight(-100.f, 0.f, -3.f, 0.f, {0}));
	Lanes.push_back(Straight(103.f, 0.f, 200.f, 0.f, {}));
	Lanes.push_back(Straight(300.f, 0.f, 400.f, 0.f, {}));
	std::vector<FEnds> Ends;
	for (const FLane& Lane : Lanes)
	{
		Ends.push_back(EndsOf(Lane));
	}

	// The file: comments, a header, rows in cm, a name that is not read.
	const std::string Text =
		"# a comment\n"
		"lane_id,start_x_cm,start_y_cm,end_x_cm,end_y_cm,name\n"
		"0,0,0,10000,0,R A\r\n"
		"1, 10300, 300, 10300, 8000, R B\n"
		"\n"
		"2,10000,8300,0,8300,\n"
		"3,-300,8000,-300,300,R D\n";
	const std::vector<FRow> Rows = ParseRows(Text);
	CHECK(Rows.size() == 4);
	CHECK(Rows[1].LaneHint == 1 && std::fabs(Rows[1].StartX - 103.f) < 0.01f && std::fabs(Rows[1].EndY - 80.f) < 0.01f);
	CHECK(Rows[0].Line.empty());
	CHECK(ParseRows("").empty() && ParseRows("lane_id,a,b,c,d\n# x\n").empty());

	std::vector<int> Lap;
	int Renumbered = -1;
	std::string Error;
	CHECK(Resolve(Rows, Ends, Lap, Renumbered, Error) && Lap == std::vector<int>({0, 1, 2, 3}) && Renumbered == 0);

	// The lanes file made again with other numbers: the rows find their lanes by their ends.
	{
		std::vector<FEnds> Moved = {Ends[7], Ends[3], Ends[2], Ends[1], Ends[0]};
		std::vector<int> Found;
		CHECK(Resolve(Rows, Moved, Found, Renumbered, Error) && Found == std::vector<int>({4, 3, 2, 1}) && Renumbered == 3);
		// A road that is no longer there: no lap, and the reason names the row.
		Moved[2] = Ends[6];
		CHECK(!Resolve(Rows, Moved, Found, Renumbered, Error) && Found.empty() && Error.find("row 3") != std::string::npos);
		// A lane 2.5 m away still matches, 3.5 m away does not.
		std::vector<FRow> Near = Rows;
		Near[0].StartY += 2.5f;
		CHECK(Resolve(Near, Ends, Found, Renumbered, Error) && Found[0] == 0);
		Near[0].StartY += 1.f;
		CHECK(!Resolve(Near, Ends, Found, Renumbered, Error));
	}

	FCircuit Circuit;
	CHECK(BuildOn(Lanes, Lap, Circuit, Error) && Circuit.IsValid());
	CHECK(std::fabs(Circuit.LapMetres - 354.f) < 0.5f);
	// On the lap: the next lane of the lap, never the way out, and the last leads to the first.
	CHECK(Circuit.Next(0) == 1 && Circuit.Next(1) == 2 && Circuit.Next(2) == 3 && Circuit.Next(3) == 0);
	CHECK(Circuit.IsOn(2) && !Circuit.IsOn(4) && !Circuit.IsOn(-1) && !Circuit.IsOn(99));
	// Off the lap: the exit that leads to it, and the metres still to drive.
	CHECK(Circuit.Next(4) == 5 && Circuit.Next(5) == 0);
	CHECK(std::fabs(Circuit.TowardMetres[4] - 97.f) < 0.5f && Circuit.TowardMetres[5] == 0.f && Circuit.TowardMetres[0] == 0.f);
	// Nothing leads to the lap from a dead end, nor from a lane apart.
	CHECK(Circuit.Next(6) == -1 && Circuit.Next(7) == -1 && Circuit.TowardMetres[6] < 0.f && Circuit.Next(-1) == -1 && Circuit.Next(99) == -1);
	CHECK(Circuit.Reaching() == 6);

	// The lines driven: one a row, from the lane's start to its end, each starting where the one before ends.
	{
		const std::string WithLines =
			"0,0,0,10000,0,R A,100 0;5000 20;9800 0\n"
			"1,10300,300,10300,8000,R B,10250 400; 10300 4000 ;10300 7800\n"
			"2,10000,8300,0,8300,,9800 8300;200 8300\n"
			"3,-300,8000,-300,300,R D,-300 7800;-300 500\n";
		std::vector<FRow> LineRows = ParseRows(WithLines);
		CHECK(LineRows.size() == 4 && LineRows[0].Line.size() == 3 && LineRows[1].Line.size() == 3 && LineRows[2].Line.size() == 2);
		CHECK(std::fabs(LineRows[0].Line[1].X - 50.f) < 0.01f && std::fabs(LineRows[0].Line[1].Y - 0.2f) < 0.01f);
		CHECK(std::fabs(LineRows[1].Line[2].Y - 78.f) < 0.01f);
		FCircuit Lined = Circuit;
		CHECK(SetLines(Lined, LineRows, Ends, Error) && Lined.Lines.size() == 4 && Lined.LinePoints() == 10);
		// A row without its line, a line that belongs to another lane, a line that stops short: none is kept.
		std::vector<FRow> Broken = LineRows;
		Broken[2].Line.clear();
		CHECK(!SetLines(Lined, Broken, Ends, Error) && Lined.Lines.empty() && Lined.LinePoints() == 0);
		Broken = LineRows;
		Broken[1].Line = LineRows[3].Line;
		CHECK(!SetLines(Lined, Broken, Ends, Error) && Lined.Lines.empty());
		Broken = LineRows;
		Broken[0].Line.pop_back();
		CHECK(!SetLines(Lined, Broken, Ends, Error) && Lined.Lines.empty());
		CHECK(!SetLines(Lined, Rows, Ends, Error) && Lined.Lines.empty());
	}

	// The approach: the rows after "approach", up to "lap". From its lanes the way to the lap is the approach's.
	{
		const std::string WithApproach =
			"lane_id,start_x_cm,start_y_cm,end_x_cm,end_y_cm,name,line_ue_cm\n"
			"approach\n"
			"4,-20000,0,-10000,0,R IN,-20000 0;-15000 30;-10200 0\n"
			"5,-10000,0,-300,0,R IN,-9800 0;-5000 0;-400 0\n"
			"lap\r\n"
			"0,0,0,10000,0,R A,100 0;5000 20;9800 0\n"
			"1,10300,300,10300,8000,R B,10250 400; 10300 4000 ;10300 7800\n"
			"2,10000,8300,0,8300,,9800 8300;200 8300\n"
			"3,-300,8000,-300,300,R D,-300 7800;-300 500\n";
		const std::vector<FRow> All = ParseRows(WithApproach);
		CHECK(All.size() == 6 && All[0].bApproach && All[1].bApproach && !All[2].bApproach && !All[5].bApproach);
		const std::vector<FRow> ApproachRows(All.begin(), All.begin() + 2);
		const std::vector<FRow> LapRows(All.begin() + 2, All.end());
		const auto NextOf = [&Lanes](int Lane) -> const std::vector<int>& { return Lanes[static_cast<std::size_t>(Lane)].Next; };
		// Two ways in: by 7 and 5 (97 m, the shortest) or by 4 and 5.
		std::vector<FLane> Two = Lanes;
		Two[7] = Straight(-100.f, -200.f, -100.f, 0.f, {5});
		const auto NextOfTwo = [&Two](int Lane) -> const std::vector<int>& { return Two[static_cast<std::size_t>(Lane)].Next; };
		FCircuit With;
		CHECK(BuildOn(Two, Lap, With, Error) && SetLines(With, LapRows, Ends, Error));
		CHECK(!With.IsKnown(4) && With.LineOf(4) == nullptr && With.LineOf(0) != nullptr && With.LineOf(0)->size() == 3);
		CHECK(SetApproach(With, {4, 5}, NextOfTwo, Error) && With.Approach.size() == 2 && With.Next(4) == 5 && With.Next(5) == 0);
		CHECK(With.IsKnown(4) && With.IsKnown(5) && With.IsKnown(0) && !With.IsKnown(7) && !With.IsOn(4) && With.ApproachPlace(5) == 1);
		CHECK(With.LineOf(4) == nullptr);
		CHECK(SetApproachLines(With, ApproachRows, Ends, Error) && With.LineOf(4) != nullptr && With.LineOf(5)->size() == 3 && With.LinePoints() == 16);
		// A line of the approach that stops short of the lap's, an approach without the lap's lines: no line kept.
		std::vector<FRow> Short = ApproachRows;
		Short[1].Line.pop_back();
		CHECK(!SetApproachLines(With, Short, Ends, Error) && With.LineOf(5) == nullptr && With.Approach.size() == 2);
		FCircuit Bare;
		CHECK(BuildOn(Lanes, Lap, Bare, Error) && SetApproach(Bare, {4, 5}, NextOf, Error) && !SetApproachLines(Bare, ApproachRows, Ends, Error));
		// What is not an approach: a lane of the lap, a lane twice, a lane that does not lead on, a last lane that does not lead to the lap.
		CHECK(!SetApproach(Bare, {4, 0}, NextOf, Error) && Bare.Approach.empty() && Bare.Next(4) == 5);
		CHECK(!SetApproach(Bare, {4, 4}, NextOf, Error) && !SetApproach(Bare, {5, 4}, NextOf, Error) && !SetApproach(Bare, {4}, NextOf, Error));
		CHECK(!SetApproach(Bare, {6}, NextOf, Error) && !SetApproach(Bare, {99}, NextOf, Error) && Error.find("99") != std::string::npos);
		CHECK(SetApproach(Bare, {}, NextOf, Error) && Bare.Approach.empty());
	}

	// The shorter of two ways to the lap is taken.
	{
		std::vector<FLane> Two = Lanes;
		Two[7] = Straight(-100.f, -200.f, -100.f, 0.f, {4, 5});
		FCircuit Shorter;
		CHECK(BuildOn(Two, Lap, Shorter, Error) && Shorter.Next(7) == 5 && std::fabs(Shorter.TowardMetres[7] - 97.f) < 0.5f);
	}

	// What is not a lap.
	{
		FCircuit Bad;
		CHECK(!BuildOn(Lanes, {0, 1}, Bad, Error) && !Bad.IsValid());
		CHECK(!BuildOn(Lanes, {0, 1, 2, 9}, Bad, Error) && Error.find("lane 9") != std::string::npos);
		CHECK(!BuildOn(Lanes, {0, 1, 2, 3, 0}, Bad, Error) && Error.find("twice") != std::string::npos);
		CHECK(!BuildOn(Lanes, {0, 2, 1, 3}, Bad, Error) && Error.find("lane 0 does not lead to lane 2") != std::string::npos);
		// The last lane must lead to the first.
		CHECK(!BuildOn(Lanes, {4, 5, 0}, Bad, Error) && Error.find("lane 0 does not lead to lane 4") != std::string::npos);
	}

	// Speeds on the lap: a quay, a main axis, a middle street, a narrow one, an unknown width.
	CHECK(CruiseKmh(12.f) == 100.f && CruiseKmh(16.f) == 100.f && CruiseKmh(9.f) == 75.f && CruiseKmh(7.f) == 75.f);
	CHECK(CruiseKmh(6.f) == 60.f && CruiseKmh(4.f) == 45.f && CruiseKmh(0.f) == 60.f);
	// Never slower than a free flight on the same road.
	for (const float Width : {0.f, 3.f, 5.f, 6.9f, 7.f, 11.9f, 12.f, 20.f})
	{
		CHECK(CruiseKmh(Width) >= GTPRoadWidth::FlightKmh(Width));
	}

	if (Argc >= 3)
	{
		const std::vector<FLane> Real = ReadLanes(Argv[1]);
		std::ifstream File(Argv[2]);
		std::stringstream Buffer;
		Buffer << File.rdbuf();
		std::vector<FRow> RealRows;
		std::vector<FRow> RealApproachRows;
		for (const FRow& Row : ParseRows(Buffer.str()))
		{
			(Row.bApproach ? RealApproachRows : RealRows).push_back(Row);
		}
		std::vector<FEnds> RealEnds;
		for (const FLane& Lane : Real)
		{
			RealEnds.push_back(Lane.Points.empty() ? FEnds{1.e9f, 1.e9f, 1.e9f, 1.e9f} : EndsOf(Lane));
		}
		std::vector<int> RealLap;
		FCircuit RealCircuit;
		if (!Resolve(RealRows, RealEnds, RealLap, Renumbered, Error) || !BuildOn(Real, RealLap, RealCircuit, Error))
		{
			std::printf("flight circuit NOT USED: the %zu row(s) do not fit the %zu lanes read: %s\n", RealRows.size(), Real.size(), Error.c_str());
			++Failures;
		}
		else
		{
			float Main = 0.f, Narrow = 0.f;
			for (const int Lane : RealCircuit.Lanes)
			{
				const FLane& Each = Real[static_cast<std::size_t>(Lane)];
				Main += GTPRoadWidth::Classify(Each.Carriageway) == GTPRoadWidth::EClass::Main ? Each.Length : 0.f;
				Narrow += GTPRoadWidth::Classify(Each.Carriageway) == GTPRoadWidth::EClass::Narrow ? Each.Length : 0.f;
			}
			std::printf("flight circuit of %zu lanes, %.0f m a lap, %.0f %% on main axes, %.0f %% in narrow streets, reached from %d of %zu lanes, %d row(s) found under another number\n",
				RealCircuit.Lanes.size(), static_cast<double>(RealCircuit.LapMetres), static_cast<double>(Main * 100.f / RealCircuit.LapMetres),
				static_cast<double>(Narrow * 100.f / RealCircuit.LapMetres), RealCircuit.Reaching(), Real.size(), Renumbered);
			CHECK(Main >= 0.7f * RealCircuit.LapMetres);
			const bool bHasLines = SetLines(RealCircuit, RealRows, RealEnds, Error);
			std::printf("driving line of %d point(s)%s%s\n", RealCircuit.LinePoints(), bHasLines ? "" : ": none, the lanes' own points are driven: ", bHasLines ? "" : Error.c_str());
			std::vector<int> RealApproach;
			int ApproachRenumbered = 0;
			const auto RealNextOf = [&Real](int Lane) -> const std::vector<int>& { return Real[static_cast<std::size_t>(Lane)].Next; };
			bool bHasApproach = false;
			if (!RealApproachRows.empty())
			{
				bHasApproach = Resolve(RealApproachRows, RealEnds, RealApproach, ApproachRenumbered, Error) && SetApproach(RealCircuit, RealApproach, RealNextOf, Error);
				const bool bApproachLines = bHasApproach && SetApproachLines(RealCircuit, RealApproachRows, RealEnds, Error);
				std::printf("approach of %zu lane(s), %s%s\n", RealCircuit.Approach.size(),
					!bHasApproach ? "NOT USED: " : (bApproachLines ? "driven on its line" : "WITHOUT its driving line: "), bHasApproach && bApproachLines ? "" : Error.c_str());
				CHECK(bHasApproach && bApproachLines);
			}
			for (const bool bLines : {false, true})
			{
				if (bLines && !bHasLines)
				{
					continue;
				}
				for (const float BrakeMax : {6.f, 9.f})
				{
					for (const float DriveMax : {3.f, 4.f})
					{
						const FDrive Run = DriveLaps(Real, RealCircuit, BrakeMax, DriveMax, bLines);
						std::printf("three laps on the %s, brakes %.0f m/s2, drive %.0f m/s2: %.0f m in %.0f s (laps %.0f, %.0f, %.0f s), first minute %.0f km/h, top %.0f km/h, %.1f m off its path at most (lane %d)\n",
							bLines ? "driving line" : "lanes' own points", static_cast<double>(BrakeMax), static_cast<double>(DriveMax), static_cast<double>(Run.Metres),
							static_cast<double>(Run.Seconds), static_cast<double>(Run.LapSeconds[0]), static_cast<double>(Run.LapSeconds[1]), static_cast<double>(Run.LapSeconds[2]),
							static_cast<double>(Run.FirstMinuteKmh), static_cast<double>(Run.PeakKmh), static_cast<double>(Run.Stray), Run.StrayLane);
						if (bLines)
						{
							// What the lap is for: on its line the decisions hold 50 km/h over the first minute and pass 90.
							CHECK(Run.FirstMinuteKmh >= 50.f && Run.PeakKmh >= 90.f && Run.Stray < 3.f);
						}
					}
				}
			}
			if (bHasLines && bHasApproach)
			{
				for (const float DriveMax : {3.f, 4.f})
				{
					// The filmed flight: from the kerb, the approach, then the lap. The standing start costs about 3 km/h on the
					// first minute against the laps above: 49 to 53 km/h here, by what the engine gives.
					const FDrive Film = DriveLaps(Real, RealCircuit, 6.f, DriveMax, true, true);
					std::printf("the filmed flight (kerb, 40 km/h for 4 s, approach, three laps), drive %.0f m/s2: %.0f m in %.0f s, first minute %.0f km/h, top %.0f km/h, %.1f m off its path at most (lane %d)\n",
						static_cast<double>(DriveMax), static_cast<double>(Film.Metres), static_cast<double>(Film.Seconds), static_cast<double>(Film.FirstMinuteKmh),
						static_cast<double>(Film.PeakKmh), static_cast<double>(Film.Stray), Film.StrayLane);
					CHECK(Film.FirstMinuteKmh >= 47.f && Film.PeakKmh >= 90.f && Film.Stray < 3.5f);
				}
			}
		}
	}
	std::printf(Failures ? "FAILED (%d)\n" : "flight circuit: all checks passed\n", Failures);
	return Failures ? 1 : 0;
}
