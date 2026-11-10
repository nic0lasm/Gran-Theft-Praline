#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPImpactLogic.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPRagdoll.generated.h"

class UAnimMontage;
class UCapsuleComponent;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EGTPImpactSeverity : uint8
{
	None,
	Bump,
	Knock,
	Launch,
	Heavy
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPRagdollEnded, class UGTPRagdollComponent* /*Ragdoll*/, bool /*bGotUp*/);

/**
 * Physics takes over a character's skeletal mesh (a hit, a fall, a bail-out) and gives
 * it back: the body falls, settles, pauses, then gets up (or stays down when badly hurt).
 * Rules live in GTPImpactLogic.h (checked offline by tools/ragdoll/check_impact_logic.py).
 *
 * Works on the player (an ACharacter: the capsule follows the pelvis, movement is
 * disabled meanwhile) and on any actor with a skeletal mesh that has a physics asset
 * (pedestrians). Ticks only while the ragdoll runs. A cap on simultaneous ragdolls is
 * enforced by UGTPRagdollSubsystem (ADR-0006).
 */
UCLASS(Config = Game, ClassGroup = (Physics), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPRagdollComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPRagdollComponent();

	/**
	 * Starts the ragdoll. LaunchVelocity is a velocity change (cm/s) for the pelvis,
	 * HealthAfterHit decides whether the body gets up. False when it cannot start
	 * (no physics asset, already running).
	 */
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	bool BeginRagdoll(FVector LaunchVelocity, EGTPImpactSeverity Severity, float HealthAfterHit, float SpinRadS = 0.f);

	UFUNCTION(BlueprintPure, Category = "Ragdoll")
	bool IsRagdoll() const { return bActive; }

	/** Stops simulating and keeps the pose (cap reached, or stays down). */
	void Freeze();

	float GetAge() const { return State.Elapsed; }

	/** Called when the body got up (true) or stays down / was frozen (false). */
	FGTPRagdollEnded OnRagdollEnded;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** Bone that decides rest and facing. */
	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll")
	FName PelvisBone = TEXT("pelvis");

	/** Pelvis-local axis that points out of the belly (to check face up / face down). Verify on the mannequin. */
	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll")
	FVector PelvisFrontAxis = FVector(0.f, 1.f, 0.f);

	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll")
	FName RagdollProfile = TEXT("Ragdoll");

	/** Optional get-up montages (GASP 5.8 get-ups once retargeted, or a pack from docs/animation/sources.md). */
	UPROPERTY(EditAnywhere, Category = "Ragdoll")
	TSoftObjectPtr<UAnimMontage> GetUpFaceUp;

	UPROPERTY(EditAnywhere, Category = "Ragdoll")
	TSoftObjectPtr<UAnimMontage> GetUpFaceDown;

	/** Used when no montage is set: pause before the character is given back (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll")
	float FallbackGetUpSeconds = 1.f;

private:
	USkeletalMeshComponent* FindMesh() const;
	void Finish(bool bGotUp);
	void RestoreCharacter();

	GTPImpact::FRagdollState State;
	GTPImpact::FRagdollTimings Timings;
	bool bActive = false;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	FTransform MeshRelative = FTransform::Identity;
	TWeakObjectPtr<USceneComponent> MeshParent;
	FName MeshParentSocket;
	ECollisionEnabled::Type MeshCollision = ECollisionEnabled::NoCollision;
	FName MeshProfile;
	float CapsuleHalfHeight = 0.f;
};

/**
 * Keeps the number of simulating ragdolls under the cap of the graphics profile
 * (m1: 3, fidelity: 6, ultra: 10; override with -MaxRagdolls=N) by freezing the
 * farthest, then the oldest. Checked twice a second.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPRagdollSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void Register(UGTPRagdollComponent* Ragdoll);
	void Unregister(UGTPRagdollComponent* Ragdoll);

	int32 GetActiveCount() const { return Active.Num(); }

	/** Hard limit: false when the cap of simulating ragdolls is reached (the body stays animated). */
	bool HasRoom() const;

protected:
	UPROPERTY(Config)
	int32 MaxRagdollsM1 = 3;

	UPROPERTY(Config)
	int32 MaxRagdollsFidelity = 6;

	UPROPERTY(Config)
	int32 MaxRagdollsUltra = 10;

private:
	void Enforce();
	int32 ChooseCap() const;

	TArray<TWeakObjectPtr<UGTPRagdollComponent>> Active;
	FTimerHandle EnforceTimer;
};
