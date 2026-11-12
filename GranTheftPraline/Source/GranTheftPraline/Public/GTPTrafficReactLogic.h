#pragma once

// How a traffic driver reacts to the street around them, on top of the plain
// driving of GTPDriveLogic.h: the horn, going round a car or a person that
// does not move, panic when a gun is pointed at them or a shot is fired near,
// getting out and running away, a moment of shock after the player rams them.
// No engine types: the rules are run outside Unreal (tools/vehicles/react_test).
//
// States and transitions (checked in this order every step):
//
//   any but Flee  -- aimed at for FleeAimSeconds while slower than FleeMaxSpeed --> Flee (bail out once)
//   any but Flee  -- shot fired near, or aimed at while moving ----------------> Panic
//   Cruise        -- hit by the player ------------------------------------------> Shaken
//   Cruise        -- stopped behind a still obstacle closer than ObstacleNear ----> Waiting
//   Waiting       -- obstacle gone or moving again ------------------------------> Cruise
//   Waiting       -- blocked BypassAfter seconds and the side is clear ----------> Bypass
//   Bypass        -- BypassMaxSeconds, or clear road for BypassClearSeconds -----> Cruise
//   Panic         -- PanicSeconds --------------------------------------------> Cruise
//   Shaken        -- ShakenSeconds -------------------------------------------> Cruise
//   Flee          -- terminal (the driver is out of the car)
//
// Units: metres, seconds, metres per second.

#include <cstdint>

namespace GTPTrafficReact
{
enum class EState : uint8_t
{
	Cruise,
	Waiting,
	Bypass,
	Panic,
	Flee,
	Shaken
};

inline const char* StateName(EState State)
{
	switch (State)
	{
	case EState::Cruise: return "cruise";
	case EState::Waiting: return "waiting";
	case EState::Bypass: return "bypass";
	case EState::Panic: return "panic";
	case EState::Flee: return "flee";
	case EState::Shaken: return "shaken";
	}
	return "?";
}

struct FSettings
{
	/** Below this speed the car counts as stopped. */
	float StoppedSpeed = 0.6f;
	/** A still obstacle closer than this blocks the car. */
	float ObstacleNear = 12.f;
	/** Stopped behind a still obstacle this long before the first horn (the player: PlayerHonkAfter). */
	float HonkAfter = 2.5f;
	float PlayerHonkAfter = 1.f;
	/** Length of one horn blast, and the pause before the next one while still blocked. */
	float HonkSeconds = 0.5f;
	float HonkRepeat = 3.f;
	/** Blocked this long before going round, when the side is clear. */
	float BypassAfter = 5.f;
	/** How far to the side the car moves to go round. */
	float BypassOffset = 2.9f;
	/** Sideways speed of the move out and back in. */
	float BypassSlideSpeed = 1.2f;
	/** A bypass ends after this long, or once the road ahead stayed clear this long. */
	float BypassMaxSeconds = 9.f;
	float BypassClearSeconds = 2.f;
	/** Speed limit while going round (fraction of the lane speed). */
	float BypassSpeedScale = 0.45f;
	/** Flooring it after a threat: how long, how much over the lane speed, horn blasts. */
	float PanicSeconds = 8.f;
	float PanicSpeedScale = 1.6f;
	float PanicHornSeconds = 1.5f;
	/** Aimed at this long while slower than FleeMaxSpeed: the driver gets out and runs. */
	float FleeAimSeconds = 1.f;
	float FleeMaxSpeed = 3.f;
	/** Standing still after being rammed by the player, one long blast first. */
	float ShakenSeconds = 2.5f;
	float ShakenHornSeconds = 0.9f;
};

/** What the driver perceives this step. */
struct FInput
{
	float Dt = 0.f;
	float Speed = 0.f;
	/** Distance to what blocks the road ahead (negative: nothing). */
	float ObstacleM = -1.f;
	/** That obstacle does not move and will not move by itself (not a queue at a light). */
	bool bObstacleStill = false;
	/** That obstacle is the player or the player's car. */
	bool bObstacleIsPlayer = false;
	/** Room on the overtaking side for a car length ahead and behind. */
	bool bSideClear = false;
	/** An armed threat points at this car's driver. */
	bool bAimedAt = false;
	/** A shot was fired near since the last step. */
	bool bShotNear = false;
	/** The player's car hit this car since the last step. */
	bool bHitByPlayer = false;
};

/** What the driving code does with it. */
struct FOutput
{
	bool bHorn = false;
	/** Multiplies the lane speed. */
	float SpeedScale = 1.f;
	/** Red lights and junction turns are ignored (panic). */
	bool bIgnoreSignals = false;
	/** Sideways shift from the lane, metres, positive towards the overtaking side. */
	float LateralOffset = 0.f;
	/** The obstacle being gone round is not an obstacle for the speed control. */
	bool bIgnoreBlocker = false;
	/** Brake and stay put. */
	bool bHold = false;
	/** True on the one step the driver decides to get out and run. */
	bool bBailOut = false;
	/** The state changed this step. */
	bool bChanged = false;
};

struct FBrain
{
	EState State = EState::Cruise;
	float StateTime = 0.f;
	float BlockedTime = 0.f;
	float AimedTime = 0.f;
	float ClearTime = 0.f;
	float HornLeft = 0.f;
	float HornWait = 0.f;
	float Offset = 0.f;
	int HornBlasts = 0;
};

inline void Enter(FBrain& Brain, EState State)
{
	Brain.State = State;
	Brain.StateTime = 0.f;
	Brain.ClearTime = 0.f;
	Brain.HornWait = 0.f;
}

inline void Blast(FBrain& Brain, float Seconds)
{
	Brain.HornLeft = Seconds;
	++Brain.HornBlasts;
}

inline float MoveTowards(float Value, float Target, float MaxStep)
{
	if (Value < Target)
	{
		return Value + MaxStep < Target ? Value + MaxStep : Target;
	}
	return Value - MaxStep > Target ? Value - MaxStep : Target;
}

inline FOutput Step(FBrain& Brain, const FInput& In, const FSettings& S)
{
	FOutput Out;
	const EState Before = Brain.State;
	const bool bStopped = In.Speed < S.StoppedSpeed;
	const bool bBlocked = In.ObstacleM >= 0.f && In.ObstacleM < S.ObstacleNear && In.bObstacleStill;
	Brain.StateTime += In.Dt;
	Brain.AimedTime = In.bAimedAt ? Brain.AimedTime + In.Dt : 0.f;
	Brain.BlockedTime = bBlocked && bStopped ? Brain.BlockedTime + In.Dt : (bBlocked ? Brain.BlockedTime : 0.f);

	if (Brain.State != EState::Flee)
	{
		if (Brain.AimedTime >= S.FleeAimSeconds && In.Speed <= S.FleeMaxSpeed)
		{
			Enter(Brain, EState::Flee);
			Out.bBailOut = true;
		}
		else if (In.bShotNear || (In.bAimedAt && In.Speed > S.FleeMaxSpeed))
		{
			if (Brain.State != EState::Panic)
			{
				Enter(Brain, EState::Panic);
				Blast(Brain, S.PanicHornSeconds);
			}
			else
			{
				// Still under fire: the panic goes on.
				Brain.StateTime = 0.f;
			}
		}
	}

	switch (Brain.State)
	{
	case EState::Cruise:
		if (In.bHitByPlayer)
		{
			Enter(Brain, EState::Shaken);
			Blast(Brain, S.ShakenHornSeconds);
		}
		else if (bBlocked && bStopped)
		{
			Enter(Brain, EState::Waiting);
		}
		break;
	case EState::Waiting:
		if (!bBlocked)
		{
			Enter(Brain, EState::Cruise);
			Brain.BlockedTime = 0.f;
			break;
		}
		if (Brain.BlockedTime >= S.BypassAfter && In.bSideClear)
		{
			Enter(Brain, EState::Bypass);
			break;
		}
		{
			const float FirstHonk = In.bObstacleIsPlayer ? S.PlayerHonkAfter : S.HonkAfter;
			if (Brain.BlockedTime >= FirstHonk && Brain.HornLeft <= 0.f)
			{
				Brain.HornWait -= In.Dt;
				if (Brain.HornWait <= 0.f)
				{
					Blast(Brain, S.HonkSeconds);
					Brain.HornWait = S.HonkRepeat;
				}
			}
		}
		break;
	case EState::Bypass:
		Brain.ClearTime = In.ObstacleM < 0.f ? Brain.ClearTime + In.Dt : 0.f;
		if (Brain.StateTime >= S.BypassMaxSeconds || Brain.ClearTime >= S.BypassClearSeconds)
		{
			Enter(Brain, EState::Cruise);
			Brain.BlockedTime = 0.f;
		}
		break;
	case EState::Panic:
		if (Brain.StateTime >= S.PanicSeconds)
		{
			Enter(Brain, EState::Cruise);
		}
		break;
	case EState::Shaken:
		if (Brain.StateTime >= S.ShakenSeconds)
		{
			Enter(Brain, EState::Cruise);
		}
		break;
	case EState::Flee:
		break;
	}

	// Outputs of the state the step ends in.
	const float OffsetGoal = Brain.State == EState::Bypass ? S.BypassOffset : 0.f;
	Brain.Offset = MoveTowards(Brain.Offset, OffsetGoal, S.BypassSlideSpeed * In.Dt);
	Out.LateralOffset = Brain.Offset;
	switch (Brain.State)
	{
	case EState::Bypass:
		Out.SpeedScale = S.BypassSpeedScale;
		Out.bIgnoreBlocker = true;
		break;
	case EState::Panic:
		Out.SpeedScale = S.PanicSpeedScale;
		Out.bIgnoreSignals = true;
		break;
	case EState::Shaken:
	case EState::Flee:
		Out.bHold = true;
		break;
	default:
		break;
	}
	if (Brain.HornLeft > 0.f)
	{
		Out.bHorn = true;
		Brain.HornLeft -= In.Dt;
	}
	Out.bChanged = Brain.State != Before;
	return Out;
}
} // namespace GTPTrafficReact
