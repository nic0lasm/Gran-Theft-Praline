// Checks of the car-following law (IDM) in GTPDriveLogic.h.
// g++ -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public idm_test.cpp

#include "GTPDriveLogic.h"

#include <cstdio>

static int Failures = 0;
static int Checks = 0;

static void Check(bool bOk, const char* What)
{
	++Checks;
	if (!bOk)
	{
		++Failures;
		std::printf("FAIL: %s\n", What);
	}
}

int main()
{
	const FGTPDriveSettings S;
	// Free road: speeds up towards the limit, never past it.
	Check(GTPDrive::IdmSpeed(5.f, 13.9f, 200.f, 13.9f, S) > 5.f, "free road: speeds up");
	Check(GTPDrive::IdmSpeed(13.9f, 13.9f, 200.f, 13.9f, S) <= 13.9f, "never past the limit");
	// Close behind a slower car: slows down.
	Check(GTPDrive::IdmSpeed(13.9f, 13.9f, 8.f, 5.f, S) < 13.f, "closing on a slower car: slows");
	// At the time gap behind a car going the same speed: keeps about that speed.
	{
		const float V = 10.f;
		const float Gap = S.SafeGap + V * S.IdmTimeGap;
		const float Next = GTPDrive::IdmSpeed(V, 13.9f, Gap, V, S);
		Check(Next > 8.5f && Next < 11.f, "at the time gap: holds the speed");
	}
	// A queue moving off: the follower starts as soon as the leader moves, not when it has gone.
	{
		const float Next = GTPDrive::IdmSpeed(0.f, 13.9f, 6.f, 2.f, S);
		Check(Next > 0.f, "queue moving off: the follower moves too");
	}
	// Simulation: a follower behind a leader at 8 m/s settles behind it without touching.
	{
		float Lx = 30.f, Fx = 0.f, Fv = 13.f;
		const float Lv = 8.f, Dt = 0.05f;
		float MinGap = 1e9f;
		for (int Step = 0; Step < 1200; ++Step)
		{
			const float Gap = Lx - Fx - 4.5f;
			MinGap = Gap < MinGap ? Gap : MinGap;
			const float Want = GTPDrive::IdmSpeed(Fv, 13.9f, Gap, Lv, S);
			// A simple car: reaches the wanted speed at the comfort rates.
			const float Dv = Want - Fv;
			Fv += Dv > 0.f ? (Dv < S.IdmMaxAccel * Dt ? Dv : S.IdmMaxAccel * Dt) : (Dv > -S.ComfortDecel * 2.f * Dt ? Dv : -S.ComfortDecel * 2.f * Dt);
			Fx += Fv * Dt;
			Lx += Lv * Dt;
		}
		const float Gap = Lx - Fx - 4.5f;
		Check(MinGap > 2.f, "follower never touches the leader");
		Check(Fv > 7.f && Fv < 9.f, "follower settles at the leader's speed");
		Check(Gap > S.SafeGap && Gap < S.SafeGap + 8.f * S.IdmTimeGap + 6.f, "follower settles near the time gap");
	}
	std::printf("%d checks, %d failures\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
