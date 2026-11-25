// The three corners of the Bellecour circuit where gate 83's car left its lane, and a car driven through them the
// way the game drives them. For flight_pedals_test.cpp and lane_change_corner_test.cpp.
//
// The lanes are the circuit's own lines (L_City_RueDeLaRepublique.circuit.csv, in metres from the first point of
// each), with the speed asked on each. The car is a bicycle model, not the game's: 2.97 m between the axles,
// 45 degrees of lock reached at 2.5 lock/s, pedals that answer in 0.2 s, 4.5 m/s2 of drive (gate 83's lines: 36 to
// 52 km/h in a second with the wheels turned), 8 m/s2 of brakes, and no more than 8 m/s2 sideways.
// Its front wheels get the steering asked (ECar::Model, what the tests of gates 83 to 89 were written with), or what
// the game's car makes of the steering the flight sends it (ECar::Gate93, ECar::Game: steering_input_test.cpp).
#pragma once

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
		float WantSum = 0.f;          // the speeds wanted, added up frame after frame: two runs alike have the same
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

	/** What the driver does with a lane as it leaves it. */
	enum class ELeave
	{
		Drop,       // the lane goes whole: FGTPLaneFollower::Step until gate 83
		KeepEnd,    // its last metres stay at the head of the path (GTPDrive::KeptBehind): FGTPLaneFollower::Step since
		Never,      // nothing ever goes: one path from the first point to the last
	};

	/** What the front wheels of the car get of the steering its driver asks. */
	enum class ECar
	{
		Model,     // the steering asked
		Gate93,    // the game's car (GTPSteeringCurve::WheelsGet) sent the steering of gates 93 to 99 (GTPFlightPedals::SteeringShareMadeUp)
		Game,      // the game's car sent the steering of the flight (GTPFlightPedals::SteeringSent)
	};

	/** From a standstill on the first lane, FromPoint points into it (the script puts the car back on a lane at rest). */
	FRun Drive(const FCorner& Corner, int FromPoint, bool bFollowSteering, ELeave Leave, bool bTrace = false, ECar Car = ECar::Model)
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
		// Points of the path up to the end of the lane the car is on.
		int LaneLength = static_cast<int>(First.size());
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
			if (Closest >= LaneLength && LaneIndex + 1 < Corner.Lanes.size())
			{
				// FGTPLaneFollower::Step and Plan: the path is what is kept of the lane left, then the lanes ahead.
				if (Leave != ELeave::Never)
				{
					std::vector<FGTPDrivePoint> Kept = Leave == ELeave::KeepEnd ? GTPDrive::KeptBehind(Path, LaneLength) : std::vector<FGTPDrivePoint>();
					Closest += static_cast<int>(Kept.size()) - LaneLength;
					Path = Kept;
					for (size_t Ahead = LaneIndex + 1; Ahead < Corner.Lanes.size(); ++Ahead)
					{
						Path.insert(Path.end(), Corner.Lanes[Ahead].Line.begin(), Corner.Lanes[Ahead].Line.end());
					}
					LaneLength = static_cast<int>(Kept.size());
				}
				++LaneIndex;
				LaneLength += static_cast<int>(Corner.Lanes[LaneIndex].Line.size());
				WantBefore = Command.TargetSpeed;
				continue;
			}
			if (WantBefore > 0.f)
			{
				Run.bWantJumped = Run.bWantJumped || Command.TargetSpeed > WantBefore * 1.5f;
				WantBefore = -1.f;
			}
			Run.WantSum += Command.TargetSpeed;
			const float Asked = Command.Steering;
			GTPFlightPedals::Apply(Command.Throttle, Command.Brake, Command.Steering, V, Cruise, Settings, bFollowSteering);
			CHECK(Command.Steering == Asked);
			if (std::fabs(Asked) > 0.5f && Command.Throttle > 0.5f && V > GTPFlightPedals::HoldSpeed(Asked, Settings))
			{
				Run.ThrottleTurned += Dt;
			}
			const float Wanted = Command.Throttle * DriveMost - Command.Brake * BrakesMost - 0.3f;
			Accel += (Wanted - Accel) * std::min(1.f, Dt / 0.2f);
			float Lock = 0.f;
			if (Car == ECar::Model)
			{
				Steer += std::max(-Dt * 2.5f, std::min(Dt * 2.5f, Command.Steering - Steer));
				Lock = Steer;
			}
			else
			{
				// The steering input of the game's car: 2.5 a second towards more lock, 5 a second back.
				const float Sent = Car == ECar::Gate93 ? GTPFlightPedals::SteeringShareMadeUp(Command.Steering, V * 3.6f) : GTPFlightPedals::SteeringSent(Command.Steering, V * 3.6f);
				const float Rate = (std::fabs(Sent) > std::fabs(Steer) && Sent * Steer >= 0.f ? 2.5f : 5.f) * Dt;
				Steer += std::max(-Rate, std::min(Rate, Sent - Steer));
				Lock = GTPSteeringCurve::WheelsGet(Steer, V * 3.6f);
			}
			V = std::max(0.f, V + Accel * Dt);
			float YawRate = V * std::tan(Lock * 0.785f) / 2.97f;
			if (std::fabs(V * YawRate) > SidewaysMost)
			{
				YawRate *= SidewaysMost / std::fabs(V * YawRate);
			}
			Yaw += YawRate * Dt;
			X += V * std::cos(Yaw) * Dt;
			Y += V * std::sin(Yaw) * Dt;
			Run.Seconds += Dt;
			Run.Off = std::max(Run.Off, Stray(Whole, X, Y));
			if (std::fabs(Lock) > 0.5f)
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

