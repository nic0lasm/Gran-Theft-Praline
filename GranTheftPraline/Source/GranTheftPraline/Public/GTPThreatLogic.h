#pragma once

// How a person reacts to being threatened (held up): fear, hands up, complying, backing
// away, fleeing, falling when hit. No Unreal types, checked offline by
// tools/ragdoll/impact_sim. One fear for the whole game (the shop hold-up script and the
// pedestrians read this one). Units: centimetres and seconds; fear is 0..1.

#include <algorithm>
#include <cmath>

namespace GTPThreat
{
enum class ETemperament : int
{
	/** Does what he is told. */
	Compliant = 0,
	/** Freezes with the hands up and does not run, even when shots are fired. */
	Frozen = 1,
	/** Holds his ground, backs away, alarms; hands up only when really afraid. */
	Brave = 2,
	/** Runs at the first real threat. */
	Fleeing = 3
};

enum class EState : int
{
	Calm = 0,
	Alarmed = 1,
	HandsUp = 2,
	/** Hands up and obeying orders (the shop script opens the till on this). */
	Complying = 3,
	Backing = 4,
	Fleeing = 5,
	Down = 6
};

struct FSettings
{
	/** Fear gained per second when aimed at by an armed person, at point blank and at AimRangeCm. */
	float AimedRateNear = 0.9f;
	float AimedRateFar = 0.2f;
	float AimRangeCm = 2000.f;
	/** Being aimed at alone never frightens beyond this: only shots, blows and panic around push past it. */
	float AimedFearCap = 0.8f;
	/** Instant fear from a shot fired at him or near him (within ShotRadiusCm). */
	float ShotNearFear = 0.35f;
	float ShotAtFear = 0.6f;
	float ShotRadiusCm = 1800.f;
	/** Fear gained per second per unit of the highest fear of someone close (contagion). */
	float ContagionRate = 0.35f;
	float ContagionRadiusCm = 1200.f;
	/** Fear lost per second once the threat is gone for CalmDelaySeconds. */
	float DecayRate = 0.12f;
	float CalmDelaySeconds = 2.f;
	/** Time hands up before he obeys (s). */
	float ComplyDelay = 1.0f;
	float FrozenComplyDelay = 0.5f;
	/** Hands up with the threat gone this long: back to alarmed (s). */
	float ThreatGoneSeconds = 4.f;
	/** Fleeing lasts at least this long and ends below this fear (s). */
	float MinFleeSeconds = 6.f;
	float EndFleeFear = 0.3f;
	/** Knocked down: back on his feet after this long (the ragdoll decides; this is the state's view). */
	float DownSeconds = 3.f;
};

struct FPerception
{
	/** In the threat's aim cone, in range, in sight. */
	bool bAimedAt = false;
	/** The threat holds a weapon (an empty hand frightens no one). */
	bool bThreatArmed = true;
	float DistanceToThreatCm = 1e9f;
	/** A shot was fired at him this tick / near him this tick. */
	bool bShotAtMe = false;
	bool bShotNearby = false;
	/** Highest fear among people within ContagionRadiusCm who are not Calm. */
	float NearbyFear = 0.f;
};

struct FBrain
{
	ETemperament Temperament = ETemperament::Compliant;
	EState State = EState::Calm;
	float Fear = 0.f;
	float StateTime = 0.f;
	/** Seconds since he last was aimed at by an armed threat (large when never). */
	float SinceAimed = 1e9f;
	float SinceThreat = 1e9f;
	/** Persists until the dead flag: a dead body does nothing. */
	bool bDead = false;
};

inline float TemperamentGain(ETemperament T)
{
	switch (T)
	{
	case ETemperament::Compliant: return 1.0f;
	case ETemperament::Frozen: return 1.2f;
	case ETemperament::Brave: return 0.5f;
	default: return 1.4f;
	}
}

/** Fear above which this temperament puts its hands up / runs. */
inline float HandsUpFear(ETemperament T) { return T == ETemperament::Brave ? 0.65f : 0.4f; }
inline float FleeFear(ETemperament T)
{
	switch (T)
	{
	case ETemperament::Frozen: return 2.f; // never
	case ETemperament::Brave: return 0.97f;
	case ETemperament::Compliant: return 0.92f;
	default: return 0.45f;
	}
}

inline float Clamp01(float V) { return std::max(0.f, std::min(1.f, V)); }

inline void Enter(FBrain& B, EState S)
{
	if (B.State != S)
	{
		B.State = S;
		B.StateTime = 0.f;
	}
}

/** A hit landed (UGTPHitReactionComponent): knockdown or death takes the body down. */
inline void OnHit(FBrain& B, bool bKnockdown, bool bDeath, const FSettings& S = FSettings())
{
	(void)S;
	if (bDeath)
	{
		B.bDead = true;
		B.Fear = 1.f;
		Enter(B, EState::Down);
	}
	else if (bKnockdown)
	{
		B.Fear = std::max(B.Fear, 0.9f);
		Enter(B, EState::Down);
	}
	else
	{
		B.Fear = Clamp01(B.Fear + 0.25f); // a flinch or stagger still frightens
	}
}

/** One step (typically 0.1 s). Returns the state after the step. */
inline EState Step(FBrain& B, const FPerception& P, float Dt, const FSettings& S = FSettings())
{
	if (B.bDead)
	{
		B.State = EState::Down;
		return B.State;
	}
	B.StateTime += Dt;
	const bool bAimedArmed = P.bAimedAt && P.bThreatArmed;
	B.SinceAimed = bAimedArmed ? 0.f : B.SinceAimed + Dt;
	const bool bThreat = bAimedArmed || P.bShotAtMe || P.bShotNearby;
	B.SinceThreat = bThreat ? 0.f : B.SinceThreat + Dt;

	const float Gain = TemperamentGain(B.Temperament);
	float Delta = 0.f;
	if (bAimedArmed)
	{
		const float T = Clamp01(P.DistanceToThreatCm / std::max(1.f, S.AimRangeCm));
		float Aim = (S.AimedRateNear + (S.AimedRateFar - S.AimedRateNear) * T) * Gain * Dt;
		// Aiming alone saturates at AimedFearCap (never lowers a fear that is already higher).
		Aim = std::min(Aim, std::max(0.f, S.AimedFearCap - B.Fear));
		Delta += Aim;
	}
	if (P.bShotAtMe) Delta += S.ShotAtFear * Gain;
	else if (P.bShotNearby) Delta += S.ShotNearFear * Gain;
	if (P.NearbyFear > 0.3f) Delta += S.ContagionRate * P.NearbyFear * Gain * Dt;
	if (B.SinceThreat > S.CalmDelaySeconds && P.NearbyFear < 0.3f) Delta -= S.DecayRate * Dt;
	B.Fear = Clamp01(B.Fear + Delta);

	const float Flee = FleeFear(B.Temperament);
	const bool bShots = P.bShotAtMe || P.bShotNearby;
	switch (B.State)
	{
	case EState::Down:
		if (B.StateTime >= S.DownSeconds)
		{
			// Back up: frozen people freeze with the hands up, the others run.
			Enter(B, B.Temperament == ETemperament::Frozen ? EState::HandsUp : EState::Fleeing);
		}
		break;
	case EState::Calm:
		if (B.Fear >= 0.15f) Enter(B, EState::Alarmed);
		break;
	case EState::Alarmed:
		if (B.Fear >= Flee || (B.Temperament == ETemperament::Fleeing && B.Fear >= HandsUpFear(B.Temperament)))
		{
			Enter(B, EState::Fleeing);
		}
		else if (B.Fear >= HandsUpFear(B.Temperament) && (bAimedArmed || bShots))
		{
			Enter(B, EState::HandsUp);
		}
		else if (B.Temperament == ETemperament::Brave && bAimedArmed && P.DistanceToThreatCm < 400.f && B.Fear >= 0.3f)
		{
			Enter(B, EState::Backing);
		}
		else if (B.Fear < 0.1f && B.StateTime > 2.f)
		{
			Enter(B, EState::Calm);
		}
		break;
	case EState::Backing:
		if (B.Fear >= Flee) Enter(B, EState::Fleeing);
		else if (B.Fear >= HandsUpFear(B.Temperament)) Enter(B, EState::HandsUp);
		else if (!bAimedArmed && B.StateTime > 1.5f) Enter(B, EState::Alarmed);
		break;
	case EState::HandsUp:
		if (B.Fear >= Flee) Enter(B, EState::Fleeing);
		else if (bAimedArmed && B.StateTime >= (B.Temperament == ETemperament::Frozen ? S.FrozenComplyDelay : S.ComplyDelay))
		{
			Enter(B, EState::Complying);
		}
		else if (B.SinceAimed > S.ThreatGoneSeconds && B.Fear < 0.5f) Enter(B, EState::Alarmed);
		break;
	case EState::Complying:
		if (B.Fear >= Flee) Enter(B, EState::Fleeing);
		else if (B.SinceAimed > S.ThreatGoneSeconds) Enter(B, EState::HandsUp);
		break;
	case EState::Fleeing:
		if (B.StateTime >= S.MinFleeSeconds && B.Fear < S.EndFleeFear) Enter(B, EState::Alarmed);
		break;
	}
	return B.State;
}

/** Is this person within reach of an order from the threat (shop script: "empty the till")? */
inline bool ObeysOrders(const FBrain& B) { return B.State == EState::Complying; }

/** Does his fear spread to others (Alarmed and worse)? */
inline float SpreadFear(const FBrain& B)
{
	if (B.State == EState::Calm || B.State == EState::Down) return 0.f;
	// A runner passes the panic on while he starts running, not for ever: otherwise a crowd of runners keeps
	// every fear above the contagion floor and nobody calms down (measured: 24 people still running after 100 s).
	// Same for every state: a group of alarmed people would keep each other afraid for ever (simulated: 24 people
	// never calm down), so a state spreads its fear during its first 3 seconds only.
	if (B.StateTime > 3.f) return 0.f;
	return B.Fear;
}

/**
 * Half angle of the aim cone by distance: a gun held on somebody close is aimed at him even when the camera
 * looks a little beside (over the shoulder, the eyes are not the barrel). BaseDeg beyond FarCm, CloseDeg
 * within NearCm, linear between.
 */
inline float AimHalfAngleAt(float DistanceCm, float BaseDeg, float CloseDeg = 35.f, float NearCm = 100.f, float FarCm = 300.f)
{
	if (DistanceCm >= FarCm) return BaseDeg;
	if (DistanceCm <= NearCm) return std::max(BaseDeg, CloseDeg);
	const float T = (DistanceCm - NearCm) / (FarCm - NearCm);
	return std::max(BaseDeg, CloseDeg + (BaseDeg - CloseDeg) * T);
}

/** Aim cone test: is the point inside the cone around Aim (unit, 2D+z free) from Origin. */
inline bool InAimCone(float OriginX, float OriginY, float OriginZ, float AimX, float AimY, float AimZ,
	float PX, float PY, float PZ, float RangeCm, float HalfAngleDeg)
{
	const float Dx = PX - OriginX, Dy = PY - OriginY, Dz = PZ - OriginZ;
	const float D = std::sqrt(Dx * Dx + Dy * Dy + Dz * Dz);
	if (D < 1.f || D > RangeCm) return false;
	const float AimLen = std::sqrt(AimX * AimX + AimY * AimY + AimZ * AimZ);
	if (AimLen < 1e-4f) return false;
	const float Dot = (Dx * AimX + Dy * AimY + Dz * AimZ) / (D * AimLen);
	return Dot >= std::cos(HalfAngleDeg * 3.14159265f / 180.f);
}
} // namespace GTPThreat
