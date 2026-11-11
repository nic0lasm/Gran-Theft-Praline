#pragma once

// Localised hit reactions for any body (player, pedestrian, later enemies): which part
// was hit, from where, how hard, and what the body does about it (flinch, stagger,
// knockdown, death). No Unreal types, checked offline by tools/ragdoll/impact_sim.
// The combat system fills FHitInfo and calls UGTPHitReactionComponent::ReactToHit;
// the rules here never decide damage, only the reaction to it (ADR-0006, 0.7).

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace GTPHit
{
enum class EZone : int
{
	Torso = 0,
	Head = 1,
	Arm = 2,
	Leg = 3
};

enum class EDirection : int
{
	Front = 0,
	Back = 1,
	Left = 2,
	Right = 3
};

enum class EReaction : int
{
	None = 0,
	/** A short flinch, the body keeps doing what it does. */
	Flinch = 1,
	/** Staggers: slowed for a moment. */
	Stagger = 2,
	/** Falls: ragdoll, gets up. */
	Knockdown = 3,
	/** Dies: ragdoll, stays down. */
	Death = 4
};

struct FSettings
{
	/** Damage as a share of full health (100) where each reaction starts. */
	float FlinchShare = 0.06f;
	float StaggerShare = 0.20f;
	float KnockdownShare = 0.45f;
	/** A head hit counts this much more. */
	float HeadFactor = 1.5f;
	/** A limb hit counts this much less. */
	float LimbFactor = 0.7f;
	/** Reactions closer than this are ignored, except a death or a knockdown (s). */
	float CooldownSeconds = 0.35f;
	/** Flinches inside this window add up: the third one staggers (s, count). */
	float ChainWindowSeconds = 2.0f;
	int ChainCount = 3;
	/** After a knockdown the body cannot be knocked down again for this long (s). */
	float KnockdownImmunitySeconds = 2.0f;
	/** Death and knockdown launch: base and per point of damage (cm/s), bounded. */
	float LaunchBaseCmS = 200.f;
	float LaunchPerDamage = 6.f;
	float MaxLaunchCmS = 650.f;
};

struct FHitInfo
{
	float Damage = 0.f;
	/** Health left after this hit (0..100). */
	float HealthAfter = 100.f;
	/** Horizontal direction the hit travels (from the attacker to the body), world x/y. */
	float DirX = 1.f;
	float DirY = 0.f;
	/** Heading of the body (degrees, 0 = +x, counter-clockwise like Unreal's yaw seen from above). */
	float FacingYawDeg = 0.f;
	EZone Zone = EZone::Torso;
	/** A melee blow pushes harder than its damage says. */
	bool bMelee = false;
	/** A blow that must put the body on the ground whatever its damage (a finisher, a heavy punch). */
	bool bHeavy = false;
};

struct FReaction
{
	EReaction Kind = EReaction::None;
	EDirection Direction = EDirection::Front;
	EZone Zone = EZone::Torso;
	/** Velocity change for a knockdown or a death (cm/s, horizontal along the hit plus a lift). */
	float LaunchX = 0.f;
	float LaunchY = 0.f;
	float LaunchZ = 0.f;
	/** Spin for a head hit that kills or knocks down (rad/s). */
	float SpinRadS = 0.f;
};

/** Zone from a UE mannequin bone name (lower case, any suffix like _l / _r / _01). */
inline EZone ZoneFromBone(const std::string& Bone)
{
	auto Has = [&](const char* Part) { return Bone.find(Part) != std::string::npos; };
	if (Has("head") || Has("neck")) return EZone::Head;
	if (Has("clavicle") || Has("upperarm") || Has("lowerarm") || Has("hand") || Has("finger") || Has("thumb")) return EZone::Arm;
	if (Has("thigh") || Has("calf") || Has("foot") || Has("ball") || Has("toe")) return EZone::Leg;
	return EZone::Torso;
}

/** Where the hit came from, relative to the body's facing. */
inline EDirection DirectionOf(float DirX, float DirY, float FacingYawDeg)
{
	const float Pi = 3.14159265f;
	const float Yaw = FacingYawDeg * Pi / 180.f;
	const float Fx = std::cos(Yaw), Fy = std::sin(Yaw);
	// The hit travels along Dir: it comes from the opposite side.
	const float FromX = -DirX, FromY = -DirY;
	const float Forward = FromX * Fx + FromY * Fy;
	const float Side = FromX * (-Fy) + FromY * Fx; // positive: from the body's left (counter-clockwise)
	if (std::fabs(Forward) >= std::fabs(Side)) return Forward >= 0.f ? EDirection::Front : EDirection::Back;
	return Side >= 0.f ? EDirection::Left : EDirection::Right;
}

/** Name of the keyed clip for a flinch or stagger (tools/animation/car_poses.json). */
inline const char* ClipName(EReaction Kind, EDirection Direction)
{
	static const char* Flinch[4] = {"AM_HitLight_Front", "AM_HitLight_Back", "AM_HitLight_Left", "AM_HitLight_Right"};
	static const char* Stagger[4] = {"AM_Stagger_Front", "AM_Stagger_Back", "AM_HitLight_Left", "AM_HitLight_Right"};
	if (Kind == EReaction::Flinch) return Flinch[static_cast<int>(Direction)];
	if (Kind == EReaction::Stagger) return Stagger[static_cast<int>(Direction)];
	return "";
}

/** Time-aware state: cooldown, flinch chain, knockdown immunity. One per body. */
struct FState
{
	double LastReaction = -1e9;
	double ChainStart = -1e9;
	int ChainHits = 0;
	double KnockdownUntil = -1e9;
	bool bDead = false;
};

inline FReaction Evaluate(const FHitInfo& Hit, FState& State, double Now, const FSettings& S = FSettings())
{
	FReaction R;
	R.Zone = Hit.Zone;
	R.Direction = DirectionOf(Hit.DirX, Hit.DirY, Hit.FacingYawDeg);
	if (State.bDead)
	{
		return R;
	}
	const float Length = std::sqrt(Hit.DirX * Hit.DirX + Hit.DirY * Hit.DirY);
	const float Dx = Length > 1e-4f ? Hit.DirX / Length : 1.f;
	const float Dy = Length > 1e-4f ? Hit.DirY / Length : 0.f;

	float Share = std::max(0.f, Hit.Damage) / 100.f;
	if (Hit.Zone == EZone::Head) Share *= S.HeadFactor;
	else if (Hit.Zone == EZone::Arm || Hit.Zone == EZone::Leg) Share *= S.LimbFactor;
	if (Hit.bMelee) Share *= 1.25f;

	const auto Launch = [&](float Scale) {
		const float Speed = std::min(S.MaxLaunchCmS, (S.LaunchBaseCmS + S.LaunchPerDamage * Hit.Damage) * Scale);
		R.LaunchX = Dx * Speed;
		R.LaunchY = Dy * Speed;
		R.LaunchZ = 80.f;
		if (Hit.Zone == EZone::Head) R.SpinRadS = 3.f;
	};

	if (Hit.HealthAfter <= 0.f)
	{
		R.Kind = EReaction::Death;
		Launch(1.f);
		State.bDead = true;
		State.LastReaction = Now;
		return R;
	}
	if (Share < S.FlinchShare && !Hit.bHeavy)
	{
		return R;
	}
	EReaction Kind = (Hit.bHeavy || Share >= S.KnockdownShare) ? EReaction::Knockdown : (Share >= S.StaggerShare ? EReaction::Stagger : EReaction::Flinch);

	// Cooldown: a knockdown always goes through, the rest wait (and do not count in the chain).
	if (Kind != EReaction::Knockdown && Now - State.LastReaction < S.CooldownSeconds)
	{
		return R;
	}
	// Flinches pile up: the third inside the window staggers.
	if (Kind == EReaction::Flinch)
	{
		if (Now - State.ChainStart > S.ChainWindowSeconds)
		{
			State.ChainStart = Now;
			State.ChainHits = 0;
		}
		if (++State.ChainHits >= S.ChainCount)
		{
			Kind = EReaction::Stagger;
			State.ChainHits = 0;
			State.ChainStart = Now;
		}
	}
	if (Kind == EReaction::Knockdown && Now < State.KnockdownUntil)
	{
		Kind = EReaction::Stagger; // already down a moment ago: no endless knockdowns
	}
	R.Kind = Kind;
	State.LastReaction = Now;
	if (Kind == EReaction::Knockdown)
	{
		Launch(0.6f);
		State.KnockdownUntil = Now + S.KnockdownImmunitySeconds;
	}
	return R;
}
} // namespace GTPHit
