// Checks what a police car does when its driving goes wrong (GTPUnitRecovery.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/unit_recovery_test.cpp -o /tmp/unit_recovery_test && /tmp/unit_recovery_test
#include "GTPUnitRecovery.h"

#include <cmath>
#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

static bool Near(float A, float B, float Tolerance = 0.01f) { return std::fabs(A - B) <= Tolerance; }

using EState = FGTPUnitRecovery::EState;
using EWhy = FGTPUnitRecovery::EWhy;

int main()
{
	const FGTPUnitRecoverySettings S;
	const float Dt = 1.f / 30.f;
	{
		// Gate 48, unit 769: 0 km/h on its four wheels with the throttle held. It backs off after 1.5 s,
		// for 1.8 s, wheels turned away from where it wanted to go (it steered right: back to the left).
		FGTPUnitRecovery R;
		float T = 0.f;
		while (R.Step(Dt, 0.1f, 1.f, 0.6f, 4, 1.f, 100.f, 50.f, S) == EState::Driving && T < 10.f)
		{
			T += Dt;
		}
		CHECK(R.State == EState::BackingOff && Near(T, S.PinnedSeconds, 0.1f));
		CHECK(Near(R.BackSteer, -1.f) && R.BackOffs == 1);
		T = 0.f;
		// While it backs off the car moves backwards and the driving gives no throttle.
		while (R.Step(Dt, -8.f, 0.f, 0.f, 4, 1.f, 99.f, 50.f, S) == EState::BackingOff && T < 10.f)
		{
			T += Dt;
		}
		CHECK(R.State == EState::Driving && Near(T, S.BackSeconds, 0.1f));
	}
	{
		// Held in the same place again and again: three back-offs, then the car is given up.
		FGTPUnitRecovery R;
		float T = 0.f;
		while (!R.IsGivenUp() && T < 60.f)
		{
			R.Step(Dt, 0.f, 0.6f, 0.f, 4, 1.f, 10.f, 10.f, S);
			T += Dt;
		}
		CHECK(R.IsGivenUp() && R.Why == EWhy::Pinned && R.BackOffs == S.MaxBackOffs);
		// 4 waits of 1.5 s and 3 back-offs of 1.8 s.
		CHECK(Near(T, 4.f * S.PinnedSeconds + 3.f * S.BackSeconds, 0.5f));
		// Given up for good.
		CHECK(R.Step(Dt, 40.f, 1.f, 0.f, 4, 1.f, 500.f, 10.f, S) == EState::GivenUp);
	}
	{
		// Going straight when held: one side, then the other.
		FGTPUnitRecovery R;
		float Sides[2] = {0.f, 0.f};
		int Seen = 0;
		for (int Frame = 0; Frame < 30 * 20 && Seen < 2; ++Frame)
		{
			const EState Before = R.State;
			if (R.Step(Dt, 0.f, 0.6f, 0.f, 4, 1.f, 0.f, 0.f, S) == EState::BackingOff && Before == EState::Driving)
			{
				Sides[Seen++] = R.BackSteer;
			}
		}
		CHECK(Seen == 2 && Near(Sides[0], -Sides[1]) && Near(std::fabs(Sides[0]), 1.f));
	}
	{
		// Backed off, got away 30 m, held again somewhere else: the count starts again.
		FGTPUnitRecovery R;
		for (int Frame = 0; Frame < 30 * 6; ++Frame)
		{
			R.Step(Dt, 0.f, 0.6f, 0.f, 4, 1.f, 0.f, 0.f, S);
		}
		CHECK(R.BackOffsHere == 2);
		for (int Frame = 0; Frame < 30 * 3; ++Frame)
		{
			R.Step(Dt, 40.f, 0.8f, 0.f, 4, 1.f, 30.f, 0.f, S);
		}
		CHECK(R.State == EState::Driving && R.BackOffsHere == 0);
		for (int Frame = 0; Frame < 30 * 2; ++Frame)
		{
			R.Step(Dt, 0.f, 0.6f, 0.f, 4, 1.f, 30.f, 0.f, S);
		}
		CHECK(R.State == EState::BackingOff && R.BackOffsHere == 1 && R.BackOffs == 3);
	}
	{
		// Stopped without the throttle (at a roadblock, behind a queue, braking): nothing to recover from.
		FGTPUnitRecovery R;
		for (int Frame = 0; Frame < 30 * 30; ++Frame)
		{
			R.Step(Dt, 0.f, 0.f, 0.f, 4, 1.f, 0.f, 0.f, S);
		}
		CHECK(R.State == EState::Driving && R.BackOffs == 0);
	}
	{
		// Gate 48, unit 989: no wheel on the ground, 0 km/h, wheels spinning for 20 s. Given up after 5 s.
		FGTPUnitRecovery R;
		float T = 0.f;
		while (!R.IsGivenUp() && T < 30.f)
		{
			R.Step(Dt, 0.f, 1.f, 0.f, 0, 0.9f, 0.f, 0.f, S);
			T += Dt;
		}
		CHECK(R.IsGivenUp() && R.Why == EWhy::OffWheels && Near(T, S.OffWheelsSeconds, 0.1f) && R.BackOffs == 0);
	}
	{
		// On its side with two wheels still touching: given up too.
		FGTPUnitRecovery R;
		for (int Frame = 0; Frame < 30 * 6; ++Frame)
		{
			R.Step(Dt, 1.f, 0.f, 0.f, 2, 0.1f, 0.f, 0.f, S);
		}
		CHECK(R.IsGivenUp() && R.Why == EWhy::OffWheels);
	}
	{
		// On its roof for 2.5 s, then the vehicle's own rule rights it: 0.5 s falling with no wheel on the
		// ground, then on its four wheels. Not given up: it drives on.
		FGTPUnitRecovery R;
		for (int Frame = 0; Frame < 75; ++Frame)
		{
			R.Step(Dt, 0.f, 0.f, 0.f, 0, -0.9f, 0.f, 0.f, S);
		}
		for (int Frame = 0; Frame < 15; ++Frame)
		{
			R.Step(Dt, 0.f, 0.f, 0.f, 0, 1.f, 0.f, 0.f, S);
		}
		for (int Frame = 0; Frame < 30 * 4; ++Frame)
		{
			R.Step(Dt, 20.f, 0.6f, 0.f, 4, 1.f, 0.f, 0.f, S);
		}
		CHECK(R.State == EState::Driving && Near(R.OffWheelsTime, 0.f));
	}
	{
		// A car that has just been made falls 45 cm with no wheel on the ground for half a second,
		// and a car in the air over a bump is moving: neither is given up.
		FGTPUnitRecovery R;
		for (int Frame = 0; Frame < 15; ++Frame)
		{
			R.Step(Dt, 0.f, 0.f, 0.f, 0, 1.f, 0.f, 0.f, S);
		}
		for (int Frame = 0; Frame < 30 * 5; ++Frame)
		{
			R.Step(Dt, 60.f, 1.f, 0.f, Frame % 60 < 30 ? 0 : 4, 1.f, static_cast<float>(Frame), 0.f, S);
		}
		CHECK(R.State == EState::Driving);
	}
	{
		// The pedals follow the steering. Full lock at 6 m/s2: 4.2 m/s (15 km/h), the radius is 2.97 m.
		CHECK(Near(GTPUnitRecovery::SteerSpeed(1.f, 6.f, S), std::sqrt(6.f * 2.97f / std::tan(0.785f)), 0.01f));
		CHECK(GTPUnitRecovery::SteerSpeed(0.f, 6.f, S) > 500.f);
		// Gate 48: full lock, throttle 1.00, 38 km/h: two wheels in the air. Now no throttle and the brake.
		float Throttle = 1.f, Brake = 0.f, Steering = 1.f;
		GTPUnitRecovery::Tame(Throttle, Brake, Steering, 38.f / 3.6f, 6.f, S);
		CHECK(Near(Throttle, 0.f) && Brake > 0.5f);
		// And the steering does not ask for more than 7.5 m/s2 at that speed (a 14.9 m circle, 11 degrees).
		CHECK(Near(Steering, std::atan(2.97f * 7.5f / (10.556f * 10.556f)) / 0.785f, 0.01f) && Steering < 0.26f);
		// To the left the same.
		Throttle = 1.f; Brake = 0.f; Steering = -1.f;
		GTPUnitRecovery::Tame(Throttle, Brake, Steering, 38.f / 3.6f, 6.f, S);
		CHECK(Steering < 0.f && Steering > -0.26f);
		// A U-turn from a standstill: full lock allowed, throttle held to what the tyres take, then eased near 15 km/h.
		Throttle = 1.f; Brake = 0.f; Steering = 1.f;
		GTPUnitRecovery::Tame(Throttle, Brake, Steering, 0.f, 6.f, S);
		CHECK(Near(Steering, 1.f) && Near(Throttle, S.LaunchThrottle) && Near(Brake, 0.f));
		Throttle = 1.f; Brake = 0.f; Steering = 1.f;
		GTPUnitRecovery::Tame(Throttle, Brake, Steering, 4.f, 6.f, S);
		CHECK(Throttle > 0.1f && Throttle < 0.3f && Near(Brake, 0.f) && Near(Steering, 1.f));
		// Straight at 90 km/h: nothing is changed.
		Throttle = 0.9f; Brake = 0.f; Steering = 0.f;
		GTPUnitRecovery::Tame(Throttle, Brake, Steering, 25.f, 6.f, S);
		CHECK(Near(Throttle, 0.9f) && Near(Brake, 0.f) && Near(Steering, 0.f));
		// A bend planned at 6 m/s2 (radius 30 m, 13.4 m/s): the steering it needs passes, the throttle is only eased.
		const float Needed = std::atan(2.97f / 30.f) / 0.785f;
		Throttle = 0.8f; Brake = 0.f; Steering = Needed;
		GTPUnitRecovery::Tame(Throttle, Brake, Steering, 13.f, 6.f, S);
		CHECK(Near(Steering, Needed, 0.001f) && Throttle > 0.1f && Near(Brake, 0.f));
		// A brake already asked for is never eased.
		Throttle = 0.f; Brake = 1.f; Steering = 0.5f;
		GTPUnitRecovery::Tame(Throttle, Brake, Steering, 20.f, 6.f, S);
		CHECK(Near(Brake, 1.f));
	}
	if (Failures == 0)
	{
		std::printf("unit_recovery_test: ok\n");
	}
	return Failures == 0 ? 0 : 1;
}
