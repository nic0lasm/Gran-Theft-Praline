// Checks of GTPTrafficReactLogic.h outside Unreal: g++ -std=c++17 -I <Public> react_test.cpp
// Each scenario plays a few seconds of one driver at 30 steps a second.
#include "GTPTrafficReactLogic.h"
#include "GTPHornSynth.h"

#include <cmath>
#include <cstdio>

using namespace GTPTrafficReact;

static int Failures = 0;

static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	Failures += bOk ? 0 : 1;
}

struct FRun
{
	FBrain Brain;
	FSettings Settings;
	int Horn = 0;
	int BailOuts = 0;
	float MaxOffset = 0.f;
	FOutput Last;

	void Play(float Seconds, FInput In)
	{
		In.Dt = 1.f / 30.f;
		const int Steps = static_cast<int>(Seconds * 30.f + 0.5f);
		for (int Index = 0; Index < Steps; ++Index)
		{
			const bool bWasHorn = Last.bHorn;
			Last = Step(Brain, In, Settings);
			Horn += Last.bHorn && !bWasHorn ? 1 : 0;
			BailOuts += Last.bBailOut ? 1 : 0;
			MaxOffset = Last.LateralOffset > MaxOffset ? Last.LateralOffset : MaxOffset;
		}
	}
};

static FInput Blocked(bool bPlayer, bool bSideClear)
{
	FInput In;
	In.ObstacleM = 6.f;
	In.bObstacleStill = true;
	In.bObstacleIsPlayer = bPlayer;
	In.bSideClear = bSideClear;
	return In;
}

int main()
{
	{
		// A queue at a red light: the car ahead is not still, nobody honks.
		FRun Run;
		FInput In;
		In.ObstacleM = 5.f;
		Run.Play(20.f, In);
		Check(Run.Brain.State == EState::Cruise && Run.Horn == 0, "waits at a red light without a horn");
	}
	{
		// A parked car across the lane, oncoming traffic on the side: horn, no bypass.
		FRun Run;
		Run.Play(2.f, Blocked(false, false));
		Check(Run.Brain.State == EState::Waiting && Run.Horn == 0, "waits quietly for the first seconds");
		Run.Play(10.f, Blocked(false, false));
		Check(Run.Horn >= 3 && Run.Horn <= 5, "honks again every few seconds");
		Check(Run.Brain.State == EState::Waiting && Run.MaxOffset == 0.f, "does not go round while the side is busy");
	}
	{
		// The player stands in front: horn after one second.
		FRun Run;
		Run.Play(1.3f, Blocked(true, false));
		Check(Run.Horn == 1, "honks at the player within a second and a half");
	}
	{
		// Side clear: goes round after BypassAfter, comes back to the lane.
		FRun Run;
		Run.Play(4.5f, Blocked(false, true));
		Check(Run.Brain.State == EState::Waiting, "waits before going round");
		Run.Play(1.f, Blocked(false, true));
		Check(Run.Brain.State == EState::Bypass, "goes round a still obstacle once the side is clear");
		FInput Passing = Blocked(false, true);
		Passing.Speed = 3.f;
		Run.Play(3.f, Passing);
		Check(Run.Last.LateralOffset > 2.8f && Run.Last.bIgnoreBlocker && Run.Last.SpeedScale < 0.5f, "moves a lane width aside, slowly");
		FInput Clear;
		Clear.Speed = 5.f;
		Run.Play(2.2f, Clear);
		Check(Run.Brain.State == EState::Cruise, "back to cruising once past");
		Run.Play(3.f, Clear);
		Check(Run.Last.LateralOffset == 0.f, "slides back into its lane");
	}
	{
		// The blocker drives off: back to cruising, no bypass.
		FRun Run;
		Run.Play(3.f, Blocked(false, true));
		FInput Gone;
		Run.Play(0.5f, Gone);
		Check(Run.Brain.State == EState::Cruise && Run.Brain.BlockedTime == 0.f, "carries on when the blocker leaves");
	}
	{
		// Gun pointed at a stopped driver: gets out once and runs.
		FRun Run;
		FInput In;
		In.bAimedAt = true;
		Run.Play(0.8f, In);
		Check(Run.BailOuts == 0, "a short aim is not enough to leave the car");
		Run.Play(1.f, In);
		Check(Run.BailOuts == 1 && Run.Brain.State == EState::Flee && Run.Last.bHold, "aimed at for a second: gets out and runs");
		Run.Play(5.f, In);
		Check(Run.BailOuts == 1, "gets out only once");
	}
	{
		// Gun pointed at a moving driver: floors it, ignores the lights, then calms down.
		FRun Run;
		FInput In;
		In.Speed = 10.f;
		In.bAimedAt = true;
		Run.Play(0.2f, In);
		Check(Run.Brain.State == EState::Panic && Run.Last.SpeedScale > 1.5f && Run.Last.bIgnoreSignals && Run.Horn == 1, "a moving driver aimed at floors it with the horn");
		FInput Calm;
		Calm.Speed = 14.f;
		Run.Play(8.5f, Calm);
		Check(Run.Brain.State == EState::Cruise && !Run.Last.bIgnoreSignals, "calms down after the panic");
	}
	{
		// Shots nearby keep the panic going.
		FRun Run;
		FInput Shot;
		Shot.Speed = 8.f;
		Shot.bShotNear = true;
		Run.Play(0.1f, Shot);
		FInput Quiet;
		Quiet.Speed = 8.f;
		Run.Play(6.f, Quiet);
		Run.Play(0.1f, Shot);
		Run.Play(6.f, Quiet);
		Check(Run.Brain.State == EState::Panic, "a new shot extends the panic");
	}
	{
		// Rammed by the player: one long blast, stays put, drives on.
		FRun Run;
		FInput Hit;
		Hit.Speed = 6.f;
		Hit.bHitByPlayer = true;
		Run.Play(1.f / 30.f, Hit);
		Check(Run.Brain.State == EState::Shaken && Run.Last.bHold && Run.Horn == 1, "rammed by the player: horn and stop");
		FInput After;
		Run.Play(3.f, After);
		Check(Run.Brain.State == EState::Cruise && Run.Horn == 1, "drives on after the shock");
	}
	{
		// Every state has a name for the logs.
		Check(StateName(EState::Bypass)[0] == 'b' && StateName(EState::Flee)[0] == 'f', "state names for the logs");
	}
	{
		// Horn synth: silent off, loud and bounded on, quiet again after the release.
		FGTPHornSynth Horn;
		Horn.Init(48000, FGTPHornSettings());
		static float Buffer[4800];
		Horn.Render(Buffer, 4800);
		float Peak = 0.f;
		for (float Sample : Buffer) { Peak = std::fabs(Sample) > Peak ? std::fabs(Sample) : Peak; }
		Check(Peak < 1e-4f, "horn silent when off");
		Horn.SetOn(true);
		Horn.Render(Buffer, 4800);
		Peak = 0.f;
		double Energy = 0.0;
		for (float Sample : Buffer)
		{
			Peak = std::fabs(Sample) > Peak ? std::fabs(Sample) : Peak;
			Energy += static_cast<double>(Sample) * static_cast<double>(Sample);
		}
		Check(Peak > 0.2f && Peak <= 1.f && Energy / 4800.0 > 0.01, "horn loud and bounded when on");
		Horn.SetOn(false);
		Horn.Render(Buffer, 4800);
		Horn.Render(Buffer, 4800);
		Horn.Render(Buffer, 4800);
		Peak = 0.f;
		for (int Index = 4000; Index < 4800; ++Index) { Peak = std::fabs(Buffer[Index]) > Peak ? std::fabs(Buffer[Index]) : Peak; }
		Check(Peak < 0.01f, "horn fades out after release");
	}
	std::printf("%d failure(s)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
