#pragma once

// Health, armour, stamina, death and respawn for any body in a fight (player,
// police, shop staff), written without engine types so the rules are built and
// checked outside Unreal (tools/combat/combat_logic_test.cpp). ADR-0011.
//
// - Health 0..MaxHealth. Armour takes a share of each hit until it is gone.
// - Health comes back by itself after a quiet moment, but only up to a cap
//   (the rest needs a medkit, food or the hospital).
// - Stamina pays for punches, dodges and sprinting; it refills after a pause.
// - Death starts a timer; the respawn point is the nearest fictional hospital.
// Units: points, seconds, centimetres (Unreal world).

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <vector>

namespace GTPHealth
{
struct FSettings
{
	float MaxHealth = 100.f;
	float MaxArmor = 100.f;
	/** Share of a hit that the armour takes while it lasts (the rest hurts). */
	float ArmorAbsorb = 0.7f;

	/** Health regenerates after this long without damage (s)... */
	float RegenDelaySeconds = 6.f;
	/** ...at this rate (points per second)... */
	float RegenPerSecond = 4.f;
	/** ...but never above this share of MaxHealth. */
	float RegenCapShare = 0.5f;

	float MaxStamina = 100.f;
	float StaminaRegenDelaySeconds = 0.8f;
	float StaminaRegenPerSecond = 30.f;

	/** Time between death and respawn (fade out, hospital) (s). */
	float RespawnDelaySeconds = 6.f;
	/** Health and armour on respawn. */
	float RespawnHealthShare = 1.f;
	bool bKeepArmorOnRespawn = false;
};

struct FState
{
	float Health = 100.f;
	float Armor = 0.f;
	float Stamina = 100.f;
	float SinceDamage = 1000.f;
	float SinceStaminaSpent = 1000.f;
	bool bDead = false;
	float DeadSeconds = 0.f;
	/** Invulnerable while > 0 (dodge window, respawn grace) (s). */
	float InvulnerableSeconds = 0.f;
};

struct FDamageResult
{
	float HealthLost = 0.f;
	float ArmorLost = 0.f;
	bool bKilled = false;
	/** Nothing happened: dead already, invulnerable or zero damage. */
	bool bIgnored = false;
};

inline FState MakeState(const FSettings& S, float Armor = 0.f)
{
	FState State;
	State.Health = S.MaxHealth;
	State.Armor = std::clamp(Armor, 0.f, S.MaxArmor);
	State.Stamina = S.MaxStamina;
	return State;
}

/** Applies Amount points of damage. bIgnoreArmor: falls, fire, a head shot at point blank. */
inline FDamageResult ApplyDamage(FState& State, const FSettings& S, float Amount, bool bIgnoreArmor = false)
{
	FDamageResult R;
	if (State.bDead || Amount <= 0.f || State.InvulnerableSeconds > 0.f)
	{
		R.bIgnored = true;
		return R;
	}
	float ToHealth = Amount;
	if (!bIgnoreArmor && State.Armor > 0.f)
	{
		const float ArmorShare = std::min(Amount * S.ArmorAbsorb, State.Armor);
		State.Armor -= ArmorShare;
		R.ArmorLost = ArmorShare;
		ToHealth = Amount - ArmorShare;
	}
	R.HealthLost = std::min(ToHealth, State.Health);
	State.Health -= R.HealthLost;
	State.SinceDamage = 0.f;
	if (State.Health <= 0.f)
	{
		State.Health = 0.f;
		State.bDead = true;
		State.DeadSeconds = 0.f;
		R.bKilled = true;
	}
	return R;
}

/** Heals (medkit, food). Not capped by the regeneration cap. Returns the points added. */
inline float Heal(FState& State, const FSettings& S, float Amount)
{
	if (State.bDead || Amount <= 0.f)
	{
		return 0.f;
	}
	const float Before = State.Health;
	State.Health = std::min(S.MaxHealth, State.Health + Amount);
	return State.Health - Before;
}

inline float AddArmor(FState& State, const FSettings& S, float Amount)
{
	const float Before = State.Armor;
	State.Armor = std::clamp(State.Armor + Amount, 0.f, S.MaxArmor);
	return State.Armor - Before;
}

/** Spends stamina; refuses (and spends nothing) when there is not enough. */
inline bool SpendStamina(FState& State, float Cost)
{
	if (State.bDead || State.Stamina < Cost)
	{
		return false;
	}
	State.Stamina -= Cost;
	State.SinceStaminaSpent = 0.f;
	return true;
}

/** One step. Returns true on the step where a dead body is ready to respawn. */
inline bool Tick(FState& State, const FSettings& S, float Dt)
{
	if (Dt <= 0.f)
	{
		return false;
	}
	State.InvulnerableSeconds = std::max(0.f, State.InvulnerableSeconds - Dt);
	if (State.bDead)
	{
		const bool bWasWaiting = State.DeadSeconds < S.RespawnDelaySeconds;
		State.DeadSeconds += Dt;
		return bWasWaiting && State.DeadSeconds >= S.RespawnDelaySeconds;
	}
	State.SinceDamage += Dt;
	State.SinceStaminaSpent += Dt;
	const float Cap = S.MaxHealth * S.RegenCapShare;
	if (State.SinceDamage >= S.RegenDelaySeconds && State.Health < Cap)
	{
		State.Health = std::min(Cap, State.Health + S.RegenPerSecond * Dt);
	}
	if (State.SinceStaminaSpent >= S.StaminaRegenDelaySeconds)
	{
		State.Stamina = std::min(S.MaxStamina, State.Stamina + S.StaminaRegenPerSecond * Dt);
	}
	return false;
}

/** Back to life at the hospital, with a short grace (no damage) (s). */
inline void Respawn(FState& State, const FSettings& S, float GraceSeconds = 3.f)
{
	const float KeptArmor = S.bKeepArmorOnRespawn ? State.Armor : 0.f;
	State = MakeState(S, KeptArmor);
	State.Health = S.MaxHealth * std::clamp(S.RespawnHealthShare, 0.01f, 1.f);
	State.InvulnerableSeconds = GraceSeconds;
}

/** A fictional hospital: where the player wakes up (world cm, yaw in degrees). */
struct FHospital
{
	float X = 0.f;
	float Y = 0.f;
	float Z = 0.f;
	float YawDeg = 0.f;
};

/** Index of the hospital nearest to the place of death (horizontal), -1 when none. */
inline int PickHospital(const std::vector<FHospital>& Hospitals, float DeathX, float DeathY)
{
	int Best = -1;
	float BestD2 = 0.f;
	for (size_t I = 0; I < Hospitals.size(); ++I)
	{
		const float Dx = Hospitals[I].X - DeathX;
		const float Dy = Hospitals[I].Y - DeathY;
		const float D2 = Dx * Dx + Dy * Dy;
		if (Best < 0 || D2 < BestD2)
		{
			Best = static_cast<int>(I);
			BestD2 = D2;
		}
	}
	return Best;
}
} // namespace GTPHealth
