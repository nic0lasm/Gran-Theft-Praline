#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MassActorPoolableInterface.h"
#include "GTPMassVehicleBase.generated.h"

class UMassAgentComponent;
class UMaterialInstanceDynamic;

/**
 * Stand-in for the base class of City Sample's car Blueprints (BP_vehicle), which lives in the
 * City Sample game module we do not have. Config/DefaultEngine.ini redirects
 * /Script/CitySample.CitySampleMassVehicleBase here, so those Blueprints, copied at their City
 * Sample paths for MassTraffic (docs/gameplay/traffic-mass.md), load in our project.
 *
 * Written from the interface only (what the Blueprints expect), not from Epic's code: an actor
 * with a Mass agent component, poolable by Mass (collision off while pooled), and the
 * Blueprint-callable wheel motion blur function with a compatible signature.
 */
UCLASS()
class GTPMASS_API AGTPMassVehicleBase : public AActor, public IMassActorPoolableInterface
{
	GENERATED_BODY()

public:
	AGTPMassVehicleBase();

	// IMassActorPoolableInterface
	virtual bool CanBePooled_Implementation() override { return true; }
	virtual void PrepareForPooling_Implementation() override { SetActorEnableCollision(false); }
	virtual void PrepareForGame_Implementation() override { SetActorEnableCollision(true); }

	/**
	 * Called by the car Blueprints every frame to blur the wheels at speed. The component is the
	 * car's MassTraffic vehicle component (typed as a plain component here so this module builds
	 * without the MassTraffic plugin). Ours leaves the wheels sharp for now: cosmetic.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle")
	void ApplyWheelMotionBlurNative(const TArray<UMaterialInstanceDynamic*>& MotionBlurMIDs, UActorComponent* MassTrafficVehicleComponent,
		int32 WheelMotionBlurNumSpokes, float WheelMotionBlurStartBlurSpeed, float WheelMotionBlurMin, float WheelMotionBlurMax);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mass")
	TObjectPtr<UMassAgentComponent> MassAgent;
};
