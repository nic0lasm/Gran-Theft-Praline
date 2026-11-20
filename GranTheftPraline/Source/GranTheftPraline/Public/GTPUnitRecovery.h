#pragma once

// What a police car does when its driving goes wrong, written without engine types so it can be
// tested outside Unreal (tools/vehicles/wanted_sim/unit_recovery_test.cpp). Units: seconds, metres, m/s, km/h.
//
// Gate of candidate 48 (2026-11-19): the chasing units averaged 13 km/h and moved 29 % of their time.
// Their lines showed two things, neither a missing throttle (it read 1.00 in every stopped sample):
//  - a car held at 0 km/h for 24 s on its four wheels, first gear, 2086 N.m on each rear wheel and no
//    wheel turning: against something, pushing. A unit never backed off (only the demo's car knew how);
//  - cars with their two inner wheels in the air seconds after pulling away, then with no wheel on the
//    ground: the driving asked for full lock with the throttle held down, on tyres that grip harder
//    (friction 0.7 x 3) than the car needs to roll over (centre of mass 83 cm up).
// So: the pedals follow the steering (Tame), a car held against something backs off (Step), and a car
// that stays off its wheels or cannot get away is given up, counted and replaced, never moved back onto a lane.
//
// Gate of candidate 51 (2026-11-20): two units backed off three times without moving (one lying against the car
// it had been made on, one on its four wheels with 751 N.m forwards then 1287 N.m backwards on wheels that did
// not turn) and stood 12 s at 0 km/h before they were given up. A car that reverse does not move either is
// stuck: given up after two such back-offs in a row.

#include <algorithm>
#include <cmath>

struct FGTPUnitRecoverySettings
{
	/** Slower than this the car is not moving. */
	float StillKmh = 2.f;
	/** A throttle above this asks the car to move. */
	float HeldThrottle = 0.3f;
	/** Not moving with the throttle held for this long: held against something. */
	float PinnedSeconds = 1.5f;
	/** Backing off: this long, the brake pedal (as reverse) this far down, wheels turned the other way. */
	float BackSeconds = 1.8f;
	float BackPedal = 0.7f;
	/** Backed off this many times within SamePlaceMetres without getting away: the car is given up. */
	int MaxBackOffs = 3;
	float SamePlaceMetres = 12.f;
	/** A back-off that moved the car less than this did nothing; this many of them in a row: the car is stuck, given up. */
	float BackMovedMetres = 0.5f;
	int MaxDeadBackOffs = 2;
	/** No wheel on the ground, or lying on its side, and slower than OffWheelsKmh for this long: given up.
	 *  Longer than the vehicle's own rule, which puts a car lying on its side or roof back on its wheels after
	 *  2.5 s and lets it fall 1.2 m (AGTPVehicle, AI-driven cars too): a car that rule has righted drives on,
	 *  only a car it could not help (four wheels in the air but not lying, or lying again at once) is given up. */
	float OffWheelsSeconds = 5.f;
	float OffWheelsKmh = 5.f;
	/** Up vector Z under this: on its side or roof (60 degrees and more). */
	float OnSideUp = 0.5f;
	/** Pulling away: the rear tyres take no more than this, and more lifts the nose (700 N.m, about 0.95 g). */
	float LaunchThrottle = 0.6f;
	float LaunchKmh = 20.f;
	/** The car itself: 45 degrees at full lock (UGTPWheelFront), 2.97 m between the axles. */
	float FullLockRadians = 0.785f;
	float WheelBase = 2.97f;
	/** The steering never asks for more than this share of the lateral acceleration the speed is planned for. */
	float SteerMargin = 1.25f;
};

struct FGTPUnitRecovery
{
	enum class EState
	{
		Driving,
		BackingOff,
		/** Off its wheels, or held in the same place after MaxBackOffs: no longer a chasing car. */
		GivenUp
	};
	enum class EWhy
	{
		None,
		Pinned,
		OffWheels,
		/** Moves neither forwards nor backwards. */
		Stuck
	};

	EState State = EState::Driving;
	EWhy Why = EWhy::None;
	float PinnedTime = 0.f;
	float OffWheelsTime = 0.f;
	float BackLeft = 0.f;
	/** Steering while backing off (-1 left, +1 right). */
	float BackSteer = 0.f;
	/** Back-offs in the same place, and in all. */
	int BackOffsHere = 0;
	int BackOffs = 0;
	bool bHasPlace = false;
	float PlaceX = 0.f;
	float PlaceY = 0.f;
	/** Where the back-off under way began, and the back-offs in a row that did not move the car. */
	float BackFromX = 0.f;
	float BackFromY = 0.f;
	int DeadBackOffs = 0;

	bool IsGivenUp() const { return State == EState::GivenUp; }

	/**
	 * One frame. ThrottleAsked and SteerAsked are the last pedals the driving gave (not the back-off's own).
	 * WheelsOnGround: wheels in contact; UpZ: Z of the car's up vector; X, Y: where the car is.
	 */
	EState Step(float DeltaSeconds, float SpeedKmh, float ThrottleAsked, float SteerAsked, int WheelsOnGround, float UpZ, float X, float Y,
		const FGTPUnitRecoverySettings& S)
	{
		if (State == EState::GivenUp)
		{
			return State;
		}
		const float Kmh = std::fabs(SpeedKmh);
		const bool bOffWheels = (WheelsOnGround <= 0 || UpZ < S.OnSideUp) && Kmh < S.OffWheelsKmh;
		OffWheelsTime = bOffWheels ? OffWheelsTime + DeltaSeconds : 0.f;
		if (OffWheelsTime >= S.OffWheelsSeconds)
		{
			State = EState::GivenUp;
			Why = EWhy::OffWheels;
			return State;
		}
		if (State == EState::BackingOff)
		{
			BackLeft -= DeltaSeconds;
			if (BackLeft <= 0.f)
			{
				State = EState::Driving;
				PinnedTime = 0.f;
				DeadBackOffs = std::hypot(X - BackFromX, Y - BackFromY) < S.BackMovedMetres ? DeadBackOffs + 1 : 0;
				if (DeadBackOffs >= S.MaxDeadBackOffs)
				{
					State = EState::GivenUp;
					Why = EWhy::Stuck;
				}
			}
			return State;
		}
		// Away from the place it was held at: that place is forgotten.
		if (bHasPlace && std::hypot(X - PlaceX, Y - PlaceY) > S.SamePlaceMetres)
		{
			bHasPlace = false;
			BackOffsHere = 0;
		}
		const bool bPinned = Kmh < S.StillKmh && ThrottleAsked >= S.HeldThrottle && WheelsOnGround > 0;
		PinnedTime = bPinned ? PinnedTime + DeltaSeconds : 0.f;
		if (PinnedTime < S.PinnedSeconds)
		{
			return State;
		}
		PinnedTime = 0.f;
		if (!bHasPlace)
		{
			bHasPlace = true;
			PlaceX = X;
			PlaceY = Y;
		}
		if (BackOffsHere >= S.MaxBackOffs)
		{
			State = EState::GivenUp;
			Why = EWhy::Pinned;
			return State;
		}
		++BackOffsHere;
		++BackOffs;
		State = EState::BackingOff;
		BackLeft = S.BackSeconds;
		BackFromX = X;
		BackFromY = Y;
		// Backwards with the wheels turned away from where the car wants to go: the nose swings towards it.
		// Going straight: one side, then the other.
		const float Side = std::fabs(SteerAsked) > 0.2f ? (SteerAsked > 0.f ? 1.f : -1.f) : (BackOffsHere % 2 == 1 ? 1.f : -1.f);
		BackSteer = -Side;
		return State;
	}
};

namespace GTPUnitRecovery
{
	inline float Clamp(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

	/** The speed (m/s) the car can hold with this steering (-1..1) at LateralAccel m/s2; huge when going straight. */
	inline float SteerSpeed(float Steering, float LateralAccel, const FGTPUnitRecoverySettings& S)
	{
		const float Angle = std::fabs(Steering) * S.FullLockRadians;
		if (Angle < 0.01f)
		{
			return 1000.f;
		}
		return std::sqrt(LateralAccel * S.WheelBase / std::tan(Angle));
	}

	/** The most steering (0..1) the car can take at Speed m/s for LateralAccel m/s2. */
	inline float SteerLimit(float Speed, float LateralAccel, const FGTPUnitRecoverySettings& S)
	{
		const float V = std::fabs(Speed);
		if (V < 1.f)
		{
			return 1.f;
		}
		return Clamp(std::atan(S.WheelBase * LateralAccel / (V * V)) / S.FullLockRadians, 0.f, 1.f);
	}

	/**
	 * The pedals follow the steering. Faster than the steering asked can be held: no throttle, some brake,
	 * until the car can turn that much. The steering itself never asks for a roll-over at the speed the car
	 * has. Pulling away: no more throttle than the tyres take. Speed in m/s; Throttle, Brake 0..1; Steering -1..1.
	 */
	inline void Tame(float& Throttle, float& Brake, float& Steering, float Speed, float LateralAccel, const FGTPUnitRecoverySettings& S)
	{
		const float V = std::fabs(Speed);
		const float Hold = SteerSpeed(Steering, LateralAccel, S);
		if (V > Hold)
		{
			Throttle = 0.f;
			Brake = std::max(Brake, Clamp((V - Hold) * 0.25f, 0.f, 0.8f));
		}
		else
		{
			Throttle = std::min(Throttle, Clamp((Hold - V) * 0.5f + 0.15f, 0.f, 1.f));
		}
		const float Lock = SteerLimit(V, LateralAccel * S.SteerMargin, S);
		Steering = Clamp(Steering, -Lock, Lock);
		if (V * 3.6f < S.LaunchKmh)
		{
			Throttle = std::min(Throttle, S.LaunchThrottle);
		}
	}

	inline const char* WhyName(FGTPUnitRecovery::EWhy Why)
	{
		switch (Why)
		{
		case FGTPUnitRecovery::EWhy::Pinned: return "held in the same place after backing off";
		case FGTPUnitRecovery::EWhy::OffWheels: return "off its wheels";
		case FGTPUnitRecovery::EWhy::Stuck: return "moves neither forwards nor backwards";
		default: return "driving";
		}
	}
}
