// Checks that a chase holds the speed asked (GTPDriveLogic.h): the end of the known path and a short bend
// horizon must not cap it. The first case reproduces the cap found on 16/11: police cars asked 105 km/h
// with the lane follower's 50 m of known lanes and the calm city settings stayed near 70 km/h.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/chase_speed_test.cpp -o /tmp/chase_speed_test && /tmp/chase_speed_test
#include "GTPDriveLogic.h"

#include <algorithm>
#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

namespace
{
	constexpr float LaneLength = 30.f; // the quarter's lanes are short
	constexpr int LanePoints = 31;     // a point every metre, both ends included

	struct FRun
	{
		float PeakKmh = 0.f;
		float MeanKmh = 0.f; // after the first 15 s
		float PathEndSeconds = 0.f;
	};

	/**
	 * A car on a straight road cut in 30 m lanes, driven as FGTPLaneFollower does: it knows its lane and
	 * lanes ahead until they add up to AheadM, and plans again only when it enters the next lane.
	 * Pedals: 4 m/s2 at full throttle, 8 m/s2 at full brake.
	 */
	FRun Drive(const FGTPDriveSettings& S, float AheadM, float AskedKmh, float Seconds)
	{
		FRun Run;
		const float Asked = AskedKmh / 3.6f;
		float X = 0.f, V = 0.f, LaneStart = 0.f;
		int Closest = 0;
		std::vector<FGTPDrivePoint> Path;
		const auto Plan = [&]()
		{
			int Lanes = 1;
			for (float Ahead = 0.f; Ahead < AheadM; Ahead += LaneLength)
			{
				++Lanes;
			}
			Path.clear();
			for (int Lane = 0; Lane < Lanes; ++Lane)
			{
				for (int Point = 0; Point < LanePoints; ++Point)
				{
					Path.push_back({LaneStart + static_cast<float>(Lane) * LaneLength + static_cast<float>(Point), 0.f});
				}
			}
		};
		Plan();
		const float Dt = 0.02f;
		double Sum = 0.0;
		int Samples = 0;
		for (float T = 0.f; T < Seconds; T += Dt)
		{
			const FGTPDriveCommand Command = GTPDrive::Decide(Path, X, 0.f, 0.f, V, Asked, -1.f, S, Closest);
			if (Closest >= LanePoints)
			{
				Closest -= LanePoints;
				LaneStart += LaneLength;
				Plan();
			}
			V = std::max(0.f, V + (Command.Throttle * 4.f - Command.Brake * 8.f) * Dt);
			X += V * Dt;
			Run.PeakKmh = std::max(Run.PeakKmh, V * 3.6f);
			if (Command.Limit == GTPDrive::LimitPathEnd)
			{
				Run.PathEndSeconds += Dt;
			}
			if (T >= 15.f)
			{
				Sum += V * 3.6f;
				++Samples;
			}
		}
		Run.MeanKmh = Samples > 0 ? static_cast<float>(Sum / Samples) : 0.f;
		return Run;
	}
}

int main()
{
	// The cap: calm settings, 50 m known ahead, 105 km/h asked (a police car at 3 stars before the fix).
	const FGTPDriveSettings Calm;
	const FRun Capped = Drive(Calm, 50.f, 105.f, 60.f);
	std::printf("calm, 50 m ahead, asked 105: peak %.0f km/h, mean %.0f km/h, %.0f s held by the end of the path\n", Capped.PeakKmh, Capped.MeanKmh, Capped.PathEndSeconds);
	CHECK(Capped.MeanKmh < 85.f);
	CHECK(Capped.PathEndSeconds > 20.f);

	// The fix: the unit's chase settings and the path the speed needs.
	FGTPDriveSettings Chase;
	Chase.MaxLateralAccel = 6.f;
	Chase.ComfortDecel = 7.f;
	Chase.SafeGap = 3.f;
	Chase.ThrottleGain = 0.8f;
	for (const float Asked : {70.f, 90.f, 105.f, 120.f, 135.f})
	{
		const FRun Free = Drive(Chase, GTPDrive::PathNeeded(Asked / 3.6f, Chase), Asked, 60.f);
		std::printf("chase, %.0f m ahead, asked %.0f: peak %.0f km/h, mean %.0f km/h, %.0f s held by the end of the path\n", GTPDrive::PathNeeded(Asked / 3.6f, Chase), Asked,
			Free.PeakKmh, Free.MeanKmh, Free.PathEndSeconds);
		CHECK(Free.MeanKmh > Asked - 3.f);
		CHECK(Free.PeakKmh < Asked + 2.f);
		CHECK(Free.PathEndSeconds < 0.01f);
	}
	// City traffic keeps its 50 m: at 50 km/h the calm settings need less.
	CHECK(GTPDrive::PathNeeded(50.f / 3.6f, Calm) < 50.f);

	// Bends: at 135 km/h a right angle must be seen from the braking distance, not from 40 m.
	std::vector<FGTPDrivePoint> Corner;
	for (int I = 0; I <= 150; ++I)
	{
		Corner.push_back({static_cast<float>(I), 0.f});
	}
	for (int I = 1; I < 60; ++I)
	{
		Corner.push_back({150.f, static_cast<float>(I)});
	}
	const float Fast = 135.f / 3.6f;
	CHECK(GTPDrive::BendHorizon(Fast, Chase) > 105.f);
	CHECK(GTPDrive::BendHorizon(50.f / 3.6f, Calm) == 40.f); // city traffic: as before
	// 90 m before the corner: unseen with 40 m, seen with the braking distance.
	CHECK(GTPDrive::BendSpeed(Corner, 60, 40.f, Chase, Fast) > Fast - 0.1f);
	CHECK(GTPDrive::BendSpeed(Corner, 60, GTPDrive::BendHorizon(Fast, Chase), Chase, Fast) < Fast - 1.f);
	// Driven to the corner at 135 km/h asked: the car gets there at a speed the corner can take (8 m/s2 of brakes).
	{
		float X = 0.f, V = Fast;
		int Closest = 0;
		float AtCorner = -1.f;
		for (float T = 0.f; T < 30.f && AtCorner < 0.f; T += 0.02f)
		{
			const FGTPDriveCommand Command = GTPDrive::Decide(Corner, X, 0.f, 0.f, V, Fast, -1.f, Chase, Closest);
			V = std::max(0.f, V + (Command.Throttle * 4.f - Command.Brake * 8.f) * 0.02f);
			X += V * 0.02f;
			if (X >= 147.f)
			{
				AtCorner = V;
			}
		}
		std::printf("corner after 150 m at 135 km/h asked: reached at %.0f km/h\n", AtCorner * 3.6f);
		CHECK(AtCorner > 0.f && AtCorner < 45.f / 3.6f);
	}
	// Exits: the open road behind a lane. 0 -> 1 (30 m, dead end) or 2 -> 3 -> 4 -> 2 (a block to drive round), 5 -> 6 (400 m, dead end).
	{
		const std::vector<std::vector<int>> Next = {{1, 2}, {}, {3}, {4}, {2}, {6}, {}};
		const std::vector<float> Length = {50.f, 30.f, 60.f, 60.f, 60.f, 20.f, 400.f};
		const auto NextOf = [&Next](int Lane) -> const std::vector<int>& { return Next[static_cast<size_t>(Lane)]; };
		const auto LengthOf = [&Length](int Lane) { return Length[static_cast<size_t>(Lane)]; };
		CHECK(GTPDrive::OpenRoad(1, 6, 250.f, NextOf, LengthOf) == 30.f);  // the dead end of the quay
		CHECK(GTPDrive::OpenRoad(2, 6, 250.f, NextOf, LengthOf) == 250.f); // round the block: open
		CHECK(GTPDrive::OpenRoad(5, 6, 250.f, NextOf, LengthOf) == 250.f); // a long way before its end: open enough
		CHECK(GTPDrive::OpenRoad(0, 6, 250.f, NextOf, LengthOf) == 250.f);
		CHECK(GTPDrive::OpenRoad(2, 1, 1000.f, NextOf, LengthOf) == 1000.f); // still going at the depth searched: open
		CHECK(GTPDrive::OpenRoad(6, 6, 1000.f, NextOf, LengthOf) == 400.f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
