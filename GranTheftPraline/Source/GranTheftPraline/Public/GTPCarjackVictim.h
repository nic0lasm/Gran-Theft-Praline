#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPCarjackLogic.h"
#include "GTPCarjackVictim.generated.h"

class ACharacter;
class AGTPVehicle;

/**
 * Put on a driver pulled out of their car by the player (AGTPVehicle::BeginEnter): the
 * driver flees, comes back at the thief and punches or bangs on the door, or cowers
 * then flees (rules in GTPCarjackLogic.h). Removes itself once the driver runs away.
 */
UCLASS(Config = Game, ClassGroup = (Vehicle))
class GRANTHEFTPRALINE_API UGTPCarjackVictimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPCarjackVictimComponent();

	/** Adds the component to Victim and starts it. -CarjackMood=flee|resist|cower forces the mood. */
	static UGTPCarjackVictimComponent* Start(ACharacter* Victim, AGTPVehicle* Car, ACharacter* Thief);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	GTPCarjack::EMood GetMood() const { return Victim.Mood; }

protected:
	/** Shares of the moods (the rest flees). */
	UPROPERTY(Config, EditAnywhere, Category = "Carjack")
	float ResistShare = 0.3f;

	UPROPERTY(Config, EditAnywhere, Category = "Carjack")
	float CowerShare = 0.15f;

	/** Damage of one punch on the thief, and of one bang on the stolen car's door. */
	UPROPERTY(Config, EditAnywhere, Category = "Carjack")
	float PunchDamage = 10.f;

	UPROPERTY(Config, EditAnywhere, Category = "Carjack")
	float DoorBangDamage = 12.f;

	/** How far the victim runs (cm), and at what speed (cm/s). */
	UPROPERTY(Config, EditAnywhere, Category = "Carjack")
	float FleeDistance = 2500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Carjack")
	float RunSpeed = 480.f;

private:
	void BeginFlee();
	FVector TargetLocation() const;

	TWeakObjectPtr<AGTPVehicle> Car;
	TWeakObjectPtr<ACharacter> Thief;
	GTPCarjack::FSettings Settings;
	GTPCarjack::FVictim Victim;
	float LastHealth = -1.f;
	float MoveOrderAge = 1000.f;
	float FleeTime = 0.f;
};
