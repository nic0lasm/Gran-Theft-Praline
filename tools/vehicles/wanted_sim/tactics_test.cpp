// Checks the police tactics rules (GTPPoliceTactics.h): deploying officers, shooting, searching, foot chase.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/tactics_test.cpp -o /tmp/tactics_test && /tmp/tactics_test
#include "GTPPoliceTactics.h"

#include <cmath>
#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

int main()
{
	using namespace GTPPoliceTactics;
	const FSettings S;

	// Officers by level.
	CHECK(OfficersForStars(0, 4) == 0 && OfficersForStars(1, 4) == 1 && OfficersForStars(5, 4) == 4);
	CHECK(OfficersForStars(5, 2) == 2 && OfficersForStars(9, 9) == 4);

	// Deploy: only for a player on foot and close, or a stuck car; never without stars or above the quota.
	CHECK(ShouldDeploy(S, 2, true, 3000.f, false, 0, 4));
	CHECK(!ShouldDeploy(S, 2, false, 3000.f, false, 0, 4));
	CHECK(!ShouldDeploy(S, 2, true, 5000.f, false, 0, 4));
	CHECK(ShouldDeploy(S, 2, false, 5000.f, true, 0, 4));
	CHECK(!ShouldDeploy(S, 0, true, 100.f, false, 0, 4));
	CHECK(!ShouldDeploy(S, 2, true, 3000.f, false, 2, 4));
	CHECK(!ShouldDeploy(S, 5, true, 3000.f, false, 0, 0));

	// Shooting: never at one star, only when provoked at two, always from three.
	CHECK(!ShootsOnSight(S, 1, true) && !ShootsOnSight(S, 2, false) && ShootsOnSight(S, 2, true));
	CHECK(ShootsOnSight(S, 3, false) && ShootsOnSight(S, 5, false));
	CHECK(MayFire(S, 3, false, 2000.f, true, 0.f));
	CHECK(!MayFire(S, 3, false, 2000.f, false, 0.f));
	CHECK(!MayFire(S, 3, false, 4000.f, true, 0.f));
	CHECK(!MayFire(S, 3, false, 2000.f, true, 0.4f));
	CHECK(!MayFire(S, 1, true, 2000.f, true, 0.f));
	// Aim gets better and fire faster with the level.
	CHECK(SpreadFactor(S, 3) == S.SpreadAtThree && SpreadFactor(S, 5) < SpreadFactor(S, 3) && SpreadFactor(S, 5) >= 1.f);
	CHECK(FireInterval(S, 5) < FireInterval(S, 3) && FireInterval(S, 5) >= S.FireIntervalMin);

	// Search: officers spread over distinct points, the ring widens up to its cap.
	{
		float X0, Y0, X1, Y1, X2, Y2;
		SearchOffset(S, 0, 3, 0.f, X0, Y0);
		SearchOffset(S, 1, 3, 0.f, X1, Y1);
		SearchOffset(S, 2, 3, 0.f, X2, Y2);
		CHECK(std::hypot(X0 - X1, Y0 - Y1) > 1000.f && std::hypot(X1 - X2, Y1 - Y2) > 1000.f && std::hypot(X0 - X2, Y0 - Y2) > 1000.f);
		CHECK(std::fabs(std::hypot(X0, Y0) - S.SearchRadius) < 1.f);
		float Xa, Ya;
		SearchOffset(S, 0, 3, 40.f, Xa, Ya);
		CHECK(std::hypot(Xa, Ya) > S.SearchRadius);
		SearchOffset(S, 0, 3, 5000.f, Xa, Ya);
		CHECK(std::hypot(Xa, Ya) <= S.SearchMaxRadius + 1.f);
	}

	// Foot chase against a player who stands still: the officer arrives from 30 m in about 7 s.
	{
		float X = 0.f, Y = 0.f;
		float T = 0.f;
		while (!StepToward(S, X, Y, 3000.f, 0.f, 1.f / 30.f) && T < 30.f)
		{
			T += 1.f / 30.f;
		}
		CHECK(T > 5.f && T < 8.f);
		CHECK(std::fabs(3000.f - X) <= S.ArriveRadius + 1.f);
	}
	// Against a player who runs faster than an officer, the gap never closes: foot officers alone do not catch a sprinter.
	{
		float X = 0.f, Y = 0.f, Px = 1000.f;
		const float PlayerSpeed = 600.f;
		for (float T = 0.f; T < 20.f; T += 1.f / 30.f)
		{
			Px += PlayerSpeed / 30.f;
			StepToward(S, X, Y, Px, 0.f, 1.f / 30.f);
		}
		CHECK(Px - X > 1000.f);
	}

	if (Failures == 0)
	{
		std::printf("tactics_test: all checks passed\n");
	}
	return Failures == 0 ? 0 : 1;
}
