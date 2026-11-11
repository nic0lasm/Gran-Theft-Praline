#pragma once

// A firearm (version 0.7: a fictional 9 mm pistol), written without engine types
// so its rules are checked outside Unreal (tools/combat/combat_logic_test.cpp).
// ADR-0011.
//
// - Magazine and reserve, semi-automatic fire rate, reload that can be cut short
//   (sprint, dodge) and refills the magazine only when it completes.
// - Spread: base cone (hip or aimed) plus a bloom that grows with each shot and
//   with movement, and recovers over time. Recoil kicks the camera pitch; the
//   kick comes back on its own.
// - Aim assist (gamepad only): snap to the best target when the aim starts,
//   and friction (slower turn) while the reticle crosses a target.
// - Seeded random numbers so a replayed fight is identical (-DemoDeterministic).

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <vector>

#include "GTPDamageLogic.h"

namespace GTPWeapon
{
struct FSpec
{
	int MagazineSize = 12;
	int MaxReserve = 96;
	float RoundsPerMinute = 330.f;
	/** Holding the trigger does not fire again (semi-automatic). */
	bool bAutomatic = false;
	float ReloadSeconds = 1.5f;
	/** An empty magazine starts a reload by itself when there is ammunition. */
	bool bAutoReload = true;
	float RangeM = 60.f;

	/** Half-angle of the cone in degrees. */
	float HipSpreadDeg = 3.0f;
	float AimSpreadDeg = 0.5f;
	float BloomPerShotDeg = 0.7f;
	float MaxBloomDeg = 4.0f;
	float BloomRecoveryDegPerSec = 6.0f;
	/** Extra spread at full running speed (scaled by speed share). */
	float MoveSpreadDeg = 2.0f;
	/** Blind fire from cover multiplies the spread. */
	float BlindFireSpreadFactor = 4.0f;

	float RecoilPitchDeg = 1.4f;
	float RecoilYawJitterDeg = 0.35f;
	float RecoilRecoveryDegPerSec = 10.f;

	GTPDamage::FWeaponDamage Damage;
};

enum class EFire : int
{
	Fired = 0,
	/** Trigger pulled on an empty gun with nothing to reload: dry click. */
	Empty = 1,
	/** Too soon after the last shot. */
	Cooldown = 2,
	Reloading = 3,
	/** Semi-automatic: release the trigger first. */
	NeedsRelease = 4
};

struct FState
{
	int Magazine = 12;
	int Reserve = 36;
	float Cooldown = 0.f;
	float ReloadLeft = 0.f;
	float BloomDeg = 0.f;
	float RecoilPitchDeg = 0.f;
	bool bTriggerHeld = false;
	/** Camera on the right shoulder (true) or the left one. */
	bool bRightShoulder = true;

	bool IsReloading() const { return ReloadLeft > 0.f; }
};

/** Small deterministic generator (xorshift32): same seed, same fight. */
struct FRandom
{
	uint32_t Seed = 0x9E3779B9u;
	explicit FRandom(uint32_t InSeed = 0x9E3779B9u) : Seed(InSeed ? InSeed : 0x9E3779B9u) {}
	uint32_t Next()
	{
		Seed ^= Seed << 13;
		Seed ^= Seed >> 17;
		Seed ^= Seed << 5;
		return Seed;
	}
	/** Uniform in [0, 1). */
	float Unit() { return static_cast<float>(Next() >> 8) * (1.f / 16777216.f); }
	/** Uniform in [-1, 1). */
	float Signed() { return Unit() * 2.f - 1.f; }
};

struct FShot
{
	/** Deviation of this shot from the aim line, degrees (yaw right, pitch up). */
	float YawDeg = 0.f;
	float PitchDeg = 0.f;
	/** Camera kick of this shot (degrees, pitch up and yaw). */
	float KickPitchDeg = 0.f;
	float KickYawDeg = 0.f;
};

inline FState MakeState(const FSpec& S, int Reserve)
{
	FState State;
	State.Magazine = S.MagazineSize;
	State.Reserve = std::clamp(Reserve, 0, S.MaxReserve);
	return State;
}

/** Current half-angle of the cone (degrees). SpeedShare: 0 still, 1 running. */
inline float SpreadDeg(const FState& State, const FSpec& S, bool bAiming, float SpeedShare, bool bBlindFire = false)
{
	float Deg = (bAiming ? S.AimSpreadDeg : S.HipSpreadDeg) + State.BloomDeg + S.MoveSpreadDeg * std::clamp(SpeedShare, 0.f, 1.f);
	if (bBlindFire)
	{
		Deg *= S.BlindFireSpreadFactor;
	}
	return Deg;
}

/** Starts a reload. False when full, already reloading or nothing in reserve. */
inline bool StartReload(FState& State, const FSpec& S)
{
	if (State.IsReloading() || State.Magazine >= S.MagazineSize || State.Reserve <= 0)
	{
		return false;
	}
	State.ReloadLeft = S.ReloadSeconds;
	return true;
}

/** Sprint, dodge, melee, entering a car: the reload stops, nothing is lost. */
inline void CancelReload(FState& State) { State.ReloadLeft = 0.f; }

/**
 * Pulls the trigger. On Fired, Shot holds the deviation (inside the cone, denser near
 * the centre) and the camera kick. The caller traces along the deviated line.
 */
inline EFire PullTrigger(FState& State, const FSpec& S, bool bAiming, float SpeedShare, bool bBlindFire, FRandom& Rng, FShot& Shot)
{
	const bool bWasHeld = State.bTriggerHeld;
	State.bTriggerHeld = true;
	if (State.IsReloading())
	{
		return EFire::Reloading;
	}
	if (!S.bAutomatic && bWasHeld)
	{
		return EFire::NeedsRelease;
	}
	if (State.Cooldown > 0.f)
	{
		return EFire::Cooldown;
	}
	if (State.Magazine <= 0)
	{
		if (S.bAutoReload && StartReload(State, S))
		{
			return EFire::Reloading;
		}
		return EFire::Empty;
	}
	const float Cone = SpreadDeg(State, S, bAiming, SpeedShare, bBlindFire);
	// Square root of a uniform radius would spread evenly over the disc; a plain
	// uniform radius keeps more shots near the centre, which reads as fair.
	const float Radius = Cone * Rng.Unit();
	const float Angle = 6.2831853f * Rng.Unit();
	Shot.YawDeg = Radius * std::cos(Angle);
	Shot.PitchDeg = Radius * std::sin(Angle);
	Shot.KickPitchDeg = S.RecoilPitchDeg;
	Shot.KickYawDeg = S.RecoilYawJitterDeg * Rng.Signed();

	--State.Magazine;
	State.Cooldown = S.RoundsPerMinute > 0.f ? 60.f / S.RoundsPerMinute : 0.f;
	State.BloomDeg = std::min(S.MaxBloomDeg, State.BloomDeg + S.BloomPerShotDeg);
	State.RecoilPitchDeg += S.RecoilPitchDeg;
	if (State.Magazine == 0 && S.bAutoReload)
	{
		StartReload(State, S);
	}
	return EFire::Fired;
}

inline void ReleaseTrigger(FState& State) { State.bTriggerHeld = false; }

/**
 * One step. Returns the camera pitch to give back this step (degrees, positive =
 * down), so the view returns to where the player aimed.
 */
inline float Tick(FState& State, const FSpec& S, float Dt)
{
	if (Dt <= 0.f)
	{
		return 0.f;
	}
	State.Cooldown = std::max(0.f, State.Cooldown - Dt);
	State.BloomDeg = std::max(0.f, State.BloomDeg - S.BloomRecoveryDegPerSec * Dt);
	if (State.ReloadLeft > 0.f)
	{
		State.ReloadLeft -= Dt;
		if (State.ReloadLeft <= 0.f)
		{
			State.ReloadLeft = 0.f;
			const int Take = std::min(S.MagazineSize - State.Magazine, State.Reserve);
			State.Magazine += Take;
			State.Reserve -= Take;
		}
	}
	const float Back = std::min(State.RecoilPitchDeg, S.RecoilRecoveryDegPerSec * Dt);
	State.RecoilPitchDeg -= Back;
	return Back;
}

/** Picks up ammunition; returns what was taken (the reserve is capped). */
inline int AddAmmo(FState& State, const FSpec& S, int Rounds)
{
	const int Before = State.Reserve;
	State.Reserve = std::clamp(State.Reserve + Rounds, 0, S.MaxReserve);
	return State.Reserve - Before;
}
} // namespace GTPWeapon

namespace GTPAimAssist
{
struct FSettings
{
	/** Snap when the aim starts: targets within this angle of the view (degrees)... */
	float SnapConeDeg = 9.f;
	/** ...and this distance (metres). */
	float MaxDistanceM = 40.f;
	/** Friction: turn rate scaled down to FrictionScale inside FrictionConeDeg. */
	float FrictionConeDeg = 3.5f;
	float FrictionScale = 0.45f;
	/** Each metre of distance weighs like this many degrees when ranking targets. */
	float DegPerMetre = 0.15f;
	/** Hostile targets (police shooting, armed staff) are preferred by this many degrees. */
	float HostileBonusDeg = 3.f;
};

struct FTarget
{
	/** Angle between the view direction and the target's chest (degrees). */
	float AngleDeg = 0.f;
	float DistanceM = 0.f;
	/** Line of sight checked by the caller. */
	bool bVisible = true;
	bool bHostile = false;
	/** Dead, surrendered (hands up and obeying) or the player's own car: never snapped to. */
	bool bExcluded = false;
};

/** Index of the target to snap to when the aim starts, -1 when none qualifies. */
inline int PickSnapTarget(const std::vector<FTarget>& Targets, const FSettings& S = FSettings())
{
	int Best = -1;
	float BestScore = 0.f;
	for (size_t I = 0; I < Targets.size(); ++I)
	{
		const FTarget& T = Targets[I];
		if (!T.bVisible || T.bExcluded || T.AngleDeg > S.SnapConeDeg || T.DistanceM > S.MaxDistanceM)
		{
			continue;
		}
		const float Score = T.AngleDeg + T.DistanceM * S.DegPerMetre - (T.bHostile ? S.HostileBonusDeg : 0.f);
		if (Best < 0 || Score < BestScore)
		{
			Best = static_cast<int>(I);
			BestScore = Score;
		}
	}
	return Best;
}

/** Look-rate multiplier while the reticle is AngleDeg away from the nearest valid target. */
inline float Friction(float AngleDeg, const FSettings& S = FSettings())
{
	if (AngleDeg >= S.FrictionConeDeg || S.FrictionConeDeg <= 0.f)
	{
		return 1.f;
	}
	const float T = std::max(0.f, AngleDeg) / S.FrictionConeDeg;
	return S.FrictionScale + (1.f - S.FrictionScale) * T;
}
} // namespace GTPAimAssist
