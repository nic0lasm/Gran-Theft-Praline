#pragma once

// Rules for hits, falls and ragdolls, shared by the player, the pedestrians and
// later the combat. No Unreal types, so tools/ragdoll/impact_sim checks it outside
// the editor (same approach as GTPWalkLogic.h and GTPDriveLogic.h).
// Units: centimetres, seconds, kilograms; speeds given in km/h where named so.
// Vectors are plain floats: x east, y south, z up, as in Unreal.

#include <algorithm>
#include <cmath>
#include <vector>

namespace GTPImpact
{
struct FVec
{
	float X = 0.f;
	float Y = 0.f;
	float Z = 0.f;
};

inline float Length(const FVec& V) { return std::sqrt(V.X * V.X + V.Y * V.Y + V.Z * V.Z); }
inline float Length2D(const FVec& V) { return std::sqrt(V.X * V.X + V.Y * V.Y); }
inline FVec Sub(const FVec& A, const FVec& B) { return {A.X - B.X, A.Y - B.Y, A.Z - B.Z}; }
inline FVec Scale(const FVec& A, float S) { return {A.X * S, A.Y * S, A.Z * S}; }
inline float Dot2D(const FVec& A, const FVec& B) { return A.X * B.X + A.Y * B.Y; }
inline float KmhFromCmS(float CmS) { return CmS * 0.036f; }
inline float CmSFromKmh(float Kmh) { return Kmh / 0.036f; }

/** How hard someone is hit. Order matters: each step is worse than the one before. */
enum class ESeverity : int
{
	None = 0,
	/** Shoulder bump: a stumble, no ragdoll. */
	Bump = 1,
	/** Knocked over: ragdoll, no launch, gets up. */
	Knock = 2,
	/** Thrown onto the bonnet or the road: ragdoll with a launch, may stay down. */
	Launch = 3,
	/** Run over at speed: ragdoll, long launch, does not get up. */
	Heavy = 4
};

struct FSettings
{
	/** Relative speeds (km/h) where each severity starts. */
	float BumpKmh = 4.f;
	float KnockKmh = 12.f;
	float LaunchKmh = 28.f;
	float HeavyKmh = 55.f;

	/** Share of the car's speed the body leaves with (a body is not a wall: below 1). */
	float LaunchTransfer = 0.55f;
	/** Vertical part of the launch, relative to the horizontal one (bonnet height). */
	float LaunchLift = 0.35f;
	float MaxLaunchCmS = 1800.f;

	/** Health lost at KnockKmh, LaunchKmh and HeavyKmh (health is 0..100). */
	float KnockDamage = 12.f;
	float LaunchDamage = 45.f;
	float HeavyDamage = 100.f;

	/** Vertical landing speeds (cm/s, downward, positive) for the player. */
	float StumbleLandCmS = 650.f;
	float RollLandCmS = 900.f;
	float RagdollLandCmS = 1250.f;
	float LandDamagePerCmS = 0.12f;
};

struct FCarHitInput
{
	/** Car and person velocity in cm/s. */
	FVec CarVelocity;
	FVec PersonVelocity;
	/** From the person to the car's contact point, used when the car is not moving toward them. */
	FVec ToCar;
	float CarMassKg = 1200.f;
	float PersonMassKg = 75.f;
};

struct FCarHitResult
{
	ESeverity Severity = ESeverity::None;
	float RelativeKmh = 0.f;
	/** Velocity change to give the body (cm/s, use AddImpulse with bVelChange). */
	FVec LaunchVelocity;
	/** Health lost (0..100). */
	float Damage = 0.f;
	/** Speed the car loses (km/h), tiny: a body weighs little. */
	float CarSpeedLossKmh = 0.f;
	/** Spin to give the body at the pelvis (rad/s), larger for harder hits. */
	float SpinRadS = 0.f;
};

inline ESeverity ClassifyRelativeKmh(float Kmh, const FSettings& S)
{
	if (Kmh >= S.HeavyKmh) return ESeverity::Heavy;
	if (Kmh >= S.LaunchKmh) return ESeverity::Launch;
	if (Kmh >= S.KnockKmh) return ESeverity::Knock;
	if (Kmh >= S.BumpKmh) return ESeverity::Bump;
	return ESeverity::None;
}

/** Health lost for a relative speed, piecewise linear through the three damage points. */
inline float DamageForKmh(float Kmh, const FSettings& S)
{
	if (Kmh < S.BumpKmh) return 0.f;
	if (Kmh < S.KnockKmh) return S.KnockDamage * (Kmh - S.BumpKmh) / std::max(1.f, S.KnockKmh - S.BumpKmh) * 0.5f;
	if (Kmh < S.LaunchKmh)
	{
		const float T = (Kmh - S.KnockKmh) / std::max(1.f, S.LaunchKmh - S.KnockKmh);
		return S.KnockDamage + T * (S.LaunchDamage - S.KnockDamage);
	}
	if (Kmh < S.HeavyKmh)
	{
		const float T = (Kmh - S.LaunchKmh) / std::max(1.f, S.HeavyKmh - S.LaunchKmh);
		return S.LaunchDamage + T * (S.HeavyDamage - S.LaunchDamage);
	}
	return S.HeavyDamage;
}

/** A car touches a person. A car backing away from them, or not moving, hurts no one. */
inline FCarHitResult ClassifyCarHit(const FCarHitInput& In, const FSettings& S)
{
	FCarHitResult R;
	const FVec Rel = Sub(In.CarVelocity, In.PersonVelocity);
	const float RelSpeed2D = Length2D(Rel);
	if (RelSpeed2D < 1.f)
	{
		return R;
	}
	// Closing speed along the line between them (the car must be going toward the person).
	const float ToCarLen = Length2D(In.ToCar);
	float Closing = RelSpeed2D;
	if (ToCarLen > 1.f)
	{
		const float Along = -Dot2D(Rel, In.ToCar) / (RelSpeed2D * ToCarLen); // 1 = head-on
		if (Along <= 0.f)
		{
			return R;
		}
		Closing = RelSpeed2D * std::max(0.35f, Along);
	}
	R.RelativeKmh = KmhFromCmS(Closing);
	R.Severity = ClassifyRelativeKmh(R.RelativeKmh, S);
	if (R.Severity == ESeverity::None)
	{
		return R;
	}
	R.Damage = DamageForKmh(R.RelativeKmh, S);

	// Momentum: the person leaves with a share of the car's velocity.
	const float MassShare = In.CarMassKg / (In.CarMassKg + In.PersonMassKg);
	float Horizontal = Closing * S.LaunchTransfer * MassShare;
	if (R.Severity == ESeverity::Bump)
	{
		Horizontal *= 0.25f;
	}
	else if (R.Severity == ESeverity::Knock)
	{
		Horizontal *= 0.5f;
	}
	Horizontal = std::min(Horizontal, S.MaxLaunchCmS);
	const float Dx = Rel.X / RelSpeed2D;
	const float Dy = Rel.Y / RelSpeed2D;
	// Lift only grows for launches: a bump shoves, a launch throws onto the bonnet.
	const float Lift = (R.Severity >= ESeverity::Launch) ? S.LaunchLift : 0.12f;
	R.LaunchVelocity = {Dx * Horizontal, Dy * Horizontal, Horizontal * Lift};
	R.CarSpeedLossKmh = KmhFromCmS(Closing) * (In.PersonMassKg / (In.CarMassKg + In.PersonMassKg)) * 0.5f;
	R.SpinRadS = (R.Severity >= ESeverity::Launch) ? 3.f + 6.f * std::min(1.f, R.RelativeKmh / S.HeavyKmh) : 0.f;
	return R;
}

enum class ELanding : int
{
	None = 0,
	Stumble = 1,
	Roll = 2,
	Ragdoll = 3
};

struct FLandingResult
{
	ELanding Kind = ELanding::None;
	float Damage = 0.f;
};

/** The player lands at VerticalCmS (downward, positive). Crouching halves the shock. */
inline FLandingResult ClassifyLanding(float VerticalCmS, bool bCrouching, const FSettings& S)
{
	FLandingResult R;
	const float V = bCrouching ? VerticalCmS * 0.5f : VerticalCmS;
	if (V < S.StumbleLandCmS) return R;
	R.Kind = V < S.RollLandCmS ? ELanding::Stumble : (V < S.RagdollLandCmS ? ELanding::Roll : ELanding::Ragdoll);
	R.Damage = (V - S.StumbleLandCmS) * S.LandDamagePerCmS;
	return R;
}

/** Leaving a moving car: above this ground speed the player tumbles, below he steps out (km/h). */
struct FBailOut
{
	float TumbleKmh = 30.f;
	float RagdollKmh = 55.f;
};

inline ESeverity ClassifyBailOut(float CarKmh, const FBailOut& B = FBailOut())
{
	if (CarKmh >= B.RagdollKmh) return ESeverity::Launch;
	if (CarKmh >= B.TumbleKmh) return ESeverity::Knock;
	if (CarKmh >= 8.f) return ESeverity::Bump;
	return ESeverity::None;
}

/** What a body does once a ragdoll is running. */
enum class EOutcome : int
{
	Simulating = 0,
	GetUpFaceUp = 1,
	GetUpFaceDown = 2,
	StayDown = 3
};

struct FRagdollTimings
{
	/** Never wake before this (s): the body must actually fall. */
	float MinSeconds = 1.5f;
	/** At rest = pelvis slower than this (cm/s) for RestSeconds. */
	float RestSpeedCmS = 25.f;
	float RestSeconds = 0.7f;
	/** Give up and decide after this even if still sliding (s). */
	float MaxSeconds = 10.f;
	/** Pause on the ground before getting up or being frozen (<= 5 s after rest, agreed with the optimisation thread). */
	float PauseKnock = 0.6f;
	float PauseLaunch = 2.5f;
};

/** Cheaper life cycle (ragdoll lite): settles sooner, pauses less, gives up sooner. */
inline FRagdollTimings LiteTimings()
{
	FRagdollTimings T;
	T.MinSeconds = 1.0f;
	T.RestSpeedCmS = 50.f;
	T.RestSeconds = 0.4f;
	T.MaxSeconds = 5.f;
	T.PauseKnock = 0.5f;
	T.PauseLaunch = 1.5f;
	return T;
}

struct FRagdollState
{
	ESeverity Severity = ESeverity::Knock;
	/** Health left after the hit (0..100). */
	float Health = 100.f;
	float Elapsed = 0.f;
	float RestFor = 0.f;
	bool bAtRest = false;
	float PauseLeft = 0.f;
	EOutcome Outcome = EOutcome::Simulating;
};

inline FRagdollState StartRagdollState(ESeverity Severity, float HealthAfterHit)
{
	FRagdollState S;
	S.Severity = Severity;
	S.Health = HealthAfterHit;
	return S;
}

/**
 * One tick. PelvisSpeedCmS = speed of the pelvis body; PelvisUpDot = dot product of the
 * pelvis "front" axis with world up (> 0: lying face up, < 0: face down).
 * Returns the outcome, which stays Simulating until the body has settled and paused.
 */
inline EOutcome StepRagdoll(FRagdollState& S, float PelvisSpeedCmS, float PelvisUpDot, float DeltaTime, const FRagdollTimings& T)
{
	if (S.Outcome != EOutcome::Simulating)
	{
		return S.Outcome;
	}
	S.Elapsed += DeltaTime;
	if (!S.bAtRest)
	{
		S.RestFor = (PelvisSpeedCmS < T.RestSpeedCmS) ? S.RestFor + DeltaTime : 0.f;
		const bool bSettled = S.Elapsed >= T.MinSeconds && S.RestFor >= T.RestSeconds;
		if (bSettled || S.Elapsed >= T.MaxSeconds)
		{
			S.bAtRest = true;
			S.PauseLeft = (S.Severity >= ESeverity::Launch) ? T.PauseLaunch : T.PauseKnock;
		}
		return EOutcome::Simulating;
	}
	S.PauseLeft -= DeltaTime;
	if (S.PauseLeft > 0.f)
	{
		return EOutcome::Simulating;
	}
	if (S.Health <= 0.f || S.Severity == ESeverity::Heavy)
	{
		S.Outcome = EOutcome::StayDown;
	}
	else
	{
		S.Outcome = PelvisUpDot >= 0.f ? EOutcome::GetUpFaceUp : EOutcome::GetUpFaceDown;
	}
	return S.Outcome;
}

/**
 * Cost cap: at most MaxActive ragdolls simulate at once. Given the distance of each
 * active ragdoll to the viewer and how long it has run, tells which to freeze (stop
 * simulating, keep the pose, hide later): the farthest, then the oldest.
 * Returns indexes into the input, to freeze.
 */
struct FRagdollEntry
{
	float DistanceCm = 0.f;
	float Age = 0.f;
};

inline std::vector<int> ChooseToFreeze(const std::vector<FRagdollEntry>& Active, int MaxActive)
{
	std::vector<int> Order(Active.size());
	for (size_t I = 0; I < Order.size(); ++I) Order[I] = static_cast<int>(I);
	// Keep the nearest, break ties by the youngest.
	std::sort(Order.begin(), Order.end(), [&](int A, int B) {
		if (std::fabs(Active[A].DistanceCm - Active[B].DistanceCm) > 1.f) return Active[A].DistanceCm < Active[B].DistanceCm;
		return Active[A].Age < Active[B].Age;
	});
	std::vector<int> Freeze;
	for (size_t I = static_cast<size_t>(std::max(0, MaxActive)); I < Order.size(); ++I)
	{
		Freeze.push_back(Order[I]);
	}
	return Freeze;
}
} // namespace GTPImpact
