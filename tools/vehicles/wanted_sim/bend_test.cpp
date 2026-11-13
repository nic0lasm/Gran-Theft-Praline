// Checks the bend speed of the drive logic (GTPDriveLogic.h): noise on a straight lane is not a bend, a real corner slows the car in time.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/bend_test.cpp -o /tmp/bend_test && /tmp/bend_test
#include "GTPDriveLogic.h"

#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

int main()
{
	FGTPDriveSettings S;
	S.MaxLateralAccel = 7.f;
	S.ComfortDecel = 6.f;
	// A straight lane with points a metre apart and 10 cm of zig-zag: no limit.
	std::vector<FGTPDrivePoint> Noisy;
	for (int I = 0; I < 100; ++I)
	{
		Noisy.push_back({static_cast<float>(I), (I % 2 == 0) ? 0.1f : -0.1f});
	}
	CHECK(GTPDrive::BendSpeed(Noisy, 1, 40.f, S, 25.f) > 24.9f);
	// A right angle 30 m ahead: 25 m/s is too fast now, and the speed wanted falls as the corner comes nearer.
	std::vector<FGTPDrivePoint> Corner;
	for (int I = 0; I < 60; ++I)
	{
		Corner.push_back({static_cast<float>(I), 0.f});
	}
	for (int I = 1; I < 60; ++I)
	{
		Corner.push_back({59.f, static_cast<float>(I)});
	}
	const float Far = GTPDrive::BendSpeed(Corner, 1, 40.f, S, 25.f);
	const float Near = GTPDrive::BendSpeed(Corner, 40, 40.f, S, 25.f);
	CHECK(Far > 24.9f);           // 59 m away: beyond the horizon
	CHECK(Near < 15.f && Near > 3.f);
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
