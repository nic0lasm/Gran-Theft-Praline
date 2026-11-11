#pragma once

// How much a hit hurts and what it looks like where it lands, written without
// engine types (checked by tools/combat/combat_logic_test.cpp). ADR-0011.
//
// - Zone from the bone name (head, torso, arm, leg) and a factor per zone.
// - Distance falloff for bullets: full damage up to FalloffStart, then a line
//   down to MinFactor at FalloffEnd.
// - The player takes a share of what NPC weapons deal (difficulty), NPCs take
//   all of it.
// - Impact effect per surface: the surface ids are those of GTPSurfaceLogic.h
//   (footsteps) plus glass, car body and flesh, which the trace knows from the
//   hit actor. The effect kind is all Unreal needs to pick a Niagara system,
//   a decal and a sound.

#include <algorithm>
#include <cstddef>
#include <cctype>
#include <cstdint>
#include <string>

namespace GTPDamage
{
enum class EZone : int
{
	Torso = 0,
	Head = 1,
	Arm = 2,
	Leg = 3
};

/** Zone of a skeleton bone by keyword (UE mannequin and GASP names). Unknown = torso. */
inline EZone ZoneFromBone(const char* Bone)
{
	if (!Bone || !*Bone)
	{
		return EZone::Torso;
	}
	std::string B(Bone);
	for (char& C : B)
	{
		C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
	}
	auto Has = [&B](const char* Key) { return B.find(Key) != std::string::npos; };
	if (Has("head") || Has("neck"))
	{
		return EZone::Head;
	}
	if (Has("thigh") || Has("calf") || Has("foot") || Has("ball_") || Has("leg") || Has("knee"))
	{
		return EZone::Leg;
	}
	if (Has("clavicle") || Has("upperarm") || Has("lowerarm") || Has("hand") || Has("arm") || Has("finger") || Has("thumb")
		|| Has("index_") || Has("middle_") || Has("ring_") || Has("pinky"))
	{
		return EZone::Arm;
	}
	return EZone::Torso;
}

struct FWeaponDamage
{
	float Base = 20.f;
	/** Bullets only: full damage up to FalloffStartM, MinFactor from FalloffEndM (metres). */
	float FalloffStartM = 15.f;
	float FalloffEndM = 45.f;
	float MinFactor = 0.5f;
	float HeadFactor = 2.5f;
	float ArmFactor = 0.6f;
	float LegFactor = 0.7f;
	/** Melee blows and point-blank head shots go through armour. */
	bool bIgnoresArmor = false;
};

struct FDifficulty
{
	/** Share of the damage the player takes from NPC weapons. */
	float PlayerTakes = 0.45f;
	/** Head shots on the player count this much at most (no one-shot deaths at full health). */
	float PlayerMaxHeadFactor = 1.5f;
};

inline float ZoneFactor(const FWeaponDamage& W, EZone Zone)
{
	switch (Zone)
	{
	case EZone::Head: return W.HeadFactor;
	case EZone::Arm: return W.ArmFactor;
	case EZone::Leg: return W.LegFactor;
	default: return 1.f;
	}
}

inline float FalloffFactor(const FWeaponDamage& W, float DistanceM)
{
	if (DistanceM <= W.FalloffStartM || W.FalloffEndM <= W.FalloffStartM)
	{
		return 1.f;
	}
	if (DistanceM >= W.FalloffEndM)
	{
		return W.MinFactor;
	}
	const float T = (DistanceM - W.FalloffStartM) / (W.FalloffEndM - W.FalloffStartM);
	return 1.f + (W.MinFactor - 1.f) * T;
}

/** Points of damage of one hit, before armour (GTPHealth::ApplyDamage takes it from there). */
inline float ComputeDamage(const FWeaponDamage& W, EZone Zone, float DistanceM, bool bTargetIsPlayer, const FDifficulty& D = FDifficulty())
{
	float Zf = ZoneFactor(W, Zone);
	float Scale = 1.f;
	if (bTargetIsPlayer)
	{
		Zf = std::min(Zf, D.PlayerMaxHeadFactor);
		Scale = D.PlayerTakes;
	}
	return std::max(0.f, W.Base * Zf * FalloffFactor(W, DistanceM) * Scale);
}

/** What a bullet hit, as far as effects go. */
enum class EImpact : int
{
	/** Asphalt, stone, cobbles: dust puff and chips. */
	Dust = 0,
	/** Metal (poles, railings): sparks. */
	Sparks = 1,
	/** Car body: sparks and a dent decal. */
	CarBody = 2,
	/** Windows, shop glass, car glass: glass shards, crack decal. */
	Glass = 3,
	/** Wood (benches, doors): splinters. */
	Wood = 4,
	/** Grass, gravel, soil: dirt kick. */
	Dirt = 5,
	/** Water (Rhône, puddles): splash. */
	Water = 6,
	/** A body: small, short blood puff (no gore pools: rating and budget). */
	Flesh = 7,
	Count = 8
};

/** Surface ids of GTPSurfaceLogic.h (kept in sync by the test). */
namespace SurfaceId
{
constexpr uint8_t Asphalt = 0;
constexpr uint8_t Stone = 1;
constexpr uint8_t Cobble = 2;
constexpr uint8_t Gravel = 3;
constexpr uint8_t Grass = 4;
constexpr uint8_t Metal = 5;
constexpr uint8_t Wood = 6;
constexpr uint8_t Water = 7;
} // namespace SurfaceId

/** The trace knows more than the surface: a pawn, a vehicle, glass (by material). */
struct FImpactContext
{
	uint8_t SurfaceId = SurfaceId::Asphalt;
	bool bHitPawn = false;
	bool bHitVehicle = false;
	bool bGlass = false;
};

inline EImpact ImpactFor(const FImpactContext& C)
{
	if (C.bHitPawn)
	{
		return EImpact::Flesh;
	}
	if (C.bGlass)
	{
		return EImpact::Glass;
	}
	if (C.bHitVehicle)
	{
		return EImpact::CarBody;
	}
	switch (C.SurfaceId)
	{
	case SurfaceId::Metal: return EImpact::Sparks;
	case SurfaceId::Wood: return EImpact::Wood;
	case SurfaceId::Grass:
	case SurfaceId::Gravel: return EImpact::Dirt;
	case SurfaceId::Water: return EImpact::Water;
	default: return EImpact::Dust;
	}
}

/**
 * Cost class of an impact effect (budget, see docs/gameplay/combat.md): particles
 * spawned at most. Glass shards are the dearest; they are capped first.
 */
inline int MaxParticles(EImpact Impact)
{
	switch (Impact)
	{
	case EImpact::Glass: return 48;
	case EImpact::Sparks:
	case EImpact::CarBody: return 24;
	case EImpact::Water: return 20;
	case EImpact::Flesh: return 8;
	default: return 16;
	}
}
} // namespace GTPDamage
