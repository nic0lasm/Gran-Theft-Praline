#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPIncidentLogic.h"
#include "GTPTrafficIncident.generated.h"

class ACharacter;
class AGTPVehicle;
class AGTPTrafficNetwork;

/**
 * Put on a traffic driver after a crash between two traffic cars
 * (AGTPTrafficNetwork::NotifyTrafficCrash): gets out, walks up to the other driver,
 * argues with a shove now and then, walks back and drives on (rules in
 * GTPIncidentLogic.h). Runs if a gun is aimed, a shot is fired near, they are hurt, or
 * their car is stolen. Removes itself at the end; a driver who ran is taken away once far
 * and out of sight.
 */
UCLASS(Config = Game, ClassGroup = (Vehicle))
class GRANTHEFTPRALINE_API UGTPTrafficArgueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPTrafficArgueComponent();

	/** Adds the component to Driver (still seated in Car; it gets out here). Other may be null. */
	static UGTPTrafficArgueComponent* Start(ACharacter* Driver, AGTPVehicle* Car, ACharacter* Other, AGTPTrafficNetwork* Network);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	GTPIncident::EPhase GetPhase() const { return Arguer.Phase; }
	bool IsFleeing() const { return Arguer.Phase == GTPIncident::EPhase::Flee; }

protected:
	UPROPERTY(Config, EditAnywhere, Category = "Incident")
	float ShoveChance = 0.4f;

	/** Push of a shove on the other driver (cm/s). */
	UPROPERTY(Config, EditAnywhere, Category = "Incident")
	float ShoveSpeed = 380.f;

	UPROPERTY(Config, EditAnywhere, Category = "Incident")
	float WalkSpeed = 170.f;

	UPROPERTY(Config, EditAnywhere, Category = "Incident")
	float RunSpeed = 480.f;

	UPROPERTY(Config, EditAnywhere, Category = "Incident")
	float FleeDistance = 3000.f;

	/** A driver who ran is removed beyond this distance from the player, out of sight (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Incident")
	float RemoveDistance = 9000.f;

private:
	void BeginFlee(const FVector& From);
	void Finish();
	bool IsThreatened(const ACharacter* Body);

	TWeakObjectPtr<AGTPVehicle> Car;
	TWeakObjectPtr<ACharacter> Other;
	TWeakObjectPtr<AGTPTrafficNetwork> Network;
	GTPIncident::FSettings Settings;
	GTPIncident::FArguer Arguer;
	float LastHealth = -1.f;
	float MoveOrderAge = 1000.f;
	float FleeTime = 0.f;
	int32 SeenShotSerial = -1;
};
