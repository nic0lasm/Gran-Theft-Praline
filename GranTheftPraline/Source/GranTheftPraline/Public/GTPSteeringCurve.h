#pragma once

// How much of the steering asked the front wheels of a car get, by its speed. Written without engine types so the
// script that drives the filmed flight can undo it (GTPFlightPedals.h) and be tested outside Unreal.
//
// AGTPVehicle gives these keys to the Chaos vehicle (SteeringSetup.SteeringCurve): "less steering at speed so the
// car stays drivable on a keyboard". Chaos reads the curve by the car's forward speed in miles per hour.

#include <cmath>

namespace GTPSteeringCurve
{
	struct FKey
	{
		float Speed;    // as the vehicle reads it (mph)
		float Share;    // of the steering asked, 0..1
	};

	constexpr FKey Keys[] = {{0.f, 1.f}, {30.f, 0.75f}, {80.f, 0.3f}, {120.f, 0.22f}};
	constexpr int KeyCount = sizeof(Keys) / sizeof(Keys[0]);
	constexpr float MphPerKmh = 0.621371f;

	/** The share of the steering asked that the wheels get at this speed (km/h): straight lines between the keys. */
	inline float ShareAtKmh(float Kmh)
	{
		const float Speed = std::fabs(Kmh) * MphPerKmh;
		if (Speed <= Keys[0].Speed)
		{
			return Keys[0].Share;
		}
		for (int Index = 1; Index < KeyCount; ++Index)
		{
			if (Speed <= Keys[Index].Speed)
			{
				const float T = (Speed - Keys[Index - 1].Speed) / (Keys[Index].Speed - Keys[Index - 1].Speed);
				return Keys[Index - 1].Share + (Keys[Index].Share - Keys[Index - 1].Share) * T;
			}
		}
		return Keys[KeyCount - 1].Share;
	}
}
