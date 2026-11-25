#pragma once

// How much of the steering asked the front wheels of a car get. Written without engine types so the drivers that
// are not a hand on a stick (the script of the filmed flight, the police units, the traffic) can undo it and be
// tested outside Unreal (tools/vehicles/wanted_sim/steering_input_test.cpp).
//
// Two things stand between the steering given to a car (-1..1) and the angle of its front wheels:
//
// 1. Its speed. AGTPVehicle gives the keys below to the Chaos vehicle (SteeringSetup.SteeringCurve): "less steering
//    at speed so the car stays drivable on a keyboard". Chaos reads the curve by the car's forward speed in miles
//    per hour.
//
// 2. A square. The vehicle squares the steering it is given and keeps its sign (the input curve of a Chaos vehicle's
//    steering, SteeringInputRate.InputCurveFunction, left as the engine sets it): half of the stick is a quarter of
//    the lock, a tenth of it a hundredth. Made for a thumb on a stick. Not read in the engine, measured: gate 93's
//    long take wrote where the flight's car was, where it pointed and the steering it was sent, once a second
//    (DemoDrive). Over the 82 seconds driven above 25 km/h, the turn the car made in each second is the one of a car
//    whose wheels got "the square of the steering sent, times the share of its speed" to 1.2 degrees a second; of a
//    car whose wheels got the steering sent, to 20 degrees a second, four times too much on average and eight times
//    under 0.2 of steering.
//
// The lane follower (GTPDriveLogic.h) asks for an angle of the front wheels, as a share of the lock: 0.10 to come
// back to a line 2 m away at 100 km/h. The wheels got 0.01, then under half of that. Gate 93, quay Jules Courmont:
// 2.2 m off the line for three seconds at 99 km/h; rue du President Carnot: 4.4 m off at 71 km/h. Every car driven
// by the lane follower drove like this, the police units and the traffic too.

#include <cmath>

namespace GTPSteeringCurve
{
	struct FSpeedShare
	{
		float Speed;    // as the vehicle reads it (mph)
		float Share;    // of the steering asked, 0..1
	};

	constexpr FSpeedShare Keys[] = {{0.f, 1.f}, {30.f, 0.75f}, {80.f, 0.3f}, {120.f, 0.22f}};
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

	/** What the car makes of the steering it is given (-1..1), before the share of its speed: its square, sign kept. */
	inline float Squared(float Input)
	{
		return Input * std::fabs(Input);
	}

	/** The share of the lock (-1..1) the front wheels get from the steering given to the car at this speed (km/h). */
	inline float WheelsGet(float Input, float Kmh)
	{
		const float Bounded = std::fmax(-1.f, std::fmin(1.f, Input));
		return Squared(Bounded) * ShareAtKmh(Kmh);
	}

	/**
	 * The steering to give the car so that it takes Asked (-1..1) the way a driver that reckons in wheel angles means
	 * it: the square undone, the share of the speed left to the car. For the police units and the traffic.
	 */
	inline float InputFor(float Asked)
	{
		const float Bounded = std::fmax(-1.f, std::fmin(1.f, Asked));
		return Bounded < 0.f ? -std::sqrt(-Bounded) : std::sqrt(Bounded);
	}

	/**
	 * The steering to give the car so that its front wheels get Wheels (share of the lock, -1..1) at this speed (km/h):
	 * the square and the share of the speed both undone. Past what the car gives at that speed: the whole of it.
	 */
	inline float InputForWheels(float Wheels, float Kmh)
	{
		const float Share = std::fmax(ShareAtKmh(Kmh), 0.1f);
		return InputFor(Wheels / Share);
	}
}
