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
	/** Units must average this, or MinPlayerShare of the player's own average when the player is slower. */
	float MinMeanKmh = 30.f;
	float MinPlayerShare = 0.6f;
	/** Share of the time with a unit within NearMetres. */
	float MinNearShare = 0.25f;
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
	/** Times a unit left behind was put back near the player, out of sight (the distance then says less). */
	int Moves = 0;
	int MovesLost = 0;

	// The frame being filled.
	int FrameUnits = 0;
	float FrameKmh = 0.f;
	int FrameMoving = 0;
	float FrameNearest = -1.f;

	void Reset() { *this = FGTPChaseTally(); }
	void NoteMoved() { ++Moves; }
	/** A move that the car did not keep (found far from where it was put half a second later). */
	void NoteMoveLost() { ++MovesLost; }

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

	/** The speed the units have to average: the floor, or less when the player himself is slow (a chase through a jam). */
	inline float WantedMeanKmh(const FGTPChaseTally& T, const FGTPChaseTallySettings& S)
	{
		return std::min(S.MinMeanKmh, S.MinPlayerShare * T.PlayerMeanKmh());
	}

	inline EVerdict Judge(const FGTPChaseTally& T, const FGTPChaseTallySettings& S)
	{
		if (T.Seconds < S.MinSeconds)
		{
			return EVerdict::TooShort;
		}
		if (T.MeanKmh() < WantedMeanKmh(T, S))
		{
			return EVerdict::TooSlow;
		}
		if (T.NearShare() < S.MinNearShare)
		{
			return EVerdict::TooFar;
		}
		return EVerdict::Follows;
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
