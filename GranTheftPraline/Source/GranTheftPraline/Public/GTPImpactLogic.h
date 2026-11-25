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

// ---------------------------------------------------------------- a body thrown by a car, kept within bounds

/**
 * A pedestrian's body hit by a car is a ragdoll that the car's own collision keeps pushing. Left to the physics
 * alone it flew 4 to 5 m up and 15 m ahead at 35 to 40 km/h and came down on the roof (long video of gate 74,
 * 75 s, 143 s, 148 s, 160 s). These bounds are applied to every part of the body at every frame.
 */
struct FBodyBounds
{
	/** No part goes up faster than this (cm/s): a free rise of 46 cm. */
	float MaxUpCmS = 300.f;
	/** Pelvis this high above the ground the person stood on (cm): nothing goes up any more. Under a car's roof. */
	float MaxHeightCm = 140.f;
	/** No part goes faster along the ground than the car that hit, times this... */
	float CarShare = 1.05f;
	/** ...and never is the cap under this (cm/s): a body shoved by a slow car still falls over. */
	float MinHorizontalCmS = 250.f;
	/** No part spins faster than this (rad/s). */
	float MaxSpinRadS = 10.f;

	/** Pelvis higher than this above the ground (cm) and not falling: the body lies on something, a car. */
	float OnCarHeightCm = 60.f;
	/** "Not falling, not thrown": vertical speed under this (cm/s). */
	float OnCarRestCmS = 150.f;
	/** A person takes this long to go down (s): before, his pelvis is high because he still stands. */
	float OnCarGrace = 0.8f;
	/** Lying on a car for this long (s): it slides off to the nearer side. Longer than the top of a free throw. */
	float SlideAfter = 0.4f;
	/** Speed of the slide, across the car (cm/s). */
	float SlideCmS = 280.f;
	/** Still up there after this long (s): the cars stop holding it and it drops to the ground. */
	float DropAfter = 1.5f;
};

/** The fastest a part may go along the ground, for a hit by a car going at CarSpeedCmS. */
inline float HorizontalCap(float CarSpeedCmS, const FBodyBounds& B)
{
	return std::max(B.MinHorizontalCmS, std::fabs(CarSpeedCmS) * B.CarShare);
}

/** Velocity of a part brought back within the bounds. PelvisHeightCm: above the ground the person stood on. */
inline FVec BoundVelocity(const FVec& V, float PelvisHeightCm, float CarSpeedCmS, const FBodyBounds& B)
{
	FVec R = V;
	if (!std::isfinite(R.X) || !std::isfinite(R.Y) || !std::isfinite(R.Z))
	{
		return FVec();
	}
	const float Cap = HorizontalCap(CarSpeedCmS, B);
	const float Along = Length2D(R);
	if (Along > Cap)
	{
		R.X *= Cap / Along;
		R.Y *= Cap / Along;
	}
	const float Up = PelvisHeightCm >= B.MaxHeightCm ? 0.f : B.MaxUpCmS;
	R.Z = std::min(R.Z, Up);
	return R;
}

/** Spin of a part (rad/s) brought back within the bounds. */
inline FVec BoundSpin(const FVec& W, const FBodyBounds& B)
{
	if (!std::isfinite(W.X) || !std::isfinite(W.Y) || !std::isfinite(W.Z))
	{
		return FVec();
	}
	const float Size = Length(W);
	return Size > B.MaxSpinRadS ? Scale(W, B.MaxSpinRadS / Size) : W;
}

enum class EOnCar : int
{
	/** On the ground, falling or thrown. */
	No = 0,
	/** Lying on a car: push it across, off the nearer side. */
	Slide = 1,
	/** Up there for too long: stop colliding with cars, it drops. */
	Drop = 2
};

struct FOnCarState
{
	/** Since the fall began, and of that, lying up on something. */
	float Age = 0.f;
	float Seconds = 0.f;
};

/** One frame of a body that may lie on a car. Back on the ground, the count starts again. */
inline EOnCar StepOnCar(FOnCarState& S, float PelvisHeightCm, float PelvisVerticalCmS, float DeltaTime, const FBodyBounds& B)
{
	S.Age += std::max(0.f, DeltaTime);
	if (PelvisHeightCm < B.OnCarHeightCm || S.Age < B.OnCarGrace)
	{
		S.Seconds = 0.f;
		return EOnCar::No;
	}
	if (std::fabs(PelvisVerticalCmS) < B.OnCarRestCmS)
	{
		S.Seconds += std::max(0.f, DeltaTime);
	}
	if (S.Seconds >= B.DropAfter)
	{
		return EOnCar::Drop;
	}
	return S.Seconds >= B.SlideAfter ? EOnCar::Slide : EOnCar::No;
}

/**
 * Does a moving car reach a person now? Along and Side: the person in the car's frame (cm, from the centre of its
 * box; Along positive ahead). The person falls when the end of the car that leads is about to touch him, or when a
 * flank brushes him, and not once he is 30 cm inside the body: a ragdoll that starts inside the car's collision is
 * thrown out of it by the physics (the old rule was "within 170 cm of the car's centre", inside a 4 m car).
 */
inline bool CarReaches(float Along, float Side, float HalfLength, float HalfWidth, float AlongSpeedCmS, float PersonRadius = 30.f, float LeadSeconds = 0.05f)
{
	const float Forward = AlongSpeedCmS >= 0.f ? Along : -Along;
	const float Lead = std::fabs(AlongSpeedCmS) * LeadSeconds;
	if (Forward < -HalfLength || Forward > HalfLength + PersonRadius + Lead)
	{
		return false;
	}
	// In front of the leading end the whole width counts, with the person's own; along a flank only a brush does.
	const float Margin = Forward > HalfLength - 60.f ? PersonRadius : 10.f;
	return std::fabs(Side) <= HalfWidth + Margin;
}
/**
 * A body that fell stays with its person until it lies still. The crowd has fewer bodies than people: it takes a body
 * from whoever left the closest set around the player and gives it to somebody nearer. A person run over at 73 km/h
 * is 40 m behind the car two seconds later: his body, still falling, lost its physics and its collision in one frame
 * and was put elsewhere. Gate 92's long take: a person falls at 150.2 s, the line of his body down for 3 s never comes,
 * and at 153.9 s the physics engine finds a disabled body in a group that goes to sleep (an ensure, 5.3 s to write
 * its stack, the picture frozen for 6 s).
 * bFalling: the body is given to the physics. bDown: it has been down for its 3 s (the cars pass over it).
 * bAwake: one of its parts still moves. FallSeconds: since it fell. Past MaxSeconds it is taken whatever it does.
 */
constexpr float FallHoldMaxSeconds = 8.f;

inline bool BodyCanBeTaken(bool bFalling, bool bDown, bool bAwake, float FallSeconds, float MaxSeconds = FallHoldMaxSeconds)
{
	return !bFalling || (bDown && !bAwake) || FallSeconds >= MaxSeconds;
}
} // namespace GTPImpact
