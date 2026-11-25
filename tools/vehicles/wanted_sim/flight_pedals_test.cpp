// The chase script's car through the three corners of the Bellecour circuit where gate 83's car left its lane,
// driven the way the game drives them: the lane follower drops the points of a lane the moment the car's closest
// point is on the next one (GTPFlightPedals.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/flight_pedals_test.cpp -o /tmp/flight_pedals_test && /tmp/flight_pedals_test
//
// The lanes are the circuit's own lines (L_City_RueDeLaRepublique.circuit.csv, in metres from the first point of
// each), with the speed asked on each. The car is a bicycle model, not the game's: 2.97 m between the axles,
// 45 degrees of lock reached at 2.5 lock/s, pedals that answer in 0.2 s, 4.5 m/s2 of drive (gate 83's lines: 36 to
// 52 km/h in a second with the wheels turned), 8 m/s2 of brakes, and no more than 8 m/s2 sideways.
//
// What it shows: the speed wanted jumps as a lane is left in a corner, the throttle then comes with the wheels
// turned, and the rule takes that throttle away at little cost in time. What it does not show: the car of the
// model takes these corners either way, where the game's left its lane by 13 to 27 m. So the rule is also
// checked on the very lines of gate 83, and whether the game's car now holds these corners is for the gate to say.
#include "GTPFlightPedals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

namespace
{
	struct FLane
	{
		float Kmh;
		std::vector<FGTPDrivePoint> Line;
	};

	struct FCorner
	{
		const char* Name;
		std::vector<FLane> Lanes;
	};

	const std::vector<FCorner>& Corners()
	{
		static const std::vector<FCorner> All = {
			{"quay to rue Childebert", {
				{100.f, {{0.0f, 0.0f}, {-0.4f, 2.9f}, {-0.7f, 5.9f}, {-1.1f, 8.9f}, {-1.4f, 11.8f}, {-1.7f, 14.8f}, {-2.0f, 17.8f}, {-2.3f, 20.8f}, {-2.6f, 23.8f}, {-2.8f, 26.8f}, {-3.0f, 29.8f}, {-3.2f, 32.8f}, {-3.4f, 35.7f}, {-3.6f, 38.7f}, {-4.6f, 40.5f}}},
				{60.f, {{-6.9f, 40.5f}, {-9.7f, 39.8f}, {-12.6f, 39.6f}, {-15.6f, 39.6f}, {-18.6f, 39.7f}, {-21.7f, 39.7f}, {-24.7f, 39.7f}, {-27.7f, 39.6f}, {-30.7f, 39.6f}, {-33.6f, 39.6f}, {-36.6f, 39.5f}, {-39.6f, 39.4f}, {-42.6f, 39.3f}, {-45.5f, 39.2f}, {-48.4f, 39.1f}, {-51.3f, 39.0f}, {-54.1f, 39.0f}, {-56.9f, 39.0f}, {-59.7f, 39.1f}, {-62.5f, 39.1f}}}}},
			{"rue Childebert to place de la Republique", {
				{60.f, {{0.0f, 0.0f}, {-2.9f, 0.1f}, {-5.7f, 0.2f}, {-8.7f, 0.2f}, {-11.6f, 0.2f}, {-14.6f, 0.2f}, {-17.6f, 0.3f}, {-20.6f, 0.3f}, {-23.6f, 0.3f}, {-26.5f, 0.4f}, {-29.5f, 0.4f}, {-32.5f, 0.5f}, {-35.5f, 0.6f}, {-38.5f, 0.6f}, {-41.5f, 0.6f}, {-44.5f, 0.6f}, {-47.5f, 0.6f}, {-50.5f, 0.6f}, {-53.5f, 0.5f}, {-56.5f, 0.5f}, {-59.5f, 0.4f}, {-62.5f, 0.3f}, {-65.5f, 0.2f}, {-68.5f, 0.0f}, {-71.4f, -0.1f}, {-74.4f, -0.2f}, {-77.4f, -0.3f}, {-80.3f, -0.0f}, {-83.0f, 0.4f}, {-85.3f, 1.4f}, {-86.8f, 2.9f}}},
				{45.f, {{-87.6f, 5.2f}, {-87.9f, 7.9f}, {-88.0f, 10.9f}, {-88.0f, 13.9f}, {-88.0f, 16.9f}, {-88.0f, 19.9f}, {-88.0f, 22.9f}, {-88.0f, 25.8f}, {-87.9f, 28.8f}, {-87.7f, 31.8f}}},
				{50.f, {{-87.5f, 34.8f}, {-87.2f, 37.7f}, {-86.8f, 40.7f}, {-86.3f, 43.6f}, {-85.6f, 46.5f}, {-84.9f, 49.4f}, {-84.1f, 52.3f}, {-83.3f, 55.2f}, {-82.4f, 58.0f}, {-81.5f, 60.9f}, {-80.6f, 63.8f}, {-79.7f, 66.6f}, {-78.8f, 69.5f}, {-77.9f, 72.3f}, {-76.9f, 75.2f}, {-76.0f, 78.0f}, {-75.1f, 80.9f}, {-74.2f, 83.8f}, {-73.3f, 86.6f}, {-72.3f, 89.5f}, {-71.4f, 92.3f}, {-70.4f, 95.1f}, {-69.3f, 97.9f}}}}},
			{"rue Herriot to rue Jean de Tournes", {
				{75.f, {{0.0f, 0.0f}, {0.2f, -2.9f}, {0.4f, -5.8f}, {0.6f, -8.8f}, {0.7f, -11.7f}, {0.8f, -14.7f}, {0.9f, -17.7f}, {1.0f, -20.7f}, {1.0f, -23.7f}, {1.1f, -26.7f}, {1.1f, -29.7f}, {1.2f, -32.7f}, {1.2f, -35.7f}, {1.3f, -38.7f}, {1.3f, -41.7f}, {1.3f, -44.7f}, {0.6f, -47.2f}}},
				{75.f, {{-0.4f, -49.0f}, {0.2f, -50.3f}, {2.6f, -50.5f}}},
				{60.f, {{5.5f, -50.2f}, {8.5f, -50.1f}, {11.5f, -50.2f}, {14.5f, -50.4f}, {17.5f, -50.4f}, {20.5f, -50.5f}, {23.5f, -50.4f}, {26.5f, -50.4f}, {29.5f, -50.4f}, {32.5f, -50.3f}, {35.5f, -50.3f}, {38.5f, -50.3f}, {41.5f, -50.2f}, {44.5f, -50.2f}, {47.5f, -50.1f}, {50.5f, -50.0f}, {53.5f, -50.0f}, {56.5f, -49.9f}, {59.5f, -49.9f}, {62.5f, -49.8f}, {65.5f, -49.8f}, {68.5f, -49.8f}, {71.5f, -49.7f}, {74.5f, -49.7f}, {77.5f, -49.7f}, {80.5f, -49.7f}, {83.4f, -49.8f}, {86.4f, -49.8f}, {89.4f, -49.8f}, {92.3f, -49.8f}, {95.3f, -49.8f}, {98.3f, -49.7f}, {101.2f, -49.5f}, {104.2f, -49.3f}, {107.2f, -49.1f}}}}},
		};
		return All;
	}

	constexpr float SidewaysMost = 8.f;
	constexpr float DriveMost = 4.5f;
	constexpr float BrakesMost = 8.f;

	struct FRun
	{
		float Off = 0.f;              // the furthest from the line, m
		float TurnedKmh = 0.f;        // the fastest with more than half of the lock on the wheels
		float ThrottleTurned = 0.f;   // seconds of throttle over 0.5 with more than half of the lock asked and the car faster than that lock holds
		float Seconds = 0.f;
		bool bWantJumped = false;     // the speed wanted rose by half or more as a lane was left
		bool bArrived = false;
	};

	float Stray(const std::vector<FGTPDrivePoint>& Line, float X, float Y)
	{
		float Best = 1e9f;
		for (size_t Index = 0; Index + 1 < Line.size(); ++Index)
		{
			const float Dx = Line[Index + 1].X - Line[Index].X, Dy = Line[Index + 1].Y - Line[Index].Y;
			const float Length2 = Dx * Dx + Dy * Dy;
			const float Along = Length2 > 0.f ? std::max(0.f, std::min(1.f, ((X - Line[Index].X) * Dx + (Y - Line[Index].Y) * Dy) / Length2)) : 0.f;
			Best = std::min(Best, std::hypot(X - Line[Index].X - Dx * Along, Y - Line[Index].Y - Dy * Along));
		}
		return Best;
	}

	/** From a standstill on the first lane, FromPoint points into it (the script puts the car back on a lane at rest). */
	FRun Drive(const FCorner& Corner, int FromPoint, bool bFollowSteering, bool bTrace = false)
	{
		FGTPDriveSettings Settings;
		GTPDrive::SetChaseDriving(Settings);
		std::vector<FGTPDrivePoint> Whole;
		for (const FLane& Lane : Corner.Lanes)
		{
			Whole.insert(Whole.end(), Lane.Line.begin(), Lane.Line.end());
		}
		// The follower's path: the lane the car is on and those after it.
		size_t LaneIndex = 0;
		std::vector<FGTPDrivePoint> Path = Whole;
		const std::vector<FGTPDrivePoint>& First = Corner.Lanes[0].Line;
		const FGTPDrivePoint& End = Whole.back();
		float X = First[FromPoint].X, Y = First[FromPoint].Y;
		float Yaw = std::atan2(First[FromPoint + 1].Y - Y, First[FromPoint + 1].X - X);
		float V = 0.f, Accel = 0.f, Steer = 0.f, WantBefore = -1.f, NextTrace = 0.f;
		int Closest = FromPoint;
		FRun Run;
		const float Dt = 1.f / 60.f;
		while (Run.Seconds < 60.f && Run.Off < 80.f)
		{
			if (std::hypot(End.X - X, End.Y - Y) < 25.f)
			{
				Run.bArrived = true;
				break;
			}
			const float Cruise = Corner.Lanes[LaneIndex].Kmh / 3.6f;
			FGTPDriveCommand Command = GTPDrive::Decide(Path, X, Y, Yaw, V, Cruise, -1.f, Settings, Closest);
			const int LaneLength = static_cast<int>(Corner.Lanes[LaneIndex].Line.size());
			if (Closest >= LaneLength && LaneIndex + 1 < Corner.Lanes.size())
			{
				// FGTPLaneFollower::Step: the lane left goes, its points with it.
				Closest -= LaneLength;
				Path.erase(Path.begin(), Path.begin() + LaneLength);
				++LaneIndex;
				WantBefore = Command.TargetSpeed;
				continue;
			}
			if (WantBefore > 0.f)
			{
				Run.bWantJumped = Run.bWantJumped || Command.TargetSpeed > WantBefore * 1.5f;
				WantBefore = -1.f;
			}
			const float Asked = Command.Steering;
			GTPFlightPedals::Apply(Command.Throttle, Command.Brake, Command.Steering, V, Cruise, Settings, bFollowSteering);
			CHECK(Command.Steering == Asked);
			if (std::fabs(Asked) > 0.5f && Command.Throttle > 0.5f && V > GTPFlightPedals::HoldSpeed(Asked, Settings))
			{
				Run.ThrottleTurned += Dt;
			}
			const float Wanted = Command.Throttle * DriveMost - Command.Brake * BrakesMost - 0.3f;
			Accel += (Wanted - Accel) * std::min(1.f, Dt / 0.2f);
			Steer += std::max(-Dt * 2.5f, std::min(Dt * 2.5f, Command.Steering - Steer));
			V = std::max(0.f, V + Accel * Dt);
			float YawRate = V * std::tan(Steer * 0.785f) / 2.97f;
			if (std::fabs(V * YawRate) > SidewaysMost)
			{
				YawRate *= SidewaysMost / std::fabs(V * YawRate);
			}
			Yaw += YawRate * Dt;
			X += V * std::cos(Yaw) * Dt;
			Y += V * std::sin(Yaw) * Dt;
			Run.Seconds += Dt;
			Run.Off = std::max(Run.Off, Stray(Whole, X, Y));
			if (std::fabs(Steer) > 0.5f)
			{
				Run.TurnedKmh = std::max(Run.TurnedKmh, V * 3.6f);
			}
			if (bTrace && Run.Seconds >= NextTrace)
			{
				NextTrace += 0.25f;
				std::printf("    t=%.2f lane=%d kmh=%.0f throttle=%.2f brake=%.2f steer=%.2f want=%.0f off=%.1f\n", static_cast<double>(Run.Seconds), static_cast<int>(LaneIndex),
					static_cast<double>(V * 3.6f), static_cast<double>(Command.Throttle), static_cast<double>(Command.Brake), static_cast<double>(Asked), static_cast<double>(Command.TargetSpeed * 3.6f),
					static_cast<double>(Stray(Whole, X, Y)));
			}
		}
		return Run;
	}
}

int main(int ArgCount, char**)
{
	const bool bTrace = ArgCount > 1;
	float ThrottleTurnedBefore = 0.f;
	int Jumps = 0;
	for (const FCorner& Corner : Corners())
	{
		const int Points = static_cast<int>(Corner.Lanes[0].Line.size());
		// From the start of the lane before the corner, from its middle, and from 4 points (about 11 m) before its end.
		for (const int From : {0, Points / 2, Points - 5})
		{
			const FRun Before = Drive(Corner, From, false, bTrace);
			const FRun After = Drive(Corner, From, true, bTrace);
			std::printf("%s, from point %d of %d: pedals alone %.1f m off the line (%.0f km/h with the wheels turned, %.1f s of throttle past what they hold)%s; following the steering %.1f m (%.0f km/h), %.1f s against %.1f s\n",
				Corner.Name, From, Points, static_cast<double>(Before.Off), static_cast<double>(Before.TurnedKmh), static_cast<double>(Before.ThrottleTurned), Before.bWantJumped ? ", speed wanted jumped" : "",
				static_cast<double>(After.Off), static_cast<double>(After.TurnedKmh), static_cast<double>(After.Seconds), static_cast<double>(Before.Seconds));
			ThrottleTurnedBefore += Before.ThrottleTurned;
			Jumps += Before.bWantJumped ? 1 : 0;
			CHECK(Before.bArrived && After.bArrived);
			// Never the throttle past what the wheels hold, no further from the line, and a second lost at most.
			CHECK(After.ThrottleTurned == 0.f);
			CHECK(After.Off < Before.Off + 0.2f);
			CHECK(After.TurnedKmh < 30.f);
			CHECK(After.Seconds < Before.Seconds + 1.f);
		}
	}
	// What gate 83 showed and the follower does: the speed wanted jumps as the lane is left, and the throttle comes
	// with the wheels turned.
	CHECK(Jumps >= 6);
	CHECK(ThrottleTurnedBefore > 1.f);
	{
		FGTPDriveSettings Settings;
		GTPDrive::SetChaseDriving(Settings);
		// A straight is not slowed: the firm throttle with the wheels straight, the whole of it when pulling away,
		// wheels turned or not.
		float Throttle = 0.2f, Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 0.02f, 20.f, 27.8f, Settings);
		CHECK(Throttle == 0.7f && Brake == 0.f);
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 1.f, 0.f, 27.8f, Settings);
		CHECK(Throttle == 1.f && Brake == 0.f);
		// The lines of gate 83. The quay at 98 km/h with 0.14 of lock asked: left alone.
		Throttle = 0.52f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, -0.14f, 98.f / 3.6f, 27.8f, Settings);
		CHECK(Throttle == 0.52f && Brake == 0.f);
		// Rue Childebert, kmh=36 throttle=1.00 steer=1.00, then kmh=52: no throttle, the brake.
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 1.f, 36.f / 3.6f, 16.7f, Settings);
		CHECK(Throttle == 0.f && Brake == 0.8f);
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 1.f, 52.f / 3.6f, 16.7f, Settings);
		CHECK(Throttle == 0.f && Brake == 0.8f);
		// Place de la Republique, kmh=37 throttle=1.00 steer=-0.41: the same.
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, -0.41f, 37.f / 3.6f, 12.5f, Settings);
		CHECK(Throttle == 0.f && Brake > 0.5f);
		// Under the speed held the throttle tapers: rue Jean de Tournes, 0.39 of lock, here at 20 km/h for the 27 held.
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 0.39f, 20.f / 3.6f, 16.7f, Settings);
		CHECK(Throttle > 0.7f && Throttle <= 1.f && Brake == 0.f);
		// The brake asked for an obstacle is never eased.
		Throttle = 0.f; Brake = 1.f;
		GTPFlightPedals::Apply(Throttle, Brake, 0.52f, 31.f / 3.6f, 20.8f, Settings);
		CHECK(Throttle == 0.f && Brake == 1.f);
		// Full lock is held at 15 km/h, a third of lock at 28.
		CHECK(std::fabs(GTPFlightPedals::HoldSpeed(1.f, Settings) * 3.6f - 15.2f) < 0.5f);
		CHECK(std::fabs(GTPFlightPedals::HoldSpeed(0.35f, Settings) * 3.6f - 28.3f) < 0.5f);
		// -DemoPedalsAlone: as before.
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 1.f, 10.f, 16.7f, Settings, false);
		CHECK(Throttle == 1.f && Brake == 0.f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
