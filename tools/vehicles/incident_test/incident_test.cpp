// Checks of GTPIncidentLogic.h (crashes between traffic cars, drivers arguing).
// g++ -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public incident_test.cpp

#include "GTPIncidentLogic.h"

#include <cstdio>

using namespace GTPIncident;

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

static FArguerOutput Run(FArguer& A, FArguerInput In, const FSettings& S, float Seconds)
{
	FArguerOutput Last;
	for (float T = 0.f; T < Seconds; T += In.Dt)
	{
		Last = StepArguer(A, In, S);
		if (Last.bReclaim || Last.bStartFleeing)
		{
			break;
		}
	}
	return Last;
}

int main()
{
	const FSettings S;

	// Outcomes.
	Check(PickOutcome(0.9f, 300.f, false, S) == EOutcome::DriveOn, "a bump: drive on");
	Check(PickOutcome(0.1f, 300.f, false, S) == EOutcome::Argue, "some bumps: argue");
	Check(PickOutcome(0.1f, 1000.f, false, S) == EOutcome::HitAndRun, "low roll on a crash: hit and run");
	Check(PickOutcome(0.5f, 1000.f, false, S) == EOutcome::Argue, "a crash, nobody timid: argue");
	Check(PickOutcome(0.5f, 1000.f, true, S) == EOutcome::FleeOnFoot, "a crash, a timid driver: runs");
	Check(PickOutcome(0.95f, 1000.f, true, S) == EOutcome::Argue, "a timid driver may still argue");
	Check(PickOutcome(0.5f, 3000.f, false, S) == EOutcome::FleeOnFoot, "a violent crash: everyone runs");

	// A whole argument: out, walk up, argue, walk back, reclaim.
	{
		FArguer A;
		A.ArgueSeconds = 6.f;
		FArguerInput In;
		In.Dt = 0.1f;
		In.bOut = false;
		In.bOtherHere = true;
		In.OtherDistance = 600.f;
		In.CarDistance = 100.f;
		StepArguer(A, In, S);
		Check(A.Phase == EPhase::GetOut, "waits to be out");
		In.bOut = true;
		StepArguer(A, In, S);
		Check(A.Phase == EPhase::Approach, "walks up to the other");
		const FArguerOutput Walk = StepArguer(A, In, S);
		Check(Walk.bMoveToOther, "moves to the other");
		In.OtherDistance = 120.f;
		StepArguer(A, In, S);
		Check(A.Phase == EPhase::Argue, "argues once close");
		int Shoves = 0;
		In.ShoveRoll = 0.f;
		for (int I = 0; I < 50; ++I)
		{
			Shoves += StepArguer(A, In, S).bShove ? 1 : 0;
		}
		Check(Shoves >= 1 && Shoves <= 3, "a shove now and then");
		In.CarDistance = 600.f;
		Run(A, In, S, 1.5f);
		Check(A.Phase == EPhase::Return, "goes back after the argument");
		Check(StepArguer(A, In, S).bMoveToCar, "walks back to the car");
		In.CarDistance = 200.f;
		Check(StepArguer(A, In, S).bReclaim && A.Phase == EPhase::Done, "back in the car");
	}

	// Threat: runs at once, whatever the phase.
	{
		FArguer A;
		FArguerInput In;
		In.Dt = 0.1f;
		In.bOut = true;
		In.bOtherHere = true;
		In.OtherDistance = 100.f;
		StepArguer(A, In, S);
		StepArguer(A, In, S);
		In.bThreat = true;
		Check(StepArguer(A, In, S).bStartFleeing && A.Phase == EPhase::Flee, "a gun: runs");
		Check(!StepArguer(A, In, S).bStartFleeing, "flees once");
	}

	// The car stolen while arguing: runs.
	{
		FArguer A;
		FArguerInput In;
		In.Dt = 0.1f;
		In.bOut = true;
		StepArguer(A, In, S);
		Check(A.Phase == EPhase::Argue, "alone: shouts where they stand");
		In.bCarGone = true;
		Check(StepArguer(A, In, S).bStartFleeing, "car stolen: runs");
	}

	// Alone, the shouting is shorter; boxed in, never gets out.
	{
		FArguer A;
		A.ArgueSeconds = 10.f;
		FArguerInput In;
		In.Dt = 0.1f;
		In.bOut = true;
		In.CarDistance = 400.f;
		StepArguer(A, In, S);
		Run(A, In, S, 4.5f);
		Check(A.Phase == EPhase::Return, "shouts after a hit and run, then goes back");
		FArguer B;
		In.bOut = false;
		Run(B, In, S, S.GetOutSeconds + 0.5f);
		Check(B.Phase == EPhase::Done, "boxed in: stays in the car");
	}

	// Lost on the way back: walks off.
	{
		FArguer A;
		A.Phase = EPhase::Return;
		FArguerInput In;
		In.Dt = 0.1f;
		In.bOut = true;
		In.CarDistance = 5000.f;
		const FArguerOutput Out = Run(A, In, S, S.WalkSeconds + 1.f);
		Check(Out.bStartFleeing, "cannot reach the car: walks off");
	}

	std::printf("%d checks, %d failures\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
