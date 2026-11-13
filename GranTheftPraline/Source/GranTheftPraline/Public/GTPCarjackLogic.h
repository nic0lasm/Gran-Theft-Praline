#pragma once

// What a driver pulled out of their car does next, and the arcade help given to the
// player's car at the wheel. No engine types: checked outside Unreal
// (tools/vehicles/carjack_test).
//
// Victim moods, drawn once when the driver lands on the road:
//   Flee   -- gets up and runs away from the car;
//   Resist -- gets up, comes back at the thief and punches (on foot) or bangs on the
//             door (the thief already inside), then gives up and runs;
//   Cower  -- stays down, hands over the head, then runs.
//
// Units: centimetres, seconds, km/h.

#include <cstdint>

namespace GTPCarjack
{
enum class EMood : uint8_t
{
	Flee,
	Resist,
	Cower
};

inline const char* MoodName(EMood Mood)
{
	switch (Mood)
	{
	case EMood::Flee: return "flee";
	case EMood::Resist: return "resist";
	case EMood::Cower: return "cower";
	}
	return "?";
}

struct FSettings
{
	/** Shares of the moods (the rest flees). */
	float ResistShare = 0.3f;
	float CowerShare = 0.15f;
	/** Time on the ground before anything else. */
	float GetUpSeconds = 0.9f;
	/** A resisting victim strikes from this close, once per StrikeInterval. */
	float StrikeRange = 170.f;
	float StrikeInterval = 1.1f;
	int MaxStrikes = 3;
	/** A resisting victim gives up after this long, or once the car drives off faster than this. */
	float ResistSeconds = 8.f;
	float GiveUpCarSpeedKmh = 15.f;
	/** A cowering victim stays down this long before running. */
	float CowerSeconds = 3.f;
};

/** Mood from a uniform roll in [0, 1). */
inline EMood PickMood(float Roll, const FSettings& S)
{
	if (Roll < S.ResistShare)
	{
		return EMood::Resist;
	}
	if (Roll < S.ResistShare + S.CowerShare)
	{
		return EMood::Cower;
	}
	return EMood::Flee;
}

enum class EPhase : uint8_t
{
	Down,
	Approach,
	Cower,
	Flee
};

struct FVictim
{
	EMood Mood = EMood::Flee;
	EPhase Phase = EPhase::Down;
	float Time = 0.f;
	float SinceStrike = 1000.f;
	int Strikes = 0;
};

struct FVictimInput
{
	float Dt = 0.f;
	/** Distance to the thief, or to the driver door when the thief is inside (cm). */
	float TargetDistance = 0.f;
	/** Speed of the stolen car (km/h, absolute). */
	float CarSpeedKmh = 0.f;
	/** The victim was hurt since the last step (shot, punched, hit by a car). */
	bool bHurt = false;
};

struct FVictimOutput
{
	/** Walk or run to the thief (or the door). */
	bool bMoveToTarget = false;
	/** One punch or one bang on the door, on this step. */
	bool bStrike = false;
	/** Start running away, on this step. */
	bool bStartFleeing = false;
};

inline FVictimOutput StepVictim(FVictim& V, const FVictimInput& In, const FSettings& S)
{
	FVictimOutput Out;
	V.Time += In.Dt;
	V.SinceStrike += In.Dt;
	auto Flee = [&V, &Out]()
	{
		V.Phase = EPhase::Flee;
		V.Time = 0.f;
		Out.bStartFleeing = true;
	};
	if (V.Phase == EPhase::Flee)
	{
		return Out;
	}
	if (In.bHurt)
	{
		Flee();
		return Out;
	}
	switch (V.Phase)
	{
	case EPhase::Down:
		if (V.Time >= S.GetUpSeconds)
		{
			V.Time = 0.f;
			if (V.Mood == EMood::Resist)
			{
				V.Phase = EPhase::Approach;
			}
			else if (V.Mood == EMood::Cower)
			{
				V.Phase = EPhase::Cower;
			}
			else
			{
				Flee();
			}
		}
		break;
	case EPhase::Approach:
		if (V.Time >= S.ResistSeconds || V.Strikes >= S.MaxStrikes || In.CarSpeedKmh > S.GiveUpCarSpeedKmh)
		{
			Flee();
			break;
		}
		if (In.TargetDistance <= S.StrikeRange)
		{
			if (V.SinceStrike >= S.StrikeInterval)
			{
				Out.bStrike = true;
				V.SinceStrike = 0.f;
				++V.Strikes;
			}
		}
		else
		{
			Out.bMoveToTarget = true;
		}
		break;
	case EPhase::Cower:
		if (V.Time >= S.CowerSeconds || In.CarSpeedKmh > S.GiveUpCarSpeedKmh)
		{
			Flee();
		}
		break;
	case EPhase::Flee:
		break;
	}
	return Out;
}

// --- Arcade help at the wheel (player's car only) ---------------------------

struct FDriveAssist
{
	/** Engine torque for the player's car, against the AI's realistic one (N.m). */
	float PlayerTorque = 240.f;
	/** Rear grip while the handbrake is held above DriftMinSpeedKmh (multiplier). */
	float DriftRearGrip = 0.55f;
	float DriftMinSpeedKmh = 20.f;
	/** Extra yaw rate asked by full steering in a handbrake turn (degrees per second, per second). */
	float DriftYawAccel = 160.f;
	/** Yaw damping when sliding without the handbrake (1/s): the car comes back straight. */
	float StabilityDamping = 1.6f;
	/** Above this slip angle (degrees) the stability help starts. */
	float StabilitySlipDeg = 18.f;
	/** Health share below which the engine loses power, and the power left at zero. */
	float WeakHealth = 0.35f;
	float WeakPower = 0.55f;
	/** Steering pull of a car dented on one side, at full dent (steering units). */
	float DentPull = 0.08f;
};

/** Torque factor from the health of the car (1 when healthy). */
inline float PowerFromHealth(float HealthRatio, const FDriveAssist& A)
{
	if (HealthRatio >= A.WeakHealth || A.WeakHealth <= 0.f)
	{
		return 1.f;
	}
	const float T = HealthRatio <= 0.f ? 0.f : HealthRatio / A.WeakHealth;
	return A.WeakPower + (1.f - A.WeakPower) * T;
}

/** Steering offset from the dents: a car beaten on its left pulls left (negative). */
inline float PullFromDents(float LeftDent01, float RightDent01, const FDriveAssist& A)
{
	return (RightDent01 - LeftDent01) * A.DentPull;
}

/**
 * Yaw acceleration to add (degrees per second, per second), positive to the right.
 * Handbrake turn: steering swings the tail out. Sliding without the handbrake: damps the
 * yaw rate so the car straightens, as in open-world games.
 */
inline float YawAssist(bool bHandbrake, float SpeedKmh, float Steer, float SlipDeg, float YawRateDeg, const FDriveAssist& A)
{
	const float Speed = SpeedKmh < 0.f ? -SpeedKmh : SpeedKmh;
	if (bHandbrake)
	{
		if (Speed < A.DriftMinSpeedKmh)
		{
			return 0.f;
		}
		const float SpeedFactor = Speed > 60.f ? 1.f : Speed / 60.f;
		return Steer * A.DriftYawAccel * SpeedFactor;
	}
	const float Slip = SlipDeg < 0.f ? -SlipDeg : SlipDeg;
	if (Slip < A.StabilitySlipDeg || Speed < 30.f)
	{
		return 0.f;
	}
	return -YawRateDeg * A.StabilityDamping;
}
} // namespace GTPCarjack
