// Checks of the mission parking spot logic (GTPKerbSpot.h), no engine needed:
//   g++ -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/kerb_test/kerb_test.cpp -o kerb_test && ./kerb_test
#include "GTPKerbSpot.h"

#include <cstdio>

static int Failures = 0;

static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	if (!bOk)
	{
		++Failures;
	}
}

static bool Near(float A, float B, float Tolerance = 0.01f)
{
	return std::fabs(A - B) <= Tolerance;
}

int main()
{
	using namespace GTPKerb;
	FSpot Spot;
	Check(SpotBeside({0.f, 0.f}, {1000.f, 0.f}, 250.f, Spot), "a spot beside a segment along +X");
	Check(Near(Spot.Position.X, 500.f) && Near(Spot.Position.Y, 250.f), "middle of the segment, 250 to the right (+Y)");
	Check(Near(Spot.YawDegrees, 0.f), "faces the travel direction");
	Check(SpotBeside({0.f, 0.f}, {0.f, 1000.f}, 250.f, Spot), "a spot beside a segment along +Y");
	Check(Near(Spot.Position.X, -250.f) && Near(Spot.Position.Y, 500.f), "right of +Y is -X");
	Check(Near(Spot.YawDegrees, 90.f), "yaw 90 along +Y");
	Check(!SpotBeside({5.f, 5.f}, {5.f, 5.f}, 250.f, Spot), "no spot on a zero-length segment");

	const std::vector<std::vector<FPoint2>> Lanes = {
		{{0.f, 0.f}, {1000.f, 0.f}, {2000.f, 0.f}},
		{{0.f, 5000.f}, {1000.f, 5000.f}},
		{{3000.f, 0.f}},
	};
	const std::vector<FSegmentRef> Found = SegmentsNear(Lanes, {1600.f, 0.f}, 1200.f);
	Check(Found.size() == 2, "two segments within the radius, none of a far lane or a one-point lane");
	Check(!Found.empty() && Found[0].Lane == 0 && Found[0].Point == 1, "nearest first");
	Check(Found.size() == 2 && Found[1].Point == 0 && Found[0].Distance <= Found[1].Distance, "then the next nearest");
	Check(SegmentsNear(Lanes, {0.f, 0.f}, 10.f).empty(), "nothing when no middle is inside the radius");

	Check(Near(DistanceToSegment({500.f, 300.f}, {0.f, 0.f}, {1000.f, 0.f}), 300.f), "distance to a segment, beside it");
	Check(Near(DistanceToSegment({-300.f, 400.f}, {0.f, 0.f}, {1000.f, 0.f}), 500.f), "distance to a segment, past its end");

	// Two lanes the same way, 3 m apart (gate 59: a car parked beside one lane stood on the next one).
	const std::vector<std::vector<FPoint2>> TwoLanes = {
		{{0.f, 0.f}, {400.f, 0.f}, {800.f, 0.f}},
		{{0.f, 300.f}, {400.f, 300.f}, {800.f, 300.f}},
	};
	const std::vector<FSegmentRef> Nearby = SegmentsNear(TwoLanes, {400.f, 150.f}, 2000.f);
	Check(SpotBeside(TwoLanes[0][0], TwoLanes[0][1], 250.f, Spot) && !IsClearOfLanes(TwoLanes, Nearby, Spot.Position, 210.f),
		"a spot beside the left lane is on the right lane: left out");
	Check(SpotBeside(TwoLanes[1][0], TwoLanes[1][1], 250.f, Spot) && IsClearOfLanes(TwoLanes, Nearby, Spot.Position, 210.f),
		"a spot beside the right lane is clear of both");
	Check(IsClearOfLanes(TwoLanes, {}, {0.f, 0.f}, 210.f), "no segment near: clear");

	Check(Near(SegmentDistance({0.f, 0.f}, {100.f, 0.f}, {50.f, -50.f}, {50.f, 50.f}), 0.f), "crossing segments: 0 apart");
	Check(Near(SegmentDistance({0.f, 0.f}, {100.f, 0.f}, {0.f, 300.f}, {100.f, 300.f}), 300.f), "parallel segments 300 apart");
	Check(Near(SegmentDistance({0.f, 0.f}, {100.f, 0.f}, {130.f, -40.f}, {130.f, 40.f}), 30.f), "a segment past the end of the other");

	FPoint2 Corners[4];
	Footprint({{0.f, 0.f}, 0.f}, 230.f, 210.f, 95.f, Corners);
	Check(Near(Corners[0].X, 230.f) && Near(Corners[0].Y, 95.f) && Near(Corners[2].X, -210.f) && Near(Corners[2].Y, -95.f),
		"footprint along +X: front 230 ahead, back 210 behind, 95 to each side");
	Footprint({{0.f, 0.f}, 90.f}, 230.f, 230.f, 95.f, Corners);
	Check(Near(Corners[0].X, -95.f, 0.1f) && Near(Corners[0].Y, 230.f, 0.1f), "footprint along +Y: the front right corner is at -X");

	// Gate 62: a Clio parked beside one lane, its centre 226 cm from another lane that crosses ahead of it.
	const std::vector<std::vector<FPoint2>> Crossing = {
		{{0.f, 0.f}, {400.f, 0.f}, {800.f, 0.f}, {1200.f, 0.f}, {1600.f, 0.f}, {2000.f, 0.f}, {2400.f, 0.f}, {2800.f, 0.f}},
		{{2426.f, -1000.f}, {2426.f, -600.f}, {2426.f, -200.f}, {2426.f, 200.f}, {2426.f, 600.f}, {2426.f, 1000.f}},
	};
	const std::vector<FSegmentRef> AroundCrossing = SegmentsNear(Crossing, {2200.f, 250.f}, 2000.f);
	Check(SpotBeside(Crossing[0][5], Crossing[0][6], 250.f, Spot) && Near(Spot.Position.X, 2200.f) && Near(Spot.Position.Y, 250.f),
		"the spot beside the sixth segment, 250 to its right");
	Check(IsClearOfLanes(Crossing, AroundCrossing, Spot.Position, 210.f), "its centre is 226 from the crossing lane: the old check let it by");
	Check(!IsBodyClearOfLanes(Crossing, AroundCrossing, Spot, 230.f, 230.f, 95.f, 130.f, 0), "its bonnet is over the crossing lane: left out");
	Check(IsBodyClearOfLanes(TwoLanes, Nearby, {{400.f, 550.f}, 0.f}, 230.f, 230.f, 95.f, 130.f, 1),
		"beside the right of two lanes 3 m apart: its body is 455 from the left one, clear");
	Check(IsBodyClearOfLanes(TwoLanes, Nearby, {{400.f, 550.f}, 0.f}, 230.f, 230.f, 120.f, 130.f, 1),
		"a van as wide: the lane it is parked beside is not checked, it is 130 from it by construction");

	// A straight lane of 40 m, points 4 m apart.
	std::vector<FPoint2> Straight;
	for (int I = 0; I <= 10; ++I)
	{
		Straight.push_back({400.f * I, 0.f});
	}
	FLaneAround Around = LaneAroundSpot(Straight, 1, 1200.f);
	Check(Near(Around.FromStart, 600.f) && Near(Around.ToEnd, 3400.f), "second segment: 6 m from the start, 34 m to the end");
	Around = LaneAroundSpot(Straight, 5, 1200.f);
	Check(Near(Around.FromStart, 2200.f) && Near(Around.ToEnd, 1800.f) && Near(Around.TowardKerb, 0.f), "middle of a straight: nothing towards the kerb");
	// The same lane bending to the right (+Y) from its eighth point: 1 m to the right 10 m after the spot's middle, 2 m at 14 m.
	std::vector<FPoint2> Bending = Straight;
	Bending[7].Y = 50.f;
	Bending[8].Y = 100.f;
	Bending[9].Y = 200.f;
	Bending[10].Y = 300.f;
	Around = LaneAroundSpot(Bending, 5, 1200.f);
	Check(Near(Around.TowardKerb, 100.f), "a bend to the right: the points within 12 m come 1 m towards the kerb");
	Around = LaneAroundSpot(Bending, 2, 1200.f);
	Check(Near(Around.TowardKerb, 0.f), "the same bend more than 12 m ahead is not counted");
	// Bending to the left (-Y): away from a car parked on the right.
	for (FPoint2& P : Bending)
	{
		P.Y = -P.Y;
	}
	Around = LaneAroundSpot(Bending, 5, 1200.f);
	Check(Near(Around.TowardKerb, 0.f), "a bend to the left goes away from the kerb");
	Check(Near(LaneAroundSpot(Straight, 10, 1200.f).ToEnd, 0.f) && Near(LaneAroundSpot({{0.f, 0.f}}, 0, 1200.f).FromStart, 0.f),
		"no segment there: nothing");

	// The line of the filmed flight is kept clear as the lanes are, with its own clearance (ParkFlightClearance, 340 cm).
	// Gate 91: a row of parked cars 2.4 m to the right of the line in the rue Bellecordiere. A line of two streets
	// that meet at a corner: clear of it 3.5 m beside either, not at 2.4 m, nor in the inside of the corner.
	{
		const std::vector<std::vector<FPoint2>> FlightLines = {{{0.f, 0.f}, {1000.f, 0.f}, {2000.f, 0.f}}, {{2000.f, 0.f}, {2000.f, 1000.f}}};
		const std::vector<FSegmentRef> FlightNearby = SegmentsNear(FlightLines, {1000.f, 0.f}, 3000.f);
		Check(FlightNearby.size() == 3, "the three segments of the two lines are near");
		Check(!IsClearOfLanes(FlightLines, FlightNearby, {500.f, 240.f}, 340.f), "2.4 m beside the flight's line: no spot");
		Check(IsClearOfLanes(FlightLines, FlightNearby, {500.f, 350.f}, 340.f), "3.5 m beside it: a spot");
		Check(!IsClearOfLanes(FlightLines, FlightNearby, {1700.f, 350.f}, 340.f), "3.5 m beside one street, 3 m from the other: no spot");
		Check(IsClearOfLanes({}, SegmentsNear({}, {0.f, 0.f}, 3000.f), {0.f, 0.f}, 340.f), "a map without a flight circuit: every spot");
	}

	std::printf("%d failure(s)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
