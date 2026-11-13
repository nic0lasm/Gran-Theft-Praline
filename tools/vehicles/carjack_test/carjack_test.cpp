// Checks of GTPCarjackLogic.h and of the driver temperaments of GTPTrafficReactLogic.h,
// outside Unreal: g++ -std=c++17 -I <Public> carjack_test.cpp
#include "GTPCarjackLogic.h"
#include "GTPTrafficReactLogic.h"

#include <cstdio>

static int Failures = 0;

static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	Failures += bOk ? 0 : 1;
}

using namespace GTPCarjack;

struct FPlay
{
	FVictim V;
	FSettings S;
	int Strikes = 0;
	int Moves = 0;
	bool bFled = false;

	void Run(float Seconds, FVictimInput In)
	{
		In.Dt = 0.1f;
		const int Steps = static_cast<int>(Seconds * 10.f + 0.5f);
		for (int Index = 0; Index < Steps; ++Index)
		{
			const FVictimOutput Out = StepVictim(V, In, S);
			Strikes += Out.bStrike ? 1 : 0;
			Moves += Out.bMoveToTarget ? 1 : 0;
			bFled |= Out.bStartFleeing;
		}
	}
};

int main()
{
	{
		FSettings S;
		Check(PickMood(0.1f, S) == EMood::Resist && PickMood(0.35f, S) == EMood::Cower && PickMood(0.9f, S) == EMood::Flee, "mood shares");
	}
	{
		FPlay P;
		P.Run(0.5f, FVictimInput{});
		Check(!P.bFled && P.V.Phase == EPhase::Down, "a fleeing driver stays down a moment");
		P.Run(1.f, FVictimInput{});
		Check(P.bFled, "then runs");
	}
	{
		FPlay P;
		P.V.Mood = EMood::Resist;
		FVictimInput Far;
		Far.TargetDistance = 600.f;
		P.Run(2.f, Far);
		Check(P.Moves > 0 && P.Strikes == 0, "a resisting driver walks back to the thief");
		FVictimInput Near;
		Near.TargetDistance = 100.f;
		P.Run(3.f, Near);
		Check(P.Strikes >= 2 && P.Strikes <= 3, "and strikes about once a second");
		P.Run(3.f, Near);
		Check(P.bFled && P.Strikes == 3, "then gives up after three strikes");
	}
	{
		FPlay P;
		P.V.Mood = EMood::Resist;
		FVictimInput Near;
		Near.TargetDistance = 100.f;
		P.Run(1.5f, Near);
		Near.CarSpeedKmh = 30.f;
		P.Run(0.3f, Near);
		Check(P.bFled, "a resisting driver gives up when the car drives off");
	}
	{
		FPlay P;
		P.V.Mood = EMood::Resist;
		FVictimInput Hurt;
		Hurt.TargetDistance = 600.f;
		P.Run(1.2f, Hurt);
		Hurt.bHurt = true;
		P.Run(0.1f, Hurt);
		Check(P.bFled, "a resisting driver who gets hurt runs away");
	}
	{
		FPlay P;
		P.V.Mood = EMood::Cower;
		P.Run(2.f, FVictimInput{});
		Check(!P.bFled && P.V.Phase == EPhase::Cower, "a cowering driver stays down");
		P.Run(3.f, FVictimInput{});
		Check(P.bFled, "then runs");
	}
	{
		FDriveAssist A;
		Check(PowerFromHealth(1.f, A) == 1.f && PowerFromHealth(0.f, A) == A.WeakPower && PowerFromHealth(0.2f, A) < 1.f, "power drops with health");
		Check(PullFromDents(1.f, 0.f, A) < 0.f && PullFromDents(0.f, 0.f, A) == 0.f, "a car beaten on the left pulls left");
		Check(YawAssist(true, 60.f, 1.f, 0.f, 0.f, A) > 0.f && YawAssist(true, 10.f, 1.f, 0.f, 0.f, A) == 0.f, "handbrake turn above walking pace only");
		Check(YawAssist(false, 80.f, 0.f, 30.f, 50.f, A) < 0.f && YawAssist(false, 80.f, 0.f, 5.f, 50.f, A) == 0.f, "stability help only in a slide");
	}
	{
		using namespace GTPTrafficReact;
		float Speed = 0.f;
		const GTPTrafficReact::FSettings Base;
		const GTPTrafficReact::FSettings Hurried = ApplyTemperament(Base, ETemperament::Hurried, Speed);
		Check(Speed > 1.f && Hurried.HonkAfter < Base.HonkAfter, "hurried drivers go faster and honk sooner");
		FBrain Brain;
		FInput In;
		In.Dt = 0.1f;
		In.bAimedAt = true;
		bool bBail = false;
		for (int Index = 0; Index < 20; ++Index)
		{
			bBail |= Step(Brain, In, Hurried).bBailOut;
		}
		Check(!bBail && Brain.State == EState::Panic, "a hurried driver floors it at gunpoint instead of getting out");
		FBrain Calm;
		bBail = false;
		for (int Index = 0; Index < 20; ++Index)
		{
			bBail |= Step(Calm, In, Base).bBailOut;
		}
		Check(bBail && Calm.State == EState::Flee, "a calm driver gets out and runs");
		FBrain Rammed;
		FInput Hit;
		Hit.Dt = 0.1f;
		Hit.Speed = 8.f;
		Hit.bHitByPlayer = true;
		Step(Rammed, Hit, Hurried);
		Check(Rammed.State == EState::Panic, "a hurried driver rammed by the player speeds off");
		Check(PickTemperament(0.1f) == ETemperament::Calm && PickTemperament(0.7f) == ETemperament::Hurried
			&& PickTemperament(0.95f) == ETemperament::Timid, "temperament shares");
	}
	std::printf("%d failure(s)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
