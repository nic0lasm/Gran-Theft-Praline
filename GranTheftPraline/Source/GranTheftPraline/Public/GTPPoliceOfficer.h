#pragma once

#include "CoreMinimal.h"
#include "GTPLiteDriver.h"
#include "GTPPoliceTactics.h"
#include "GTPPoliceOfficer.generated.h"

class UGTPWeaponComponent;

/**
 * A police officer on foot (ADR-0004, tactics in GTPPoliceTactics.h). Put on the ground by the
 * AGTPPoliceDirector when the player is on foot near a police car (or the car is stuck).
 * It runs at the player while the level is high enough and the police see them, goes to the last
 * known position and then sweeps a widening circle around it while searching, and shoots at three
 * stars or more (two once the player has fired or threatened), through the weapon of the combat
 * layer (UGTPWeaponComponent::FireAt). No navigation mesh: it steers straight and slides along
 * walls; the real path comes with the alleys (traboules) scenario.
 *
 * A pawn of its own with a light body (the AGTPLiteDriver mesh logic: the player's mesh unless
 * -LiteDriverMesh= says otherwise), one actor tick, no camera.
 */
UCLASS(NotBlueprintable, Config = Game)
class GRANTHEFTPRALINE_API AGTPPoliceOfficer : public AGTPLiteDriver
{
	GENERATED_BODY()

public:
	AGTPPoliceOfficer(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	/** Slot among the officers of the director, for the search pattern. */
	void SetSlot(int32 InIndex, int32 InCount)
	{
		SlotIndex = InIndex;
		SlotCount = FMath::Max(1, InCount);
	}

	/** Looping run animation; empty = the mesh's reference pose (slides). -PoliceRunAnim= overrides. */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	FSoftObjectPath RunAnimation;

protected:
	virtual void BeginPlay() override;

private:
	bool HasLineOfSight(const FVector& From, const AActor& Target) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UGTPWeaponComponent> Weapon;

	GTPPoliceTactics::FSettings Tactics;
	int32 SlotIndex = 0;
	int32 SlotCount = 1;
	float FireCooldown = 0.f;
	float SearchSeconds = 0.f;
	float NextLosTime = 0.f;
	bool bLosCached = false;
};
