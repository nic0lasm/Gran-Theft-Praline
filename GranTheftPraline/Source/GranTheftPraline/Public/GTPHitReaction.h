#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPHitLogic.h"
#include "GTPRagdoll.h"
#include "GTPHitReaction.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EGTPHitReaction : uint8
{
	None,
	Flinch,
	Stagger,
	Knockdown,
	Death
};

/** What the combat system tells a body about a hit it already resolved. */
USTRUCT(BlueprintType)
struct FGTPHitInfo
{
	GENERATED_BODY()

	/** Health points lost (0..100 scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage = 0.f;

	/** Health left after the hit; 0 or less means the body dies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HealthAfter = 100.f;

	/** World direction the hit travels, from the attacker toward the body (need not be normalised). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Direction = FVector::ForwardVector;

	/** Bone that was hit (decides the zone), NAME_None = torso. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Bone = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bMelee = false;

	/** Forces a knockdown whatever the damage (the heavy third punch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHeavy = false;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPHitReactionEvent, EGTPHitReaction /*Reaction*/, const FGTPHitInfo& /*Hit*/, AActor* /*Body*/);

/**
 * How a body reacts to a hit (version 0.7, called by the combat system; ADR-0006):
 * a flinch or stagger plays the keyed clip for the side it came from (AM_HitLight_*,
 * AM_Stagger_*, see tools/animation), a knockdown or a death hands the body to the
 * ragdoll component with a launch along the hit, the dead stay down. Decisions come from
 * GTPHitLogic.h; this component only acts. Works on the player (an ACharacter, slowed
 * during a stagger), on pedestrians and later on enemies. -HitReactions=0 turns it off.
 */
UCLASS(Config = Game, ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static bool IsEnabled();

	/** Decides and performs the reaction; returns what was done (None when nothing). */
	UFUNCTION(BlueprintCallable, Category = "Reaction")
	EGTPHitReaction ReactToHit(const FGTPHitInfo& Hit);

	UFUNCTION(BlueprintPure, Category = "Reaction")
	bool IsDead() const { return State.bDead; }

	/** Combat, police and pedestrians listen here (a body flinched, fell or died). */
	FGTPHitReactionEvent OnReaction;

	/** Clips by name (AM_HitLight_Front...): filled by the character setup. */
	UPROPERTY(EditAnywhere, Category = "Reaction")
	TMap<FName, TSoftObjectPtr<UAnimMontage>> Clips;

protected:
	/** Walking speed multiplier during a stagger (characters) and its length (s). */
	UPROPERTY(Config)
	float StaggerSpeedScale = 0.5f;

	UPROPERTY(Config)
	float StaggerSeconds = 0.6f;

private:
	void PlayClip(const char* Name);
	void EndStagger();
	UGTPRagdollComponent* GetRagdoll();

	GTPHit::FState State;
	GTPHit::FSettings Settings;
	FTimerHandle StaggerTimer;
	float SavedWalkSpeed = 0.f;
	bool bStaggering = false;

	UPROPERTY(Transient)
	TObjectPtr<UGTPRagdollComponent> Ragdoll;
};
