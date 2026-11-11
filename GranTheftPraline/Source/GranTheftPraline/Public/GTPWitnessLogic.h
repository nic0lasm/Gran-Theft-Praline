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
};

namespace GTPWitness
{
	inline FGTPWitnessResult Evaluate(float CrimeX, float CrimeY, bool bLoud, bool bVictimInvolved,
		const std::vector<FGTPWitness>& Witnesses, const FGTPWitnessSettings& S)
	{
		FGTPWitnessResult Out;
		if (bVictimInvolved)
		{
			Out.bReported = true;
			Out.Delay = S.CallDelay;
		}
		for (const FGTPWitness& W : Witnesses)
		{
			const float D = std::hypot(W.X - CrimeX, W.Y - CrimeY);
			if (W.bPolice)
			{
				if ((W.bCanSee && D <= S.PoliceRange) || (bLoud && D <= S.LoudPoliceRange))
				{
					// Police on the spot: no call needed.
					Out.bReported = true;
					Out.Delay = 0.f;
					return Out;
				}
			}
			else if ((W.bCanSee && D <= S.PedestrianRange) || (bLoud && D <= S.LoudPedestrianRange))
			{
				if (!Out.bReported || S.CallDelay < Out.Delay)
				{
					Out.Delay = S.CallDelay;
				}
				Out.bReported = true;
			}
		}
		return Out;
	}
}
