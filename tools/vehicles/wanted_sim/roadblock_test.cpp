// Checks the roadblock placement (GTPRoadblockLogic.h).
//   g++ -O2 -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/roadblock_test.cpp -o /tmp/roadblock_test && /tmp/roadblock_test
#include "GTPRoadblockLogic.h"

#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

static std::vector<FGTPRoadPoint> Straight(float Length, float Step)
{
	std::vector<FGTPRoadPoint> Path;
	for (float X = 0.f; X <= Length; X += Step) Path.push_back({X, 0.f});
	return Path;
}

int main()
{
	const FGTPRoadblockSettings S;
	{
		const auto Path = Straight(500.f, 10.f);
		const auto Plan = GTPRoadblock::Plan(Path, 0, 0.f, 0.f, S);
		CHECK(Plan.bValid);
		CHECK(Plan.Cars[0].X > 215.f && Plan.Cars[0].X < 235.f);
		// Two cars side by side across a lane running along X: they differ in Y by the spacing.
		CHECK(std::fabs(std::fabs(Plan.Cars[0].Y - Plan.Cars[1].Y) - S.Spacing) < 0.01f);
		CHECK(std::fabs(Plan.Cars[0].Yaw + Plan.Cars[1].Yaw) < 1e-4f); // symmetric angles
	}
	{
		// Path too short: no block.
		CHECK(!GTPRoadblock::Plan(Straight(100.f, 10.f), 0, 0.f, 0.f, S).bValid);
		CHECK(!GTPRoadblock::Plan({}, 0, 0.f, 0.f, S).bValid);
	}
	{
		// A U-turn that brings the path back next to the player: no point is both far enough along and far enough away.
		std::vector<FGTPRoadPoint> UTurn;
		for (float X = 0.f; X <= 200.f; X += 10.f) UTurn.push_back({X, 0.f});
		for (float X = 190.f; X >= 0.f; X -= 10.f) UTurn.push_back({X, 20.f});
		FGTPRoadblockSettings Far = S;
		Far.AheadMetres = 300.f;
		CHECK(!GTPRoadblock::Plan(UTurn, 0, 0.f, 0.f, Far).bValid);
		// With the shorter default distance the block sits on the way out, on the first leg.
		const auto Plan = GTPRoadblock::Plan(UTurn, 0, 0.f, 0.f, S);
		CHECK(Plan.bValid && Plan.Cars[0].X > 100.f);
	}
	{
		const auto Path = Straight(500.f, 10.f);
		const auto Plan = GTPRoadblock::Plan(Path, 0, 0.f, 0.f, S);
		CHECK(!GTPRoadblock::IsBehind(Plan, 0.f, 0.f, 0.f));
		CHECK(GTPRoadblock::IsBehind(Plan, 400.f, 0.f, 0.f)); // player went past it
		CHECK(GTPRoadblock::IsBehind(FGTPRoadblockPlan(), 0.f, 0.f, 0.f));
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
