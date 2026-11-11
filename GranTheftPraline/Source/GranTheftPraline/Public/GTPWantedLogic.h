#pragma once

// Wanted level, written without engine types so it can be simulated outside
// Unreal (tools/vehicles/wanted_sim). Units: seconds, metres, km/h.
//
// - Crimes add "heat". Stars are a step function of the heat (1 to 5).
// - Police who see the player keep the level; once nobody has seen the
//   player for the search time (longer at higher levels) the level is lost.
// - Arrest: a police unit within ArrestRange of a player who is on foot or
//   nearly stopped, for ArrestSeconds in a row.

#include <algorithm>

enum class EGTPCrimeKind
{
	CarTheft,
	HitPedestrian,
	HitVehicle,
	HitPolice,
	/** Combat (docs/gameplay/combat): a person hurt, a weapon fired or pointed, a person or a police officer killed. */
	Assault,
	ShotsFired,
	ArmedThreat,
	Killing,
	KillPolice
};

constexpr int GTPCrimeKindCount = 9;

struct FGTPWantedSettings
{
	/** Heat needed for 1 .. 5 stars. */
	float StarHeat[5] = {20.f, 60.f, 120.f, 220.f, 360.f};

	float HeatCarTheft = 25.f;
	float HeatHitPedestrian = 35.f;
	float HeatHitVehicle = 12.f;
	float HeatHitPolice = 45.f;
	float HeatAssault = 15.f;
	float HeatShotsFired = 20.f;
	float HeatArmedThreat = 25.f;
	float HeatKilling = 60.f;
	float HeatKillPolice = 130.f; // 3 stars at once: more than a killing (2 stars)

	/** Weapon crimes come in bursts: shots and threats count once per this time (s), not once per CrimeCooldown. */
	float BurstCooldown = 5.f;

	/** The same kind of crime does not count twice within this time (a car dragging a person). */
	float CrimeCooldown = 2.0f;

	float SearchBaseSeconds = 14.f;
	float SearchPerStarSeconds = 4.f;

	float ArrestRange = 7.f;
	float ArrestSeconds = 3.f;
	float ArrestMaxSpeedKmh = 12.f;

	/** Stars are never higher than this (M1 budget: fewer police cars on screen). */
	int MaxStars = 5;
};

enum class EGTPWantedPhase
{
	Clear,
	/** Police have seen the player within the search time. */
	Pursuit,
	/** Nobody has seen the player: the flashing stars of the search. */
	Searching
};

class FGTPWantedState
{
public:
	explicit FGTPWantedState(const FGTPWantedSettings& InSettings = FGTPWantedSettings()) : Settings(InSettings) {}

	int GetStars() const
	{
		int Stars = 0;
		for (int I = 0; I < 5; ++I)
		{
			if (Heat >= Settings.StarHeat[I])
			{
				Stars = I + 1;
			}
		}
		return std::min(Stars, Settings.MaxStars);
	}

	float GetHeat() const { return Heat; }
	EGTPWantedPhase GetPhase() const { return GetStars() == 0 ? EGTPWantedPhase::Clear : (SearchLeft < SearchTotal ? EGTPWantedPhase::Searching : EGTPWantedPhase::Pursuit); }
	/** 0 = just lost sight, 1 = about to lose the level; 0 in pursuit. */
	float GetSearchProgress() const { return SearchTotal > 0.f ? 1.f - SearchLeft / SearchTotal : 0.f; }
	float GetArrestProgress() const { return Settings.ArrestSeconds > 0.f ? ArrestTime / Settings.ArrestSeconds : 0.f; }

	/** Adds a crime at Time (s); returns true when the heat changed. */
	bool AddCrime(EGTPCrimeKind Kind, float Time)
	{
		const int Index = static_cast<int>(Kind);
		const bool bBurst = Kind == EGTPCrimeKind::ShotsFired || Kind == EGTPCrimeKind::ArmedThreat;
		if (Time - LastCrimeTime[Index] < (bBurst ? Settings.BurstCooldown : Settings.CrimeCooldown))
		{
			return false;
		}
		LastCrimeTime[Index] = Time;
		float Add = 0.f;
		switch (Kind)
		{
		case EGTPCrimeKind::CarTheft: Add = Settings.HeatCarTheft; break;
		case EGTPCrimeKind::HitPedestrian: Add = Settings.HeatHitPedestrian; break;
		case EGTPCrimeKind::HitVehicle: Add = Settings.HeatHitVehicle; break;
		case EGTPCrimeKind::HitPolice: Add = Settings.HeatHitPolice; break;
		case EGTPCrimeKind::Assault: Add = Settings.HeatAssault; break;
		case EGTPCrimeKind::ShotsFired: Add = Settings.HeatShotsFired; break;
		case EGTPCrimeKind::ArmedThreat: Add = Settings.HeatArmedThreat; break;
		case EGTPCrimeKind::Killing: Add = Settings.HeatKilling; break;
		case EGTPCrimeKind::KillPolice: Add = Settings.HeatKillPolice; break;
		}
		Heat = std::min(Heat + Add, Settings.StarHeat[4] + 40.f);
		RestartSearch();
		return true;
	}

	/** One step. bSeen: some police unit has the player in sight now. */
	void Tick(float DeltaSeconds, bool bSeen)
	{
		if (GetStars() == 0)
		{
			Heat = 0.f;
			SearchLeft = SearchTotal = 0.f;
			return;
		}
		if (bSeen)
		{
			RestartSearch();
			return;
		}
		SearchLeft -= DeltaSeconds;
		if (SearchLeft <= 0.f)
		{
			Clear();
		}
	}

	/** Returns true when the player is arrested this step. */
	bool UpdateArrest(float DeltaSeconds, float NearestPoliceMetres, float PlayerSpeedKmh, bool bOnFoot)
	{
		const bool bClose = NearestPoliceMetres >= 0.f && NearestPoliceMetres <= Settings.ArrestRange;
		const bool bHelpless = bOnFoot || PlayerSpeedKmh <= Settings.ArrestMaxSpeedKmh;
		if (GetStars() > 0 && bClose && bHelpless)
		{
			ArrestTime += DeltaSeconds;
		}
		else
		{
			ArrestTime = std::max(0.f, ArrestTime - 2.f * DeltaSeconds);
		}
		return ArrestTime >= Settings.ArrestSeconds;
	}

	/** Sets the level directly (tests, video): Stars 0 to 5. */
	void ForceStars(int Stars)
	{
		Stars = Stars < 0 ? 0 : (Stars > 5 ? 5 : Stars);
		Heat = Stars == 0 ? 0.f : Settings.StarHeat[Stars - 1];
		ArrestTime = 0.f;
		if (Stars > 0)
		{
			RestartSearch();
		}
	}

	void Clear()
	{
		Heat = 0.f;
		SearchLeft = SearchTotal = 0.f;
		ArrestTime = 0.f;
	}

	const FGTPWantedSettings& GetSettings() const { return Settings; }

private:
	void RestartSearch()
	{
		SearchTotal = Settings.SearchBaseSeconds + Settings.SearchPerStarSeconds * static_cast<float>(GetStars());
		SearchLeft = SearchTotal;
	}

	FGTPWantedSettings Settings;
	float Heat = 0.f;
	float SearchLeft = 0.f;
	float SearchTotal = 0.f;
	float ArrestTime = 0.f;
	float LastCrimeTime[GTPCrimeKindCount] = {-1000.f, -1000.f, -1000.f, -1000.f, -1000.f, -1000.f, -1000.f, -1000.f, -1000.f};
};

namespace GTPWanted
{
	/** Police cars sent for a number of stars (M1 budget, ADR-0004), never more than MaxUnits. */
	inline int UnitsForStars(int Stars, int MaxUnits)
	{
		static const int Table[6] = {0, 1, 2, 2, 3, 4};
		const int Wanted = Table[Stars < 0 ? 0 : (Stars > 5 ? 5 : Stars)];
		return std::min(Wanted, MaxUnits);
	}

	/** Top speed (km/h) of a police car in pursuit: a bit faster at each level, never faster than a city car can hold the road. */
	inline float ChaseSpeedKmh(int Stars)
	{
		return 55.f + 10.f * static_cast<float>(Stars < 0 ? 0 : (Stars > 5 ? 5 : Stars));
	}
}
