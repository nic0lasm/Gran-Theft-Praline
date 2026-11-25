#pragma once

// The pedals of the car the chase script drives (GTPDemoRoute, the filmed flight of -GTARun), written without
// engine types so it can be tested outside Unreal (tools/vehicles/wanted_sim/flight_pedals_test.cpp).
// Units: m/s, pedals 0..1, steering -1..1.
//
// Gate of candidate 83 (2026-11-24): the stolen car was put back on a lane 6 times in one pass and 3 in the other.
// Its lines, one a second, show the same thing at each corner it left its lane at:
//   lane=131 kmh=23 throttle=0.00 steer=-0.65 want=23 limit=bend
//   lane=140 kmh=37 throttle=1.00 steer=-0.41 want=45 limit=lane     (then into the pit beside the lane)
//   lane=41  kmh=26 throttle=0.00 steer=0.03  want=24 limit=bend
//   lane=45  kmh=36 throttle=1.00 steer=1.00  want=57 limit=bend     (then 52 km/h at full lock, 27 m past the corner)
// The speed wanted jumps the moment the car's closest point is on the next lane: the lane follower then dropped the
// points of the lane left (FGTPLaneFollower::Step; it keeps the last of them since, GTPDrive::KeptBehind), and a
// corner at the very start of the path is not counted (GTPDrive::BendSpeedAt wants 3.6 m of path before a vertex). The car, still turning, gets the whole throttle with
// its front wheels at full lock: it runs wide, at 52 km/h and 27 m from its lane a second later.
//
// So with the wheels turned the script's pedals follow its steering, as the police units' do
// (GTPUnitRecovery::Tame): no throttle, some brake, while the car is faster than the wheel angle asked can be
// held. Only past a third of lock: under it the rule is not for the flight. On the quay the same lines show the
// car at 98 km/h with 0.14 of lock asked, where the rule would hold it to 46 km/h: at speed the car does not turn
// as much as its wheels say, and the lane follower's own bend limit is what slows it there. In those lines the
// steering asked never passes a third of lock above 45 km/h. The steering itself is left as asked.

#include "GTPDriveLogic.h"
#include "GTPUnitRecovery.h"

namespace GTPFlightPedals
{
	/** The throttle is never under this when the lane follower asks for some and the car is under its cruise speed. */
	constexpr float FirmThrottle = 0.7f;
	/** Within this of the cruise speed (m/s) the follower's own throttle is kept. */
	constexpr float FirmMargin = 4.f / 3.6f;
	/** The pedals follow the steering from this much lock asked (0..1): 16 degrees of the 45. */
	constexpr float TurnedLock = 0.35f;
	/** Brake per m/s over the speed the steering holds, and the most of it. */
	constexpr float BrakePerMs = 0.25f;
	constexpr float BrakeMost = 0.8f;

	/** The speed (m/s) at which the car turns as tightly as Steering (-1..1) asks, at S.MaxLateralAccel. */
	inline float HoldSpeed(float Steering, const FGTPDriveSettings& S)
	{
		FGTPUnitRecoverySettings Wheels;
		Wheels.WheelBase = S.WheelBase;
		Wheels.FullLockRadians = S.MaxSteerAngle;
		return GTPUnitRecovery::SteerSpeed(Steering, S.MaxLateralAccel, Wheels);
	}

	/**
	 * Throttle and Brake as the lane follower asked them, changed in place into what the car gets.
	 * Speed and Cruise in m/s. bFollowSteering false: only the firm throttle, as until gate 83.
	 */
	inline void Apply(float& Throttle, float& Brake, float Steering, float Speed, float Cruise, const FGTPDriveSettings& S, bool bFollowSteering = true)
	{
		const float V = std::fabs(Speed);
		// The lane follower's throttle is soft (half of the time under 0.3 below the set speed): firmer for the chase.
		// Only when it asks for throttle at all: under a bend's speed it asks for none, and 0.7 there pushed the
		// car past the speed wanted and onto the brake, again and again.
		if (Throttle > 0.f && Brake < 0.05f && V < Cruise - FirmMargin)
		{
			Throttle = std::max(Throttle, FirmThrottle);
		}
		if (bFollowSteering && std::fabs(Steering) >= TurnedLock)
		{
			const float Hold = HoldSpeed(Steering, S);
			if (V > Hold)
			{
				Throttle = 0.f;
				Brake = std::max(Brake, std::min(BrakeMost, (V - Hold) * BrakePerMs));
			}
			else
			{
				// Up to the speed held: the whole throttle from a standstill, little left close to it.
				Throttle = std::min(Throttle, std::min(1.f, (Hold - V) * 0.5f + 0.15f));
			}
		}
	}
}
