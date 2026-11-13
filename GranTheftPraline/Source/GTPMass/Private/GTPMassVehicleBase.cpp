#include "GTPMassVehicleBase.h"

#include "MassAgentComponent.h"

AGTPMassVehicleBase::AGTPMassVehicleBase()
{
	MassAgent = CreateDefaultSubobject<UMassAgentComponent>(TEXT("MassAgent"));
}

void AGTPMassVehicleBase::ApplyWheelMotionBlurNative(const TArray<UMaterialInstanceDynamic*>& MotionBlurMIDs, UActorComponent* MassTrafficVehicleComponent,
	int32 WheelMotionBlurNumSpokes, float WheelMotionBlurStartBlurSpeed, float WheelMotionBlurMin, float WheelMotionBlurMax)
{
	// Wheel blur not done yet: the wheels stay sharp, nothing else depends on it.
}
