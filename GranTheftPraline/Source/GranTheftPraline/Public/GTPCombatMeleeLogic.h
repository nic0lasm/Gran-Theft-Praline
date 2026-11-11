#pragma once

// Bare-hands fighting for one fighter (player, police, shop staff), written
// without engine types (checked by tools/combat/combat_logic_test.cpp). ADR-0011.
//
// A fighter is a small state machine:
//   Idle -> Windup -> Strike (the blow lands on the first step of Strike) ->
//   Recovery -> Idle, with a combo of up to three blows when the next press
//   comes inside the combo window; the third blow is heavy (knocks down).
//   Dodge: a short step with invulnerability in its middle.
//   Guard: blocks most of a frontal blow for stamina; out of stamina the guard
//   breaks and the fighter is stunned.
//   Takedown: an instant non-lethal neutralisation of a stunned or unaware
//   target at close range, from behind or from the front when stunned.
// Stamina belongs to GTPHealth; the caller passes what is available and pays.

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace GTPMelee
{
enum class EPhase : int
{
	Idle = 0,
	Windup = 1,
	Strike = 2,
	Recovery = 3,
	Dodge = 4,
	Guard = 5,
	Stunned = 6
};

enum class EInput : int
{
	None = 0,
	Attack = 1,
	Dodge = 2,
	GuardOn = 3,
	GuardOff = 4
};

struct FSettings
{
	float WindupSeconds = 0.12f;
	float StrikeSeconds = 0.08f;
	float RecoverySeconds = 0.26f;
	/** Third blow of a combo: slower and heavier. */
	float HeavyWindupSeconds = 0.22f;
	/** A press up to this long before the end of Recovery chains the next blow. */
	float ComboWindowSeconds = 0.35f;
	int ComboLength = 3;
	float Damage[3] = {8.f, 10.f, 18.f};
	float StaminaPerBlow = 8.f;
	float RangeCm = 140.f;

	float DodgeSeconds = 0.45f;
	/** Invulnerable from DodgeIFrameStart to DodgeIFrameEnd after the dodge starts. */
	float DodgeIFrameStart = 0.05f;
	float DodgeIFrameEnd = 0.30f;
	float DodgeStamina = 18.f;

	/** Share of a frontal blow that a guard stops, and the stamina it costs. */
	float GuardBlockShare = 0.8f;
	float GuardStaminaPerBlock = 14.f;
	/** Blows within this half-angle of the facing count as frontal (degrees). */
	float GuardHalfAngleDeg = 70.f;

	float StunSeconds = 1.2f;
	/** A non-heavy blow interrupts an attacker in Windup (not in Strike). */
	bool bBlowsInterruptWindup = true;

	float TakedownRangeCm = 150.f;
	/** "From behind" means within this half-angle of the target's back (degrees). */
	float TakedownBehindHalfAngleDeg = 60.f;
};

/** What a step produced. */
struct FStep
{
	/** The blow of combo index BlowIndex lands now: the caller sweeps RangeCm in front. */
	bool bBlowLands = false;
	int BlowIndex = 0;
	float BlowDamage = 0.f;
	bool bHeavy = false;
};

enum class EReceive : int
{
	Hit = 0,
	Blocked = 1,
	Dodged = 2,
	/** Blocked, but the guard broke: stunned. */
	GuardBroken = 3
};

struct FReceiveResult
{
	EReceive Outcome = EReceive::Hit;
	/** Damage that goes through to health. */
	float Damage = 0.f;
	float StaminaCost = 0.f;
	/** The blow cut the fighter's own attack short. */
	bool bInterrupted = false;
	/** A heavy blow that landed: knockdown (the ragdoll thread's reaction). */
	bool bKnockdown = false;
};

class FFighter
{
public:
	explicit FFighter(const FSettings& InSettings = FSettings()) : S(InSettings) {}

	EPhase GetPhase() const { return Phase; }
	int GetComboIndex() const { return Combo; }
	bool IsInvulnerable() const
	{
		return Phase == EPhase::Dodge && PhaseTime >= S.DodgeIFrameStart && PhaseTime < S.DodgeIFrameEnd;
	}
	bool IsBusy() const { return Phase != EPhase::Idle && Phase != EPhase::Guard; }
	const FSettings& GetSettings() const { return S; }

	/**
	 * Feeds one input. Stamina is what the fighter has now; StaminaSpent returns what
	 * this input cost (the caller takes it from GTPHealth::FState).
	 */
	void Input(EInput In, float Stamina, float& StaminaSpent)
	{
		StaminaSpent = 0.f;
		switch (In)
		{
		case EInput::Attack:
			if (Phase == EPhase::Idle || Phase == EPhase::Guard)
			{
				if (Stamina >= S.StaminaPerBlow)
				{
					StaminaSpent = S.StaminaPerBlow;
					Combo = 0;
					Enter(EPhase::Windup);
				}
			}
			else if (Phase == EPhase::Strike || Phase == EPhase::Recovery)
			{
				// Buffered: chains when it falls inside the window before the end of Recovery.
				const float Left = Phase == EPhase::Recovery ? S.RecoverySeconds - PhaseTime : S.RecoverySeconds;
				if (Left <= S.ComboWindowSeconds && Combo + 1 < S.ComboLength && Stamina >= S.StaminaPerBlow)
				{
					bQueued = true;
				}
			}
			break;
		case EInput::Dodge:
			if ((Phase == EPhase::Idle || Phase == EPhase::Guard || Phase == EPhase::Recovery) && Stamina >= S.DodgeStamina)
			{
				StaminaSpent = S.DodgeStamina;
				bQueued = false;
				Enter(EPhase::Dodge);
			}
			break;
		case EInput::GuardOn:
			bGuardHeld = true;
			if (Phase == EPhase::Idle)
			{
				Enter(EPhase::Guard);
			}
			break;
		case EInput::GuardOff:
			bGuardHeld = false;
			if (Phase == EPhase::Guard)
			{
				Enter(EPhase::Idle);
			}
			break;
		default:
			break;
		}
	}

	/** One step. Stamina is passed to pay for a queued combo blow. */
	FStep Tick(float Dt, float Stamina, float& StaminaSpent)
	{
		FStep Out;
		StaminaSpent = 0.f;
		if (Dt <= 0.f)
		{
			return Out;
		}
		PhaseTime += Dt;
		switch (Phase)
		{
		case EPhase::Windup:
			if (PhaseTime >= WindupLength())
			{
				Enter(EPhase::Strike);
				Out.bBlowLands = true;
				Out.BlowIndex = Combo;
				Out.BlowDamage = S.Damage[std::clamp(Combo, 0, 2)];
				Out.bHeavy = IsHeavy(Combo);
			}
			break;
		case EPhase::Strike:
			if (PhaseTime >= S.StrikeSeconds)
			{
				Enter(EPhase::Recovery);
			}
			break;
		case EPhase::Recovery:
			if (bQueued && Stamina >= S.StaminaPerBlow)
			{
				bQueued = false;
				StaminaSpent = S.StaminaPerBlow;
				++Combo;
				Enter(EPhase::Windup);
			}
			else if (PhaseTime >= S.RecoverySeconds)
			{
				bQueued = false;
				Combo = 0;
				Enter(bGuardHeld ? EPhase::Guard : EPhase::Idle);
			}
			break;
		case EPhase::Dodge:
			if (PhaseTime >= S.DodgeSeconds)
			{
				Enter(bGuardHeld ? EPhase::Guard : EPhase::Idle);
			}
			break;
		case EPhase::Stunned:
			if (PhaseTime >= S.StunSeconds)
			{
				Enter(bGuardHeld ? EPhase::Guard : EPhase::Idle);
			}
			break;
		default:
			break;
		}
		return Out;
	}

	/**
	 * A blow arrives. AngleFromFacingDeg: angle between where this fighter faces and
	 * where the blow comes from (0 = straight in front). Stamina as in Input.
	 */
	FReceiveResult Receive(float Damage, bool bHeavy, float AngleFromFacingDeg, float Stamina)
	{
		FReceiveResult R;
		if (IsInvulnerable())
		{
			R.Outcome = EReceive::Dodged;
			return R;
		}
		const bool bFrontal = std::fabs(AngleFromFacingDeg) <= S.GuardHalfAngleDeg;
		if (Phase == EPhase::Guard && bFrontal)
		{
			if (Stamina >= S.GuardStaminaPerBlock && !bHeavy)
			{
				R.Outcome = EReceive::Blocked;
				R.Damage = Damage * (1.f - S.GuardBlockShare);
				R.StaminaCost = S.GuardStaminaPerBlock;
				return R;
			}
			// Out of stamina, or a heavy blow: the guard breaks.
			R.Outcome = EReceive::GuardBroken;
			R.Damage = Damage * (1.f - S.GuardBlockShare);
			R.StaminaCost = std::min(Stamina, S.GuardStaminaPerBlock);
			Enter(EPhase::Stunned);
			return R;
		}
		R.Outcome = EReceive::Hit;
		R.Damage = Damage;
		R.bKnockdown = bHeavy;
		if (Phase == EPhase::Windup && (bHeavy || S.bBlowsInterruptWindup))
		{
			R.bInterrupted = true;
			bQueued = false;
			Combo = 0;
			Enter(EPhase::Stunned);
			PhaseTime = S.StunSeconds * 0.5f; // a short stagger, not a full stun
		}
		else if (bHeavy)
		{
			bQueued = false;
			Combo = 0;
			Enter(EPhase::Stunned);
		}
		return R;
	}

	/** Forces the stunned state (a takedown attempt failed, a car brushed him). */
	void Stun()
	{
		bQueued = false;
		Combo = 0;
		Enter(EPhase::Stunned);
	}

private:
	bool IsHeavy(int Index) const { return Index == S.ComboLength - 1; }
	float WindupLength() const { return IsHeavy(Combo) ? S.HeavyWindupSeconds : S.WindupSeconds; }
	void Enter(EPhase P)
	{
		Phase = P;
		PhaseTime = 0.f;
	}

	FSettings S;
	EPhase Phase = EPhase::Idle;
	float PhaseTime = 0.f;
	int Combo = 0;
	bool bQueued = false;
	bool bGuardHeld = false;
};

/**
 * Can the attacker neutralise the target in one move? Distance in cm; AngleFromBackDeg:
 * angle between the target's back direction and the direction toward the attacker
 * (0 = right behind). bAware: the target knows about the attacker (threat state Alarmed
 * or worse, or fighting).
 */
inline bool CanTakedown(const FSettings& S, float DistanceCm, float AngleFromBackDeg, bool bTargetStunned, bool bAware, bool bTargetArmedAndAiming)
{
	if (DistanceCm > S.TakedownRangeCm)
	{
		return false;
	}
	if (bTargetStunned)
	{
		return true;
	}
	const bool bBehind = std::fabs(AngleFromBackDeg) <= S.TakedownBehindHalfAngleDeg;
	return bBehind && !bAware && !bTargetArmedAndAiming;
}
} // namespace GTPMelee
