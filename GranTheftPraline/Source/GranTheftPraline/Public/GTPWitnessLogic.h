#pragma once

// Who saw a crime, written without engine types so it can be tested outside
// Unreal (tools/vehicles/wanted_sim/witness_test.cpp). Units: metres, seconds.
//
// - A police unit that can see the crime within PoliceRange reports it at once.
// - A person who can see it within PedestrianRange calls the police: the
//   report comes CallDelay seconds later.
// - Loud crimes (shots, killings) are also heard without sight: by police
//   within LoudPoliceRange, by people within LoudPedestrianRange.
// - Somebody directly involved (the driver pulled out of a car, a person
//   hit) counts as a person who saw it, at distance zero.
// - Nobody saw or heard: the crime is not reported.

#include <cmath>
#include <vector>

struct FGTPWitness
{
	float X = 0.f;
	float Y = 0.f;
	bool bPolice = false;
	/** Clear line of sight to the crime (traced by the game). */
	bool bCanSee = false;
};

struct FGTPWitnessSettings
{
	float PoliceRange = 60.f;
	float PedestrianRange = 25.f;
	float LoudPoliceRange = 120.f;
	float LoudPedestrianRange = 40.f;
	float CallDelay = 4.f;
};

struct FGTPWitnessResult
{
	bool bReported = false;
	/** Seconds before the police learn of it (0 = at once). */
	float Delay = 0.f;
	/** A police unit saw or heard it: no call needed. */
	bool bByPolice = false;
	/** The person it was done to tells the police. */
	bool bVictim = false;
	/** People (not police) who saw or heard it. */
	int Persons = 0;
};

namespace GTPWitness
{
	inline FGTPWitnessResult Evaluate(float CrimeX, float CrimeY, bool bLoud, bool bVictimInvolved,
		const std::vector<FGTPWitness>& Witnesses, const FGTPWitnessSettings& S)
	{
		FGTPWitnessResult Out;
		Out.bVictim = bVictimInvolved;
		for (const FGTPWitness& W : Witnesses)
		{
			const float D = std::hypot(W.X - CrimeX, W.Y - CrimeY);
			if (W.bPolice)
			{
				Out.bByPolice = Out.bByPolice || (W.bCanSee && D <= S.PoliceRange) || (bLoud && D <= S.LoudPoliceRange);
			}
			else if ((W.bCanSee && D <= S.PedestrianRange) || (bLoud && D <= S.LoudPedestrianRange))
			{
				++Out.Persons;
			}
		}
		Out.bReported = Out.bByPolice || Out.bVictim || Out.Persons > 0;
		// Police on the spot: no call needed. Otherwise somebody has to phone.
		Out.Delay = (Out.bReported && !Out.bByPolice) ? S.CallDelay : 0.f;
		return Out;
	}
}
