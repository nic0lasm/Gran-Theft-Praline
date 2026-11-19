// Drives a car round a right angle with the chase settings (GTPDriveLogic.h, GTPUnitRecovery.h): it must reach
// the corner at the corner's speed and stay on its lane, whatever the spacing of the lane's points and the brakes.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/corner_drive_test.cpp -o /tmp/corner_drive_test && /tmp/corner_drive_test
//
// The car is a bicycle model, not the game's: 2.97 m between the axles, 45 degrees of lock reached at 2.5 lock/s,
// pedals that answer in 0.2 s, 4 m/s2 of drive. It says how the decisions behave, not how the tyres do.
#include "GTPDriveLogic.h"
#include "GTPUnitRecovery.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

namespace
{
	constexpr float Corner = 120.f;

	struct FRun
	{
		float CornerKmh = -1.f;
		float Inside = 0.f;
		float Outside = 0.f;
		float Lateral = 0.f;
		float Seconds = 0.f;
	};

	/** Metres from the path: a straight along X to (Corner, 0), then along Y. */
	float Stray(float X, float Y)
	{
		const float First = X <= Corner ? std::fabs(Y) : std::hypot(X - Corner, Y);
		const float Second = Y >= 0.f ? std::fabs(X - Corner) : std::hypot(X - Corner, Y);
		return std::min(First, Second);
	}

	FRun Drive(const FGTPDriveSettings& S, float Spacing, float BrakeMax, bool bTame)
	{
		const FGTPUnitRecoverySettings Recovery;
		std::vector<FGTPDrivePoint> Path;
		for (float X = 0.f; X < Corner - 0.01f; X += Spacing)
		{
			Path.push_back({X, 0.f});
		}
		Path.push_back({Corner, 0.f});
		for (float Y = Spacing; Y <= Corner + 0.01f; Y += Spacing)
		{
			Path.push_back({Corner, Y});
		}
		float X = 5.f, Y = 0.f, Yaw = 0.f, V = 22.f, Accel = 0.f, Steer = 0.f;
		int Closest = 0;
		FRun Run;
		const float Dt = 1.f / 60.f;
		while (Y < 60.f && Run.Seconds < 60.f)
		{
			const FGTPDriveCommand Command = GTPDrive::Decide(Path, X, Y, Yaw, V, 25.f, -1.f, S, Closest);
			float Throttle = Command.Throttle, Brake = Command.Brake, Steering = Command.Steering;
			if (bTame)
			{
				GTPUnitRecovery::Tame(Throttle, Brake, Steering, V, S.MaxLateralAccel, Recovery);
			}
			const float Wanted = Throttle * 4.f - Brake * BrakeMax - 0.3f;
			Accel += (Wanted - Accel) * std::min(1.f, Dt / 0.2f);
			Steer += std::max(-Dt * 2.5f, std::min(Dt * 2.5f, Steering - Steer));
			V = std::max(0.f, V + Accel * Dt);
			const float YawRate = V * std::tan(Steer * 0.785f) / 2.97f;
			Run.Lateral = std::max(Run.Lateral, std::fabs(V * YawRate));
			Yaw += YawRate * Dt;
			X += V * std::cos(Yaw) * Dt;
			Y += V * std::sin(Yaw) * Dt;
			Run.Seconds += Dt;
			if (Run.CornerKmh < 0.f && X >= Corner - 2.f)
			{
				Run.CornerKmh = V * 3.6f;
			}
			const float Off = Stray(X, Y);
			if (X < Corner && Y > 0.f)
			{
				Run.Inside = std::max(Run.Inside, Off);
			}
			else
			{
				Run.Outside = std::max(Run.Outside, Off);
			}
		}
		return Run;
	}
}

int main()
{
	FGTPDriveSettings Chase;
	GTPDrive::SetChaseDriving(Chase);
	// The corner is worth sqrt(6 x 3.8) = 4.8 m/s, 17 km/h.
	for (const float Spacing : {1.f, 5.f, 10.f, 20.f, 40.f})
	{
		for (const float BrakeMax : {6.f, 9.f, 12.f})
		{
			for (const bool bTame : {true, false})
			{
				const FRun Run = Drive(Chase, Spacing, BrakeMax, bTame);
				std::printf("points %2.0f m apart, brakes %2.0f m/s2%s: %2.0f km/h at the corner, %.1f m inside, %.1f m outside, %.1f m/s2 sideways, %.1f s\n",
					static_cast<double>(Spacing), static_cast<double>(BrakeMax), bTame ? ", police" : ", demo  ", static_cast<double>(Run.CornerKmh),
					static_cast<double>(Run.Inside), static_cast<double>(Run.Outside), static_cast<double>(Run.Lateral), static_cast<double>(Run.Seconds));
				CHECK(Run.CornerKmh > 12.f && Run.CornerKmh < 27.f);
				CHECK(Run.Inside < 1.8f && Run.Outside < 1.2f);
				CHECK(Run.Seconds < 13.f);
				if (bTame)
				{
					CHECK(Run.Lateral < 6.f * 1.25f + 0.1f);
				}
			}
		}
	}
	// What the 49 drove with: 7 m/s2 planned, the brake at 0.4 of the error, the point aimed at 5 m + 0.6 s away.
	// With brakes of 6 m/s2 the car is still at 50 km/h at the corner and leaves the lane by metres.
	{
		FGTPDriveSettings Before;
		Before.MaxLateralAccel = 6.f;
		Before.ComfortDecel = 7.f;
		Before.ThrottleGain = 0.8f;
		const FRun Run = Drive(Before, 20.f, 6.f, true);
		CHECK(Run.CornerKmh > 40.f && Run.Outside > 3.f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
