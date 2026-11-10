#pragma once

// Street furniture that gives way: what a car or a running person does to a lamp,
// a bin, a sign. No Unreal types, checked offline by tools/ragdoll/impact_sim.
// Kinds: R = tips over, C = breaks above a threshold, F = fixed (stops the car).
// Units: kilograms, cm/s, joules.

#include <algorithm>
#include <cmath>

namespace GTPBreak
{
enum class EKind : int
{
	Fixed = 0,
	Tippable = 1,
	Breakable = 2
};

enum class EOutcome : int
{
	None = 0,
	/** The object leaves its place as a physical body, thrown by the hit. */
	Topple = 1,
	/** Same, and the broken version (or debris) replaces it. */
	Break = 2,
	/** Fixed object: the impactor is stopped, the object stays. */
	Stop = 3
};

struct FSettings
{
	/** Tippable: pushed by anything closing faster than this (km/h). */
	float TippableMinKmh = 3.f;
	/** Breakable: energy needed = Base + PerKg * object mass (joules). */
	float BreakBaseJoules = 400.f;
	float BreakJoulesPerKg = 20.f;
	/** Objects of at least HeavyMassKg are anchored: energy needed is at least this per kg (a 250 kg lamp breaks near 35 km/h under a car). */
	float HeavyMassKg = 100.f;
	float HeavyJoulesPerKg = 225.f;
	/** The object leaves with this share of the impactor's speed, shared by mass. */
	float Transfer = 1.1f;
	float MaxThrowCmS = 1500.f;
};

struct FResult
{
	EOutcome Outcome = EOutcome::None;
	/** Velocity to give the object (cm/s, along the impactor's direction). */
	float ThrowCmS = 0.f;
	/** Speed the impactor loses (km/h): the momentum given to the object. */
	float ImpactorLossKmh = 0.f;
};

inline float Energy(float ImpactorMassKg, float ClosingCmS)
{
	const float V = ClosingCmS * 0.01f;
	return 0.5f * ImpactorMassKg * V * V;
}

inline float BreakThresholdJoules(float ObjectMassKg, const FSettings& S)
{
	const float Light = S.BreakBaseJoules + S.BreakJoulesPerKg * ObjectMassKg;
	return ObjectMassKg >= S.HeavyMassKg ? std::max(Light, S.HeavyJoulesPerKg * ObjectMassKg) : Light;
}

inline FResult Resolve(EKind Kind, float ObjectMassKg, float ImpactorMassKg, float ClosingCmS, const FSettings& S = FSettings())
{
	FResult R;
	const float Kmh = ClosingCmS * 0.036f;
	if (Kind == EKind::Fixed)
	{
		R.Outcome = Kmh >= 3.f ? EOutcome::Stop : EOutcome::None;
		return R;
	}
	if (Kind == EKind::Tippable)
	{
		if (Kmh < S.TippableMinKmh) return R;
		R.Outcome = EOutcome::Topple;
	}
	else
	{
		if (Energy(ImpactorMassKg, ClosingCmS) < BreakThresholdJoules(ObjectMassKg, S))
		{
			// Too gentle to break: a light one still tips like a tippable.
			if (Kmh >= S.TippableMinKmh && ObjectMassKg <= 40.f) R.Outcome = EOutcome::Topple;
			return R.Outcome == EOutcome::None ? R : FResult{R.Outcome, std::min(S.MaxThrowCmS, ClosingCmS * S.Transfer * ImpactorMassKg / (ImpactorMassKg + ObjectMassKg)), 0.f};
		}
		R.Outcome = EOutcome::Break;
	}
	const float Share = ImpactorMassKg / (ImpactorMassKg + std::max(1.f, ObjectMassKg));
	R.ThrowCmS = std::min(S.MaxThrowCmS, ClosingCmS * S.Transfer * Share);
	// Momentum given to the object comes out of the impactor's speed.
	R.ImpactorLossKmh = (ObjectMassKg * R.ThrowCmS * 0.036f) / std::max(1.f, ImpactorMassKg);
	return R;
}
} // namespace GTPBreak
