#pragma once

// Cars of the traffic left in the street by their driver: what becomes of them. Written without engine types
// (tools/vehicles/wanted_sim/left_cars_test.cpp). Units: metres.
//
// A driver of the traffic gets out and runs from a threat (shots, a gun aimed at him: AGTPVehicle::PanicExit). His
// car then stood on its lane for good: the network forgot it the moment it had no driver, and only the cars of a
// crash were ever taken away (AGTPTrafficNetwork::UpdateIncidents). The traffic queues behind it, and so does the
// filmed flight, whose way was kept clear of the cars that have a driver only. Read in the code. Seen in gate 91's
// film: at 62 s the flight stops behind three cars standing in its lane beside officers on foot, and is arrested
// there at 70 s; that their drivers had run is likely and in no line read so far.

namespace GTPLeftCars
{
	enum class EFate
	{
		/** Somebody is getting in or out, or it stands where the player can see it: looked at again later. */
		Keep,
		/** Driven again, the player's, or a wreck: no longer a car left in the street. */
		Forget,
		/** Empty, far and out of sight: taken away. */
		TakeAway,
	};

	struct FCar
	{
		/** A driver of the traffic is at the wheel again. */
		bool bDrivenAgain = false;
		/** The player drives it or is getting in. */
		bool bPlayers = false;
		bool bWrecked = false;
		/** Nobody in it and nobody getting in or out. */
		bool bEmpty = false;
		float ToPlayerM = 0.f;
		bool bSeenByPlayer = false;
	};

	inline EFate Fate(const FCar& Car, float RemoveM)
	{
		if (Car.bDrivenAgain || Car.bPlayers || Car.bWrecked)
		{
			return EFate::Forget;
		}
		return Car.bEmpty && Car.ToPlayerM > RemoveM && !Car.bSeenByPlayer ? EFate::TakeAway : EFate::Keep;
	}

	/**
	 * Is a car in the way of the filmed flight? AlongM ahead of the player's car and AsideM beside the line it points
	 * along: within 35 m ahead and 2.5 m aside, the cars beside him, behind him or coming the other way stay.
	 */
	inline bool InTheWay(float AlongM, float AsideM)
	{
		return AlongM > 0.f && AlongM < 35.f && AsideM > -2.5f && AsideM < 2.5f;
	}
}
