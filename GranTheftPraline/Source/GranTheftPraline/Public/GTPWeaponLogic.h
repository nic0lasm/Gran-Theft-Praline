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
// - The arsenal (pistol, carbine), the loadout that switches between them, and the
//   rules of shooting from a car window (GTPArsenal, GTPDriveBy).

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

	/** Held in one hand: the only kind that can be fired from a car window. */
	bool bOneHanded = true;

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

namespace GTPArsenal
{
enum class EKind : int
{
	Pistol = 0,
	/** The long weapon: a fictional automatic carbine. */
	Carbine = 1,
	Count = 2
};

/** The fictional 9 mm pistol: the FSpec defaults. */
inline GTPWeapon::FSpec Pistol()
{
	return GTPWeapon::FSpec();
}

/**
 * The fictional carbine: automatic, a lighter kick per shot but a climb that adds up,
 * wider from the hip, longer reach, harder hits. Officer: 4 torso shots at 10 m.
 */
inline GTPWeapon::FSpec Carbine()
{
	GTPWeapon::FSpec S;
	S.MagazineSize = 30;
	S.MaxReserve = 180;
	S.RoundsPerMinute = 600.f;
	S.bAutomatic = true;
	S.ReloadSeconds = 2.2f;
	S.RangeM = 90.f;
	S.HipSpreadDeg = 4.5f;
	S.AimSpreadDeg = 0.6f;
	S.BloomPerShotDeg = 0.35f;
	S.MaxBloomDeg = 3.5f;
	S.BloomRecoveryDegPerSec = 8.f;
	S.MoveSpreadDeg = 3.f;
	S.RecoilPitchDeg = 0.75f;
	S.RecoilYawJitterDeg = 0.45f;
	S.RecoilRecoveryDegPerSec = 4.f;
	S.bOneHanded = false;
	S.Damage.Base = 26.f;
	S.Damage.FalloffStartM = 30.f;
	S.Damage.FalloffEndM = 75.f;
	S.Damage.HeadFactor = 2.f;
	return S;
}

inline GTPWeapon::FSpec Make(EKind Kind)
{
	return Kind == EKind::Carbine ? Carbine() : Pistol();
}

/**
 * The weapons a body carries, one in hand. Switching takes SwitchSeconds (nothing fires
 * meanwhile), stops the reload of the weapon put away and keeps its magazine.
 */
struct FLoadout
{
	std::vector<GTPWeapon::FSpec> Specs;
	std::vector<GTPWeapon::FState> States;
	std::vector<bool> Owned;
	int Current = 0;
	float SwitchLeft = 0.f;

	static FLoadout Make(const std::vector<EKind>& Kinds, const std::vector<int>& Reserves)
	{
		FLoadout L;
		const int Count = static_cast<int>(EKind::Count);
		for (int K = 0; K < Count; ++K)
		{
			L.Specs.push_back(GTPArsenal::Make(static_cast<EKind>(K)));
			L.States.push_back(GTPWeapon::MakeState(L.Specs.back(), 0));
			L.Owned.push_back(false);
		}
		bool bFirst = true;
		for (size_t I = 0; I < Kinds.size(); ++I)
		{
			const size_t K = static_cast<size_t>(Kinds[I]);
			if (K >= L.Specs.size())
			{
				continue;
			}
			L.Owned[K] = true;
			const int Reserve = I < Reserves.size() ? Reserves[I] : L.Specs[K].MagazineSize * 2;
			L.States[K] = GTPWeapon::MakeState(L.Specs[K], Reserve);
			if (bFirst)
			{
				L.Current = static_cast<int>(K);
				bFirst = false;
			}
		}
		return L;
	}

	GTPWeapon::FSpec& Spec() { return Specs[static_cast<size_t>(Current)]; }
	const GTPWeapon::FSpec& Spec() const { return Specs[static_cast<size_t>(Current)]; }
	GTPWeapon::FState& State() { return States[static_cast<size_t>(Current)]; }
	const GTPWeapon::FState& State() const { return States[static_cast<size_t>(Current)]; }
	EKind CurrentKind() const { return static_cast<EKind>(Current); }
	bool IsSwitching() const { return SwitchLeft > 0.f; }
	bool Has(EKind Kind) const
	{
		const size_t K = static_cast<size_t>(Kind);
		return K < Owned.size() && Owned[K];
	}

	/** Gives a weapon (a pickup, a mission); returns false when already carried. */
	bool Give(EKind Kind, int Reserve)
	{
		const size_t K = static_cast<size_t>(Kind);
		if (K >= Owned.size())
		{
			return false;
		}
		if (Owned[K])
		{
			GTPWeapon::AddAmmo(States[K], Specs[K], Reserve);
			return false;
		}
		Owned[K] = true;
		States[K] = GTPWeapon::MakeState(Specs[K], Reserve);
		return true;
	}

	/** Takes Kind in hand. False when not carried or already in hand. */
	bool Select(EKind Kind, float SwitchSeconds)
	{
		if (!Has(Kind) || Kind == CurrentKind())
		{
			return false;
		}
		GTPWeapon::CancelReload(State());
		GTPWeapon::ReleaseTrigger(State());
		const bool bRight = State().bRightShoulder;
		Current = static_cast<int>(Kind);
		State().bRightShoulder = bRight;
		SwitchLeft = std::max(0.f, SwitchSeconds);
		return true;
	}

	/** The next carried weapon after the one in hand (Step +1 or -1); the same one when alone. */
	EKind Next(int Step = 1) const
	{
		const int Count = static_cast<int>(Owned.size());
		for (int I = 1; I <= Count; ++I)
		{
			const int K = ((Current + Step * I) % Count + Count) % Count;
			if (Owned[static_cast<size_t>(K)])
			{
				return static_cast<EKind>(K);
			}
		}
		return CurrentKind();
	}

	/** First carried one-handed weapon (drive-by), Count when none. */
	EKind FirstOneHanded() const
	{
		for (size_t K = 0; K < Owned.size(); ++K)
		{
			if (Owned[K] && Specs[K].bOneHanded)
			{
				return static_cast<EKind>(K);
			}
		}
		return EKind::Count;
	}

	/**
	 * Picks up Rounds for Kind (a weapon on the ground): carried already = ammunition up to the
	 * reserve cap, else the weapon itself. Returns the rounds taken (0 = nothing to take).
	 */
	int Collect(EKind Kind, int Rounds)
	{
		const size_t K = static_cast<size_t>(Kind);
		if (K >= Owned.size() || Rounds <= 0)
		{
			return 0;
		}
		if (!Owned[K])
		{
			Give(Kind, Rounds);
			return States[K].Reserve;
		}
		return GTPWeapon::AddAmmo(States[K], Specs[K], Rounds);
	}

	/** Arrest: every carried weapon loses its magazine and reserve (it stays carried, empty). */
	void StripAll()
	{
		for (GTPWeapon::FState& S : States)
		{
			GTPWeapon::CancelReload(S);
			S.Magazine = 0;
			S.Reserve = 0;
		}
	}

	/** Advances the switch; returns true on the step it ends. */
	bool TickSwitch(float Dt)
	{
		if (SwitchLeft <= 0.f || Dt <= 0.f)
		{
			return false;
		}
		SwitchLeft -= Dt;
		if (SwitchLeft <= 0.f)
		{
			SwitchLeft = 0.f;
			return true;
		}
		return false;
	}
};
} // namespace GTPArsenal

namespace GTPPickup
{
// Weapons left on the ground, GTA-like: a body that dies armed, or is disarmed by a heavy
// blow or a takedown, drops its gun with some ammunition; walking over it picks it up.
struct FSettings
{
	/** A dropped gun always holds at least this, at most two magazines. */
	int MinRounds = 6;
	/** Share of the reserve that falls with the gun (the rest is lost). */
	float ReserveShare = 0.5f;
};

inline int DropRounds(int Magazine, int Reserve, int MagazineSize, const FSettings& S = FSettings())
{
	const int Rounds = std::max(0, Magazine) + static_cast<int>(static_cast<float>(std::max(0, Reserve)) * S.ReserveShare);
	return std::clamp(Rounds, S.MinRounds, std::max(S.MinRounds, 2 * MagazineSize));
}

/** Whether this hit makes an armed body let go of its gun. bDrawn: gun in hand. */
inline bool Disarms(bool bMelee, bool bHeavy, bool bKilled, bool bCarries, bool bDrawn)
{
	if (!bCarries)
	{
		return false;
	}
	return bKilled || (bMelee && bHeavy && bDrawn);
}
} // namespace GTPPickup

namespace GTPDriveBy
{
// Shooting from a car the player drives: one-handed weapons only, out of the side
// windows and the windscreen, never straight back through the driver's own seat;
// wider spread that grows with the car's speed.
struct FSettings
{
	/** Largest angle between the car's nose and the aim, either side (degrees). */
	float MaxYawDeg = 150.f;
	/** Aim pitch allowed from a seat (degrees, up positive). */
	float MinPitchDeg = -30.f;
	float MaxPitchDeg = 35.f;
	/** Spread multiplier while seated, and the extra cone at FullSpeedMS (degrees). */
	float SpreadFactor = 1.6f;
	float SpeedSpreadDeg = 2.5f;
	float FullSpeedMS = 30.f;
};

/** YawDeg: aim relative to the car's nose (-180..180, right positive). */
inline bool CanShoot(float YawDeg, float PitchDeg, const FSettings& S = FSettings())
{
	return std::fabs(YawDeg) <= S.MaxYawDeg && PitchDeg >= S.MinPitchDeg && PitchDeg <= S.MaxPitchDeg;
}

/** Extra cone (degrees) from the car's speed. */
inline float SpeedSpreadDeg(float SpeedMS, const FSettings& S = FSettings())
{
	if (S.FullSpeedMS <= 0.f)
	{
		return 0.f;
	}
	return S.SpeedSpreadDeg * std::clamp(std::fabs(SpeedMS) / S.FullSpeedMS, 0.f, 1.f);
}

enum class EWindow : int
{
	Left = 0,
	Windscreen = 1,
	Right = 2
};

/** Which opening the shot leaves by (the arm and the glass that breaks). */
inline EWindow WindowFor(float YawDeg)
{
	if (YawDeg < -30.f)
	{
		return EWindow::Left;
	}
	return YawDeg > 30.f ? EWindow::Right : EWindow::Windscreen;
}
} // namespace GTPDriveBy
