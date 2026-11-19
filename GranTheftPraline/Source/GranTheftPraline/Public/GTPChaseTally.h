#pragma once

// What a player being chased would see of the police, in numbers, written without engine types so it
// can be tested outside Unreal (tools/vehicles/wanted_sim/chase_tally_test.cpp). Units: km/h, metres, seconds.
//
// The police director feeds it every frame while the player is wanted: one AddUnit per chasing car
// (roadblock cars stand still by design and are left out), then EndFrame. -GTARun and the chase demo read it.
// At candidate 43 the units averaged 1 km/h in a chase that the test called a success on stars and
// unit count alone: a pursuit is cars that move and stay near, so that is what is counted here.

#include <algorithm>

struct FGTPChaseTallySettings
{
	/** A unit slower than this is not moving (a car that creeps or spins its wheels on the spot). */
	float MovingKmh = 10.f;
	/** A unit this close is in the player's mirror. */
	float NearMetres = 80.f;
	/** The verdict needs at least this long with a unit on the road. */
	float MinSeconds = 10.f;
	/**
	 * Units must average MinPlayerShare of the player's own average, and never less than FloorKmh.
	 * The floor is fixed: the streets of the quarter are posted at 20 to 30 km/h, and a police car that averages
	 * less than the ordinary traffic there is not pursuing anybody, however slow the car it follows. Without it a
	 * slower player made the step easier (gate of candidate 49: 22 km/h passed against 20 wanted, for a player at 33).
	 */
	float FloorKmh = 25.f;
	float MinPlayerShare = 0.6f;
	/** Units that stay this close on average are on the player whatever their speed (a player crawling in a jam, boxed in). */
	float CloseMetres = 30.f;
	/** Share of the time with a unit within NearMetres. */
	float MinNearShare = 0.25f;
	/** A replacement made nearer the player than this did not drive to him: the pursuit was helped. */
	float ReplacementMinMetres = 150.f;
};

struct FGTPChaseTally
{
	// Sums over the frames with at least one chasing unit.
	double Seconds = 0.0;
	double UnitSeconds = 0.0;
	double UnitKmhSeconds = 0.0;
	double MovingUnitSeconds = 0.0;
	double NearestMetreSeconds = 0.0;
	double NearSeconds = 0.0;
	double PlayerKmhSeconds = 0.0;
	float NearestMin = -1.f;
	float UnitPeakKmh = 0.f;
	int PeakUnits = 0;
	/** Times a unit held against something backed off, and units given up (off their wheels, or held in the
	 *  same place after backing off): a given-up car is a wreck, no longer counted among the chasing cars. */
	int BackOffs = 0;
	int GivenUp = 0;
	/** Chasing cars sent (made on a lane out of the player's sight), and those of them sent to replace a car that was
	 *  lost (left behind, given up, wrecked) once the level had its cars. ReplacementNearest: how near the player the
	 *  nearest of those was made (metres, -1 = none): a replacement made close hands the player a pursuer that has
	 *  not driven to him, and the distance to the nearest unit then says nothing of how the police drive. */
	int Sent = 0;
	int Replacements = 0;
	float ReplacementNearest = -1.f;

	// The frame being filled.
	int FrameUnits = 0;
	float FrameKmh = 0.f;
	int FrameMoving = 0;
	float FrameNearest = -1.f;

	void Reset() { *this = FGTPChaseTally(); }
	void NoteBackedOff() { ++BackOffs; }
	void NoteGivenUp() { ++GivenUp; }
	void NoteSent(float MetresFromPlayer, bool bReplacement)
	{
		++Sent;
		if (bReplacement)
		{
			++Replacements;
			ReplacementNearest = ReplacementNearest < 0.f ? MetresFromPlayer : std::min(ReplacementNearest, MetresFromPlayer);
		}
	}

	void AddUnit(float SpeedKmh, float DistanceMetres, const FGTPChaseTallySettings& S)
	{
		const float Kmh = SpeedKmh < 0.f ? -SpeedKmh : SpeedKmh;
		++FrameUnits;
		FrameKmh += Kmh;
		FrameMoving += Kmh >= S.MovingKmh ? 1 : 0;
		FrameNearest = FrameNearest < 0.f ? DistanceMetres : std::min(FrameNearest, DistanceMetres);
		UnitPeakKmh = std::max(UnitPeakKmh, Kmh);
	}

	/** Closes the frame; a frame without a unit counts for nothing (nobody to see). */
	void EndFrame(float DeltaSeconds, float PlayerKmh, const FGTPChaseTallySettings& S)
	{
		if (FrameUnits > 0 && DeltaSeconds > 0.f)
		{
			Seconds += DeltaSeconds;
			UnitSeconds += static_cast<double>(DeltaSeconds) * FrameUnits;
			UnitKmhSeconds += static_cast<double>(DeltaSeconds) * FrameKmh;
			MovingUnitSeconds += static_cast<double>(DeltaSeconds) * FrameMoving;
			NearestMetreSeconds += static_cast<double>(DeltaSeconds) * FrameNearest;
			NearSeconds += FrameNearest <= S.NearMetres ? DeltaSeconds : 0.f;
			PlayerKmhSeconds += static_cast<double>(DeltaSeconds) * (PlayerKmh < 0.f ? -PlayerKmh : PlayerKmh);
			NearestMin = NearestMin < 0.f ? FrameNearest : std::min(NearestMin, FrameNearest);
			PeakUnits = std::max(PeakUnits, FrameUnits);
		}
		FrameUnits = 0;
		FrameKmh = 0.f;
		FrameMoving = 0;
		FrameNearest = -1.f;
	}

	/** Average speed of a chasing unit (each unit weighs the time it was on the road). */
	float MeanKmh() const { return UnitSeconds > 0.0 ? static_cast<float>(UnitKmhSeconds / UnitSeconds) : 0.f; }
	/** Share of the unit time spent moving (0..1). */
	float MovingShare() const { return UnitSeconds > 0.0 ? static_cast<float>(MovingUnitSeconds / UnitSeconds) : 0.f; }
	/** Average distance from the player to the nearest unit. */
	float MeanNearest() const { return Seconds > 0.0 ? static_cast<float>(NearestMetreSeconds / Seconds) : -1.f; }
	/** Share of the time with a unit within NearMetres (0..1). */
	float NearShare() const { return Seconds > 0.0 ? static_cast<float>(NearSeconds / Seconds) : 0.f; }
	float PlayerMeanKmh() const { return Seconds > 0.0 ? static_cast<float>(PlayerKmhSeconds / Seconds) : 0.f; }
};

namespace GTPChase
{
	enum class EVerdict
	{
		Follows,
		/** Not enough time with a unit on the road to say. */
		TooShort,
		/** The units do not drive: slower than the floor and than the player. */
		TooSlow,
		/** They drive but are almost never near the player. */
		TooFar
	};

	/** The speed the units have to average: a share of the player's own, and never under the fixed floor. */
	inline float WantedMeanKmh(const FGTPChaseTally& T, const FGTPChaseTallySettings& S)
	{
		return std::max(S.FloorKmh, S.MinPlayerShare * T.PlayerMeanKmh());
	}

	inline EVerdict Judge(const FGTPChaseTally& T, const FGTPChaseTallySettings& S)
	{
		if (T.Seconds < S.MinSeconds)
		{
			return EVerdict::TooShort;
		}
		if (T.MeanKmh() < WantedMeanKmh(T, S) && T.MeanNearest() > S.CloseMetres)
		{
			return EVerdict::TooSlow;
		}
		if (T.NearShare() < S.MinNearShare)
		{
			return EVerdict::TooFar;
		}
		return EVerdict::Follows;
	}

	/** True when no car was handed to the player: every replacement was made at least ReplacementMinMetres from him. */
	inline bool Unaided(const FGTPChaseTally& T, const FGTPChaseTallySettings& S)
	{
		return T.ReplacementNearest < 0.f || T.ReplacementNearest >= S.ReplacementMinMetres;
	}

	inline const char* VerdictName(EVerdict V)
	{
		switch (V)
		{
		case EVerdict::Follows: return "the police follow";
		case EVerdict::TooShort: return "no unit on the road long enough";
		case EVerdict::TooSlow: return "the units do not drive";
		default: return "the units stay far from the player";
		}
	}
}
