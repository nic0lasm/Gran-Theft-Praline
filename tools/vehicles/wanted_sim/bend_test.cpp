// Checks the bend speed of the drive logic (GTPDriveLogic.h): noise on a straight lane is not a bend, a real corner slows the car in time.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/bend_test.cpp -o /tmp/bend_test && /tmp/bend_test
#include "GTPDriveLogic.h"

#include <cmath>
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
	const float Closer = GTPDrive::BendSpeed(Corner, 52, 40.f, S, 25.f);
	CHECK(Near < 17.f && Near > 3.f);
	CHECK(Closer < Near);

	// Gate of candidate 49: two lanes meeting at a right angle, their points 20 m apart (62 -> 185, 61 -> 184...).
	// The corner is worth sqrt(7 x 3.8) = 5.2 m/s (19 km/h) whatever the spacing of the points, and the distance
	// to it is the car's own: 20 m before, 16.3 m/s. It read 18 m/s (65 km/h) at the corner itself.
	for (const float Spacing : {1.f, 5.f, 10.f, 20.f, 40.f})
	{
		std::vector<FGTPDrivePoint> Sparse;
		for (float X = 0.f; X < 80.f - 0.01f; X += Spacing)
		{
			Sparse.push_back({X, 0.f});
		}
		const int Vertex = static_cast<int>(Sparse.size());
		Sparse.push_back({80.f, 0.f});
		for (float Y = Spacing; Y <= 80.f + 0.01f; Y += Spacing)
		{
			Sparse.push_back({80.f, Y});
		}
		const auto ClosestTo = [&Sparse](float X, float Y)
		{
			int Closest = 0;
			float Best = 1e9f;
			for (int I = 0; I < static_cast<int>(Sparse.size()); ++I)
			{
				const float D = std::hypot(Sparse[I].X - X, Sparse[I].Y - Y);
				if (D < Best)
				{
					Best = D;
					Closest = I;
				}
			}
			return Closest;
		};
		const float AtCorner = GTPDrive::BendSpeedAt(Sparse, Vertex, 80.f, 0.f, 60.f, S, 25.f);
		CHECK(AtCorner > 4.8f && AtCorner < 5.6f);
		// 20 m before the corner: the closest point is the corner's or one before it, by the spacing.
		const float Before = GTPDrive::BendSpeedAt(Sparse, ClosestTo(60.f, 0.f), 60.f, 0.f, 60.f, S, 25.f);
		CHECK(Before > 15.5f && Before < 16.8f);
		// Round the corner: 1 m after it the car is still in the bend, 12 m after it is free again
		// (with points 40 m apart the corner stays its closest point for 20 m: it held the car all that way).
		CHECK(GTPDrive::BendSpeedAt(Sparse, ClosestTo(80.f, 1.f), 80.f, 1.f, 60.f, S, 25.f) < 5.6f);
		CHECK(GTPDrive::BendSpeedAt(Sparse, ClosestTo(80.f, 12.f), 80.f, 12.f, 60.f, S, 25.f) > 24.9f);
	}
	// The whole decision, a car driven along the lane: the speed wanted at the corner is the corner's.
	{
		std::vector<FGTPDrivePoint> Two = {{0.f, 0.f}, {40.f, 0.f}, {80.f, 0.f}, {80.f, 40.f}, {80.f, 80.f}};
		int Closest = 0;
		FGTPDriveCommand Command;
		for (float X = 0.f; X <= 79.f; X += 0.25f)
		{
			Command = GTPDrive::Decide(Two, X, 0.f, 0.f, 20.f, 25.f, -1.f, S, Closest);
		}
		CHECK(Command.TargetSpeed < 7.f && Command.Limit == GTPDrive::LimitBend && Command.Brake > 0.9f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
