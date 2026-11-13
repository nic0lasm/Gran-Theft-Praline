#include "GTPVehicleWheels.h"

namespace
{
	void ApplyCommonWheel(UChaosVehicleWheel& Wheel)
	{
		// 195/55 R16: about 31 cm radius, 19.5 cm wide.
		Wheel.WheelRadius = 31.f;
		Wheel.WheelWidth = 19.5f;
		Wheel.bAffectedByBrake = true;
		Wheel.MaxBrakeTorque = 1800.f;
		// Suspension of Epic's City Sample city car (vehicle02, 1 200 kg, the closest to a
		// Clio): soft springs on a high preload and long travel, so the body rolls, dives and
		// squats visibly instead of riding stiff (13/11; brakes stay ours, stronger).
		Wheel.SuspensionMaxRaise = 10.f;
		Wheel.SuspensionMaxDrop = 17.f;
		Wheel.SuspensionDampingRatio = 0.5f;
		Wheel.SpringRate = 100.f;
		Wheel.SpringPreload = 300.f;
		Wheel.SlipThreshold = 20.f;
		Wheel.SkidThreshold = 20.f;
	}
}

UGTPWheelFront::UGTPWheelFront()
{
	ApplyCommonWheel(*this);
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	bAffectedByEngine = true;
	bAffectedByHandbrake = false;
	// City Sample's cars turn 42 to 47 degrees: tighter U-turns in Lyon's narrow streets.
	MaxSteerAngle = 45.f;
	FrictionForceMultiplier = 3.f;
	CorneringStiffness = 1000.f;
}

UGTPWheelRear::UGTPWheelRear()
{
	ApplyCommonWheel(*this);
	AxleType = EAxleType::Rear;
	bAffectedBySteering = false;
	bAffectedByEngine = false;
	bAffectedByHandbrake = true;
	MaxHandBrakeTorque = 3500.f; // City Sample vehicle02
	// More grip at the rear than at the front: the car understeers gently
	// when pushed instead of spinning (the handbrake still breaks it loose).
	FrictionForceMultiplier = 3.3f;
	CorneringStiffness = 1100.f;
}
