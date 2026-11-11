#pragma once

// Car signal rules (brake lights, reverse light, turn indicators), written without
// engine types so they are checked outside Unreal (tools/environment/car_signals_test.cpp).
// UGTPEnvironmentSubsystem feeds them the driving inputs of each lit car.
//
// - Brake: brake input above a threshold, or the handbrake, while the car is not
//   standing still in reverse.
// - Reverse: a negative gear.
// - Indicators: the drivers (player or AI) have no indicator stalk, so a turn is
//   inferred from the steering at town speed and held a little after the wheel
//   straightens, like a self-cancelling stalk. Blinks at 1.5 Hz.

#include <cmath>

struct FGTPCarSignalInput
{
	float Brake = 0.f;      // 0..1
	bool bHandbrake = false;
	float Steering = 0.f;   // -1 left .. 1 right
	int Gear = 0;           // negative: reverse
	float SpeedCmS = 0.f;   // forward speed, absolute
	double TimeSeconds = 0.0;
};

struct FGTPCarSignalOutput
{
	bool bBrake = false;
	bool bReverse = false;
	int IndicatorSide = 0;  // -1 left, 1 right, 0 none
	bool bIndicatorLit = false;
};

struct FGTPCarSignalState
{
	int Side = 0;
	double HoldUntil = -1.0;
};

namespace GTPCarSignals
{
	constexpr float BrakeThreshold = 0.1f;
	constexpr float SteerThreshold = 0.3f;
	constexpr float MaxIndicatorSpeedCmS = 1400.f;  // about 50 km/h
	constexpr float MinIndicatorSpeedCmS = 50.f;    // not while parked
	constexpr double HoldSeconds = 1.2;
	constexpr double BlinkPeriod = 1.0 / 1.5;

	inline FGTPCarSignalOutput Evaluate(const FGTPCarSignalInput& In, FGTPCarSignalState& State)
	{
		FGTPCarSignalOutput Out;
		Out.bReverse = In.Gear < 0;
		Out.bBrake = In.Brake > BrakeThreshold || In.bHandbrake;

		const bool bTurning = std::fabs(In.Steering) > SteerThreshold && In.SpeedCmS > MinIndicatorSpeedCmS
			&& In.SpeedCmS < MaxIndicatorSpeedCmS && !Out.bReverse;
		if (bTurning)
		{
			State.Side = In.Steering < 0.f ? -1 : 1;
			State.HoldUntil = In.TimeSeconds + HoldSeconds;
		}
		else if (In.TimeSeconds > State.HoldUntil)
		{
			State.Side = 0;
		}
		Out.IndicatorSide = State.Side;
		if (State.Side != 0)
		{
			const double Phase = std::fmod(In.TimeSeconds, BlinkPeriod);
			Out.bIndicatorLit = Phase < 0.5 * BlinkPeriod;
		}
		return Out;
	}
}
