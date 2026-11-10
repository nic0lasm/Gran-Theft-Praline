// Checks of GTPTrafficGhost.h outside Unreal: g++ -std=c++17 -I <Public> ghost_test.cpp
#include "GTPTrafficGhost.h"

#include <cstdio>
#include <cstdlib>

static int Failures = 0;

static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	Failures += bOk ? 0 : 1;
}

static bool Near(float A, float B, float Tolerance = 1e-3f) { return std::fabs(A - B) <= Tolerance; }

int main()
{
	// An L: 10 m east, then 10 m north.
	const std::vector<FGTPDrivePoint> Path = {{0.f, 0.f}, {10.f, 0.f}, {10.f, 10.f}};
	int Index = 0;
	float Along = 0.f;
	FGTPDrivePoint P;
	float Yaw = 0.f;

	Check(GTPGhost::Advance(Path, Index, Along, 4.f, P, Yaw) && Near(P.X, 4.f) && Near(P.Y, 0.f) && Near(Yaw, 0.f), "moves along the first segment");
	Check(GTPGhost::Advance(Path, Index, Along, 8.f, P, Yaw) && Index == 1 && Near(P.X, 10.f) && Near(P.Y, 2.f) && Near(Yaw, 1.5707963f), "turns onto the next segment");
	Check(!GTPGhost::Advance(Path, Index, Along, 50.f, P, Yaw) && Near(P.X, 10.f) && Near(P.Y, 10.f), "stops at the end of the path");
	Index = 0;
	Along = 0.f;
	Check(GTPGhost::Advance(Path, Index, Along, 0.f, P, Yaw) && Near(P.X, 0.f), "a stopped car stays put");

	// Speed: 2 m/s² up, 6 m/s² down, never negative.
	Check(Near(GTPGhost::NextSpeed(0.f, 10.f, 2.f, 6.f, 0.5f), 1.f), "accelerates gently");
	Check(Near(GTPGhost::NextSpeed(9.5f, 10.f, 2.f, 6.f, 0.5f), 10.f), "does not overshoot the wanted speed");
	Check(Near(GTPGhost::NextSpeed(10.f, 0.f, 2.f, 6.f, 0.5f), 7.f), "brakes firmly");
	Check(Near(GTPGhost::NextSpeed(1.f, 0.f, 2.f, 6.f, 0.5f), 0.f), "never goes backwards");

	// Hysteresis: ghost past 90 m, awake again inside 70 m.
	Check(!GTPGhost::WantsGhost(false, 80.f, 90.f, 70.f), "a driven car at 80 m stays driven");
	Check(GTPGhost::WantsGhost(false, 95.f, 90.f, 70.f), "a driven car at 95 m turns ghost");
	Check(GTPGhost::WantsGhost(true, 80.f, 90.f, 70.f), "a ghost at 80 m stays a ghost");
	Check(!GTPGhost::WantsGhost(true, 65.f, 90.f, 70.f), "a ghost at 65 m wakes up");

	// A queue of ghosts behind a stopped car keeps its gap (same rule as GTPDrive::Decide).
	FGTPDriveSettings Settings;
	const std::vector<FGTPDrivePoint> Straight = {{0.f, 0.f}, {200.f, 0.f}};
	float Speed = 12.f;
	float X = 0.f;
	Index = 0;
	Along = 0.f;
	const float StoppedCarAt = 60.f;
	for (int Step = 0; Step < 400; ++Step)
	{
		int Closest = Index;
		const float Gap = StoppedCarAt - X;
		const FGTPDriveCommand Command = GTPDrive::Decide(Straight, X, 0.f, 0.f, Speed, 13.9f, Gap, Settings, Closest);
		Speed = GTPGhost::NextSpeed(Speed, Command.TargetSpeed, 2.f, 6.f, 0.05f);
		GTPGhost::Advance(Straight, Index, Along, Speed * 0.05f, P, Yaw);
		X = P.X;
	}
	Check(X < StoppedCarAt - 2.f && X > StoppedCarAt - Settings.SafeGap - 3.f && Speed < 0.1f, "stops behind a stopped car, gap kept");
	std::printf("final gap %.2f m, speed %.2f m/s\n", StoppedCarAt - X, Speed);

	return Failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
