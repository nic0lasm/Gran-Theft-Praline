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
		Wheel.SuspensionMaxRaise = 8.f;
		Wheel.SuspensionMaxDrop = 10.f;
		Wheel.SuspensionDampingRatio = 0.5f;
		Wheel.SpringRate = 250.f;
		Wheel.SpringPreload = 50.f;
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
	MaxSteerAngle = 38.f;
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
	MaxHandBrakeTorque = 3000.f;
	// More grip at the rear than at the front: the car understeers gently
	// when pushed instead of spinning (the handbrake still breaks it loose).
	FrictionForceMultiplier = 3.3f;
	CorneringStiffness = 1100.f;
}
