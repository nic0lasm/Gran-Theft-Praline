// Checks of the far traffic logic (GTPTrafficFar.h), no engine needed:
//   g++ -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/far_test/far_test.cpp -o far_test && ./far_test
#include "GTPTrafficFar.h"
#include "GTPTrafficGhost.h"

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
	const GTPFar::FLanePath Lane = GTPFar::MakeLanePath({{0.f, 0.f}, {30.f, 0.f}, {30.f, 40.f}});
	Check(Near(Lane.Length, 70.f), "lane length is the sum of its segments");
	Check(Near(GTPFar::DistanceOnLane(Lane, 1, 10.f), 40.f), "distance on lane = start of the point + along");
	Check(Near(GTPFar::DistanceOnLane(Lane, 5, 0.f), 70.f), "distance on lane clamps past the end");

	Check(Near(GTPFar::GapAhead(10.f, {5.f, 25.f, 40.f}), 15.f), "gap ahead takes the nearest car in front");
	Check(GTPFar::GapAhead(50.f, {5.f, 25.f}) < 0.f, "no car ahead: negative gap");
	Check(GTPFar::GapAhead(10.f, {10.f}) < 0.f, "a car does not see itself");

	Check(Near(GTPFar::WantSpeed(13.9f, -1.f, -1.f, 8.f, 3.f), 13.9f), "free road: lane limit");
	Check(Near(GTPFar::WantSpeed(13.9f, 8.f, -1.f, 8.f, 3.f), 0.f), "at the minimum gap: stop");
	Check(Near(GTPFar::WantSpeed(13.9f, 14.f, -1.f, 8.f, 3.f), 6.f), "6 m of room at 3 m/s2: 6 m/s");
	Check(Near(GTPFar::WantSpeed(13.9f, -1.f, 0.f, 8.f, 3.f), 0.f), "at the red light: stop");
	Check(GTPFar::WantSpeed(13.9f, 100.f, 6.f, 8.f, 3.f) < 6.1f, "the nearer of gap and light wins");

	using GTPFar::EAction;
	Check(GTPFar::Decide(8000.f, true, true, false, 9000.f, 5000.f, 25000.f) == EAction::Promote, "close and a driven car free: promote");
	Check(GTPFar::Decide(8000.f, true, false, false, 9000.f, 5000.f, 25000.f) == EAction::Keep, "close, none free, seen: keep");
	Check(GTPFar::Decide(4000.f, false, false, false, 9000.f, 5000.f, 25000.f) == EAction::Relocate, "very close, none free, unseen: relocate");
	Check(GTPFar::Decide(4000.f, true, false, false, 9000.f, 5000.f, 25000.f) == EAction::Keep, "very close but seen: never vanish in view");
	Check(GTPFar::Decide(30000.f, false, false, false, 9000.f, 5000.f, 25000.f) == EAction::Relocate, "too far and unseen: relocate");
	Check(GTPFar::Decide(15000.f, false, false, true, 9000.f, 5000.f, 25000.f) == EAction::Relocate, "dead end, unseen: relocate");
	Check(GTPFar::Decide(15000.f, false, false, false, 9000.f, 5000.f, 25000.f) == EAction::Keep, "in the far ring: keep");

	Check(GTPFar::PickSwap({3000.f, 12000.f, 20000.f}, 9000.f, 8000.f) == 2, "swap with the farthest driven car");
	Check(GTPFar::PickSwap({3000.f, 6000.f}, 9000.f, 8000.f) == -1, "no driven car past the ghost distance: no swap");
	Check(GTPFar::PickSwap({9500.f}, 9000.f, 9800.f) == -1, "never swap with a car nearer than the far car");

	// A far car driven along the lane with the ghost step reaches the end and stops there.
	int Index = 0;
	float Along = 0.f;
	FGTPDrivePoint Position;
	float Yaw = 0.f;
	bool bMoving = true;
	for (int Step = 0; Step < 200 && bMoving; ++Step)
	{
		bMoving = GTPGhost::Advance(Lane.Points, Index, Along, 0.5f, Position, Yaw);
	}
	Check(!bMoving && Near(Position.X, 30.f) && Near(Position.Y, 40.f), "slides to the end of the lane, then reports it");
	Check(Near(GTPFar::DistanceOnLane(Lane, Index, Along), 70.f), "distance on lane at the end = lane length");

	std::printf("%s (%d failure%s)\n", Failures == 0 ? "PASS" : "FAIL", Failures, Failures == 1 ? "" : "s");
	return Failures == 0 ? 0 : 1;
}
