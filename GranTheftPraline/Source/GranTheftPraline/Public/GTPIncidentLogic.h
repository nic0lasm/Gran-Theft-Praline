#pragma once

// What two traffic drivers do after their cars crashed into each other, and the steps of
// an argument in the street. No engine types: checked outside Unreal
// (tools/vehicles/incident_test).
//
// Outcomes, drawn once per crash:
//   DriveOn   -- a light bump: a blast of the horn, both drive on;
//   HitAndRun -- the one who hit floors it, the other gets out and shouts after them;
//   FleeOnFoot-- a timid driver leaves the car and runs, the other gets out;
//   Argue     -- both get out, walk up to each other, argue (a shove now and then), then
//                go back to their cars and drive on.
//
// Units: centimetres, seconds, cm/s.

#include <cstdint>

namespace GTPIncident
{
enum class EOutcome : uint8_t
{
	DriveOn,
	HitAndRun,
	FleeOnFoot,
	Argue
};

inline const char* OutcomeName(EOutcome Outcome)
{
	switch (Outcome)
	{
	case EOutcome::DriveOn: return "drive on";
	case EOutcome::HitAndRun: return "hit and run";
	case EOutcome::FleeOnFoot: return "flee on foot";
	case EOutcome::Argue: return "argue";
	}
	return "?";
}

struct FSettings
{
	/** Below this change of speed the crash is a bump (cm/s). */
	float BumpSpeedChange = 700.f;
	/** Share of bumps where the drivers still get out to argue. */
	float BumpArgueShare = 0.35f;
	/** Shares of real crashes ending in a hit and run, and (one driver timid) a flight on foot. */
	float HitAndRunShare = 0.2f;
	float TimidFleeShare = 0.6f;
	/** Above this change of speed, nobody argues: both get out and run (cm/s). */
	float PanicSpeedChange = 2200.f;

	/** The argument: how close they stand, how long it lasts, the shoves. */
	float TalkDistance = 150.f;
	float ArgueMinSeconds = 6.f;
	float ArgueMaxSeconds = 10.f;
	float ShoveInterval = 2.5f;
	/** Longest wait to be out of the car, and to walk up or back, before giving up. */
	float GetOutSeconds = 6.f;
	float WalkSeconds = 12.f;
	/** Back at the car from this close (cm). */
	float CarReachDistance = 260.f;
};

/**
 * Outcome from two uniform rolls in [0, 1). bTimid: either driver is timid.
 * SpeedChange: the crash's change of speed (cm/s).
 */
inline EOutcome PickOutcome(float Roll, float SpeedChange, bool bTimid, const FSettings& S)
{
	if (SpeedChange < S.BumpSpeedChange)
	{
		return Roll < S.BumpArgueShare ? EOutcome::Argue : EOutcome::DriveOn;
	}
	if (SpeedChange >= S.PanicSpeedChange)
	{
		return EOutcome::FleeOnFoot;
	}
	if (Roll < S.HitAndRunShare)
	{
		return EOutcome::HitAndRun;
	}
	if (bTimid && Roll < S.HitAndRunShare + (1.f - S.HitAndRunShare) * S.TimidFleeShare)
	{
		return EOutcome::FleeOnFoot;
	}
	return EOutcome::Argue;
}

enum class EPhase : uint8_t
{
	GetOut,
	Approach,
	Argue,
	Return,
	Done,
	Flee
};

inline const char* PhaseName(EPhase Phase)
{
	switch (Phase)
	{
	case EPhase::GetOut: return "get out";
	case EPhase::Approach: return "approach";
	case EPhase::Argue: return "argue";
	case EPhase::Return: return "return";
	case EPhase::Done: return "done";
	case EPhase::Flee: return "flee";
	}
	return "?";
}

struct FArguer
{
	EPhase Phase = EPhase::GetOut;
	float Time = 0.f;
	/** Length of this argument, drawn at the start (ArgueMin..ArgueMax). */
	float ArgueSeconds = 8.f;
	float SinceShove = 0.f;
	int Shoves = 0;
};

struct FArguerInput
{
	float Dt = 0.f;
	/** The driver stands in the street (out of the car). */
	bool bOut = false;
	/** The other driver is there to argue with (out, not fled, not gone with their car). */
	bool bOtherHere = false;
	float OtherDistance = 0.f;
	float CarDistance = 0.f;
	/** The car is gone: stolen, wrecked, taken away. */
	bool bCarGone = false;
	/** A gun aimed at them, a shot nearby, hurt. */
	bool bThreat = false;
	/** Uniform roll in [0, 1) for a shove on this step. */
	float ShoveRoll = 1.f;
	/** Share of shove chances taken. */
	float ShoveChance = 0.4f;
};

struct FArguerOutput
{
	bool bMoveToOther = false;
	bool bMoveToCar = false;
	/** Face the other one and gesture. */
	bool bTalk = false;
	bool bShove = false;
	/** Back at the car: sit in and drive on, on this step. */
	bool bReclaim = false;
	bool bStartFleeing = false;
};

inline FArguerOutput StepArguer(FArguer& A, const FArguerInput& In, const FSettings& S)
{
	FArguerOutput Out;
	if (A.Phase == EPhase::Done || A.Phase == EPhase::Flee)
	{
		return Out;
	}
	A.Time += In.Dt;
	A.SinceShove += In.Dt;
	auto Enter = [&A](EPhase Phase)
	{
		A.Phase = Phase;
		A.Time = 0.f;
	};
	// Threatened, or the car stolen under their nose: they run.
	if (In.bThreat || (In.bCarGone && In.bOut))
	{
		Enter(EPhase::Flee);
		Out.bStartFleeing = true;
		return Out;
	}
	switch (A.Phase)
	{
	case EPhase::GetOut:
		if (In.bOut)
		{
			Enter(In.bOtherHere ? EPhase::Approach : EPhase::Argue);
		}
		else if (A.Time >= S.GetOutSeconds)
		{
			// Never got out (boxed in): stays in the car.
			Enter(EPhase::Done);
		}
		break;
	case EPhase::Approach:
		if (!In.bOtherHere)
		{
			// Shouts after them where they stand.
			Enter(EPhase::Argue);
		}
		else if (In.OtherDistance <= S.TalkDistance)
		{
			Enter(EPhase::Argue);
			A.SinceShove = 0.f;
		}
		else if (A.Time >= S.WalkSeconds)
		{
			Enter(EPhase::Return);
		}
		else
		{
			Out.bMoveToOther = true;
		}
		break;
	case EPhase::Argue:
	{
		// Alone (the other drove off or ran), the shouting is shorter.
		const float Length = In.bOtherHere ? A.ArgueSeconds : A.ArgueSeconds * 0.4f;
		if (A.Time >= Length)
		{
			Enter(EPhase::Return);
			break;
		}
		Out.bTalk = true;
		if (In.bOtherHere && In.OtherDistance > S.TalkDistance * 1.6f)
		{
			Out.bMoveToOther = true;
		}
		if (In.bOtherHere && A.SinceShove >= S.ShoveInterval)
		{
			A.SinceShove = 0.f;
			if (In.ShoveRoll < In.ShoveChance && In.OtherDistance <= S.TalkDistance * 1.3f)
			{
				Out.bShove = true;
				++A.Shoves;
			}
		}
		break;
	}
	case EPhase::Return:
		if (In.CarDistance <= S.CarReachDistance)
		{
			Enter(EPhase::Done);
			Out.bReclaim = true;
		}
		else if (A.Time >= S.WalkSeconds)
		{
			// Lost: walks away from it all.
			Enter(EPhase::Flee);
			Out.bStartFleeing = true;
		}
		else
		{
			Out.bMoveToCar = true;
		}
		break;
	case EPhase::Done:
	case EPhase::Flee:
		break;
	}
	return Out;
}
} // namespace GTPIncident
