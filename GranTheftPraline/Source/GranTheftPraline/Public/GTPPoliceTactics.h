// Engine-free police tactics: when officers get out of their car, when they may shoot, where they
// search after losing sight, how a foot chase closes in. Compiled by the unreal module and by the
// plain C++ test tools/vehicles/wanted_sim/tactics_test.cpp (CI job "wanted-rules").
#pragma once

#include <algorithm>
#include <cmath>

namespace GTPPoliceTactics
{
	struct FSettings
	{
		/** A car this close to a player on foot (cm) puts its officer out. */
		float DeployRange = 3500.f;
		/** A car that cannot move (blocked, stuck) deploys at this range at any level (cm). */
		float StuckDeployRange = 6000.f;
		float FootSpeedCmS = 430.f;
		float FireRange = 3500.f;
		/** Stars at which the police shoot on sight. Below, only after the player fired or threatened. */
		int ShootStars = 3;
		/** Spread multiplier handed to UGTPWeaponComponent::FireAt: the lower the level, the worse the aim. */
		float SpreadAtThree = 2.5f;
		float SpreadPerExtraStar = 0.35f;
		/** Seconds between two shots of one officer, by level. */
		float FireIntervalAtThree = 1.1f;
		float FireIntervalMin = 0.5f;
		/** Search: first ring radius and growth per second spent searching (cm). */
		float SearchRadius = 1500.f;
		float SearchGrowthCmS = 60.f;
		float SearchMaxRadius = 6000.f;
		float ArriveRadius = 150.f;
	};

	/** What the police do at each level (1 to 5): the escalation the player feels. */
	struct FProfile
	{
		int Cars = 0;
		/** Top speed of a car in pursuit (km/h) and how hard it pushes through traffic. */
		float ChaseKmh = 0.f;
		bool bRam = false;
		/** Some cars come from in front of the player, not only from behind. */
		bool bFlank = false;
		int RoadblockCars = 0;
	};

	inline FProfile Profile(int Stars, int MaxCars)
	{
		static const FProfile Table[6] = {
			{0, 0.f, false, false, 0},
			{1, 70.f, false, false, 0},
			{2, 90.f, false, true, 0},
			{3, 105.f, true, true, 2},
			{4, 120.f, true, true, 2},
			{4, 135.f, true, true, 2},
		};
		FProfile Out = Table[Stars < 0 ? 0 : (Stars > 5 ? 5 : Stars)];
		Out.Cars = std::min(Out.Cars, MaxCars);
		return Out;
	}

	/** What the probe of a unit's car meets ahead of its bumper. */
	enum class EMet
	{
		Nothing,
		Quarry,    // the man the police are after, or the car he drives
		Police,    // a police car (a unit, a roadblock car) or an officer on foot
		Person,
		Other,     // a car of the traffic, a parked car, a thing that moves when pushed
	};

	/**
	 * The obstacle a unit drives by (metres ahead of its bumper, negative: none), from what its probe met Metres ahead.
	 *
	 * A unit that rams (FProfile::bRam, from three stars) does not brake for the man it is after or the car he is in.
	 * It brakes for everything else. Until gate 101 it looked at nothing at all: at 105 to 135 km/h into the parked
	 * cars, the roadblock cars of its own side, the other units and the people on the road. Gate 93's long take,
	 * three then four stars, 60 seconds: four units with no wheel on the ground and "obstacle=-1", one of them on its
	 * roof 5 s after it was made, 1.1 m higher than it was made, one 1.6 m higher 6 s after it backed off from a car
	 * 75 cm ahead of its bumper, one that had just slowed from 44 to 6 km/h where a roadblock car stood. Read from
	 * those lines and the code: no line says what each of them climbed.
	 * What it still pushes through, from a crawl: ShovesThrough.
	 */
	inline float ObstacleFor(float Metres, EMet Met, bool bRams)
	{
		return bRams && Met == EMet::Quarry ? -1.f : Metres;
	}

	/**
	 * Does a unit held up behind what it met push through it after a moment (AGTPPoliceDirector: 1.5 s under 15 km/h)?
	 * A car in its way, and the car of the man it is after: yes. A colleague or a person: it waits, as any driver.
	 * (A unit stopped 4 s far from the player still pushes through whatever holds it.)
	 */
	inline bool ShovesThrough(EMet Met)
	{
		return Met == EMet::Other || Met == EMet::Quarry;
	}

	/** A unit is held up behind what is nearer than this ahead of its bumper (m), and under this speed (km/h) below the ramming levels. */
	constexpr float HeldUpMetres = 12.f;
	constexpr float HeldUpKmh = 15.f;

	/**
	 * Is a unit held up behind what it met, this frame? After a moment of it the unit pushes through for a few seconds.
	 * From two stars, behind something it shoves (ShovesThrough) nearer than HeldUpMetres. Under three stars only at a
	 * crawl, as before. From three (bRams) at any speed: a unit that now brakes for the cars of the traffic would
	 * follow one at 30 km/h down a whole street; it bumps it out of its way instead, from the speed of that car and
	 * not from 105 km/h.
	 */
	inline bool IsHeldUp(float Kmh, float ObstacleMetres, EMet Met, int Stars, bool bRams)
	{
		return Stars >= 2 && ObstacleMetres > 0.f && ObstacleMetres < HeldUpMetres && ShovesThrough(Met) && (bRams || Kmh < HeldUpKmh);
	}

	inline const char* MetName(EMet Met)
	{
		static const char* const Names[] = {"nothing", "quarry", "police", "person", "other"};
		return Names[static_cast<int>(Met)];
	}

	/**
	 * Is a point ahead of a moving player? Direction of travel (DirX, DirY) unit vector; the point at
	 * (Dx, Dy) from the player (m). Used to place flanking cars and roadblocks.
	 */
	inline bool IsAhead(float Dx, float Dy, float DirX, float DirY, float MinCos)
	{
		const float Length = std::sqrt(Dx * Dx + Dy * Dy);
		return Length > 1e-3f && (Dx * DirX + Dy * DirY) / Length >= MinCos;
	}

	/** Officers wanted outside their cars for a level (at most MaxOfficers). */
	inline int OfficersForStars(int Stars, int MaxOfficers)
	{
		static const int Table[6] = {0, 1, 2, 2, 3, 4};
		return std::min(Table[Stars < 0 ? 0 : (Stars > 5 ? 5 : Stars)], MaxOfficers);
	}

	/**
	 * Should the unit put an officer on the ground now?
	 * Player in a car: officers stay in theirs (cars do the work), unless the car is stuck.
	 */
	inline bool ShouldDeploy(const FSettings& S, int Stars, bool bPlayerOnFoot, float CarToPlayerCm, bool bCarStuck, int OfficersOut, int MaxOfficers)
	{
		if (Stars <= 0 || OfficersOut >= OfficersForStars(Stars, MaxOfficers))
		{
			return false;
		}
		if (bCarStuck)
		{
			return CarToPlayerCm <= S.StuckDeployRange;
		}
		return bPlayerOnFoot && CarToPlayerCm <= S.DeployRange;
	}

	inline bool ShootsOnSight(const FSettings& S, int Stars, bool bPlayerAggressive)
	{
		return Stars >= S.ShootStars || (Stars >= 2 && bPlayerAggressive);
	}

	/** Returns true when the officer may pull the trigger this frame. */
	inline bool MayFire(const FSettings& S, int Stars, bool bPlayerAggressive, float DistanceCm, bool bLineOfSight, float CooldownLeft)
	{
		return CooldownLeft <= 0.f && bLineOfSight && DistanceCm <= S.FireRange && ShootsOnSight(S, Stars, bPlayerAggressive);
	}

	inline float SpreadFactor(const FSettings& S, int Stars)
	{
		const float Extra = static_cast<float>(std::max(0, Stars - 3));
		return std::max(1.0f, S.SpreadAtThree - S.SpreadPerExtraStar * Extra);
	}

	inline float FireInterval(const FSettings& S, int Stars)
	{
		const float Extra = static_cast<float>(std::max(0, Stars - 3));
		return std::max(S.FireIntervalMin, S.FireIntervalAtThree - 0.25f * Extra);
	}

	/**
	 * Search point around the last known position for officer Index of Count, SearchSeconds after
	 * the player was lost: the officers spread over the circle and the circle widens. Angle in
	 * radians, result is an offset in the ground plane (cm).
	 */
	inline void SearchOffset(const FSettings& S, int Index, int Count, float SearchSeconds, float& OutX, float& OutY)
	{
		const float Radius = std::min(S.SearchMaxRadius, S.SearchRadius + S.SearchGrowthCmS * std::max(0.f, SearchSeconds));
		const float Step = 6.2831853f / static_cast<float>(std::max(1, Count));
		// Turn slowly so each officer sweeps a sector: a full turn takes 60 s.
		const float Angle = Step * static_cast<float>(Index) + 0.1047f * std::max(0.f, SearchSeconds);
		OutX = Radius * std::cos(Angle);
		OutY = Radius * std::sin(Angle);
	}

	/** One step of a foot chase: move toward the target at speed; true when within ArriveRadius. */
	inline bool StepToward(const FSettings& S, float& X, float& Y, float TargetX, float TargetY, float Dt)
	{
		const float Dx = TargetX - X;
		const float Dy = TargetY - Y;
		const float Distance = std::sqrt(Dx * Dx + Dy * Dy);
		if (Distance <= S.ArriveRadius)
		{
			return true;
		}
		const float Move = std::min(Distance - S.ArriveRadius * 0.5f, S.FootSpeedCmS * Dt);
		X += Dx / Distance * Move;
		Y += Dy / Distance * Move;
		return false;
	}
}
