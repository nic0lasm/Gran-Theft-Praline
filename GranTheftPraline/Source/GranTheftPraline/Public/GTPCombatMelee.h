#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPCombatMeleeLogic.h"
#include "GTPCombatMelee.generated.h"

class UAnimMontage;
class UGTPHealthComponent;

/**
 * Bare-hands fighting for one body (player first; police and shop staff can use it):
 * punches in a three-blow combo, dodge, guard, takedown (ADR-0011, rules in
 * GTPCombatMeleeLogic.h). It ticks only while something happens. The landing blow
 * sweeps a short arc in front; the hit goes through UGTPCombatSubsystem::ApplyHit like
 * a bullet, so police, reactions and sounds follow. Stamina is the body's health
 * component's.
 */
UCLASS(Config = Game, ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPCombatMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPCombatMeleeComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Punch (or a takedown when a target qualifies). */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void Attack();

	UFUNCTION(BlueprintCallable, Category = "Melee")
	void SetGuard(bool bOn);

	/** Dodge along WorldDirection (zero = backwards). */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void Dodge(FVector WorldDirection);

	/**
	 * A blow arrives at this body (called by another fighter before ApplyHit).
	 * Returns the result: damage after guard, dodge, guard broken.
	 */
	GTPMelee::FReceiveResult Receive(float Damage, bool bHeavy, const FVector& FromLocation);

	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsStunned() const { return Fighter.GetPhase() == GTPMelee::EPhase::Stunned; }
	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsGuarding() const { return Fighter.GetPhase() == GTPMelee::EPhase::Guard; }
	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsBusy() const { return Fighter.IsBusy(); }

	/** Clips by name: AM_Punch_1..3, AM_Dodge_F/B/L/R, AM_Guard, AM_Block, AM_Takedown, AM_Takedown_Victim. */
	UPROPERTY(EditAnywhere, Category = "Melee")
	TMap<FName, TSoftObjectPtr<UAnimMontage>> Clips;

protected:
	/** Dodge push (cm/s, horizontal). */
	UPROPERTY(Config)
	float DodgeSpeedCmS = 650.f;

	/** Half-angle of the arc a blow sweeps in front (degrees). */
	UPROPERTY(Config)
	float StrikeHalfAngleDeg = 55.f;

	/** A takedown leaves this much health: the body is knocked out, not killed. */
	UPROPERTY(Config)
	float TakedownHealthLeft = 1.f;

private:
	AActor* FindStrikeTarget(float RangeCm) const;
	bool TryTakedown();
	void LandBlow(const GTPMelee::FStep& Step);
	UGTPHealthComponent* GetHealth() const;
	float GetStamina() const;
	void PlayClip(FName Name);
	void Wake() { SetComponentTickEnabled(true); }

	GTPMelee::FFighter Fighter;
};
