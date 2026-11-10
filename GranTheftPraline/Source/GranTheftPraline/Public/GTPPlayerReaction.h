#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPImpactLogic.h"
#include "GTPPlayerReaction.generated.h"

class ACharacter;
class UAnimMontage;
class UGTPRagdollComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPPlayerHurt, float /*Damage*/, float /*HealthLeft*/);

/**
 * How the player's body answers the world: a car that hits him, a fall from height,
 * a bail-out from a moving car (ADR-0006). Decisions come from GTPImpactLogic.h; this
 * component measures (vehicles around the capsule, landing speed) and acts (ragdoll,
 * stumble, damage event).
 *
 * Add it to the player character (the pawn class or at runtime). It adds the ragdoll
 * component itself when missing. Health here is the player's physical integrity in
 * 0..100; the HUD and wanted-level systems listen to OnHurt.
 */
UCLASS(Config = Game, ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPPlayerReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPPlayerReactionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The player left a moving car (called by the vehicle when he bails out). Velocity in cm/s. */
	UFUNCTION(BlueprintCallable, Category = "Reaction")
	void ReactToBailOut(FVector VehicleVelocity);

	UFUNCTION(BlueprintPure, Category = "Reaction")
	float GetHealth() const { return Health; }

	/** Back to full health (respawn, tests). */
	UFUNCTION(BlueprintCallable, Category = "Reaction")
	void ResetHealth() { Health = 100.f; }

	FGTPPlayerHurt OnHurt;

protected:
	/** Vehicles are looked for this often around the player (s). */
	UPROPERTY(Config)
	float CarCheckInterval = 0.05f;

	/** Vehicles are looked for within this radius of the capsule (cm). */
	UPROPERTY(Config)
	float CarCheckRadius = 450.f;

	/** The same car cannot hurt him again for this long (s). */
	UPROPERTY(Config)
	float CarHitCooldown = 1.5f;

	/** Stumble: walking speed multiplier and its duration. */
	UPROPERTY(Config)
	float StumbleSpeedScale = 0.45f;

	UPROPERTY(Config)
	float StumbleSeconds = 0.7f;

	/** Optional stumble montage (hit reaction). */
	UPROPERTY(EditAnywhere, Category = "Reaction")
	TSoftObjectPtr<UAnimMontage> StumbleMontage;

private:
	void CheckCars();
	UFUNCTION()
	void OnLandedDynamic(const FHitResult& Hit);
	void ApplyHit(GTPImpact::ESeverity Severity, const FVector& Launch, float Damage, float Spin);
	void Stumble();
	void EndStumble();
	UGTPRagdollComponent* GetRagdoll();

	GTPImpact::FSettings Settings;
	float Health = 100.f;
	float DefaultWalkSpeed = 0.f;
	bool bStumbling = false;
	/** Vertical speed seen by the last car check (cm/s), the landing happens after it is zeroed. */
	float LastFallSpeed = 0.f;
	FTimerHandle CarTimer;
	FTimerHandle StumbleTimer;
	TMap<TWeakObjectPtr<AActor>, double> LastHit;

	UPROPERTY(Transient)
	TObjectPtr<UGTPRagdollComponent> Ragdoll;
};
