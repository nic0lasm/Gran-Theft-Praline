#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPImpactLogic.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPRagdoll.generated.h"

GRANTHEFTPRALINE_API DECLARE_LOG_CATEGORY_EXTERN(LogGTPRagdoll, Log, All);

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

	/** Lite mode: physics solver iterations per body (default engine value is higher). */
	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll|Lite")
	int32 LiteSolverIterations = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll|Lite")
	float LiteDamping = 1.5f;

	/** Lite mode: these bones and everything below them (fingers) are not simulated. */
	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll|Lite")
	TArray<FName> LiteKinematicBones = {TEXT("hand_l"), TEXT("hand_r")};

	/** Lite mode: no ragdoll for a body farther than this from the viewer (cm; it stays animated). */
	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll|Lite")
	float MaxStartDistance = 3500.f;

	/** Lite mode: a frozen non-player body is hidden after this long (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Ragdoll|Lite")
	float HideFrozenAfter = 4.f;

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
	FTimerHandle HideTimer;

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

	/** -Ragdoll=0 on the command line turns every physical reaction off. */
	static bool IsEnabled();

	/**
	 * -RagdollLite=1: cheaper ragdolls (ADR-0006, cost section): fewer solver iterations,
	 * more damping so bodies sleep sooner, hands kept kinematic, shorter life cycle,
	 * a higher cap, no ragdoll beyond MaxStartDistance, frozen bodies hidden after a few seconds.
	 */
	static bool IsLite();

	/** Hard limit: false when the cap of simulating ragdolls is reached (the body stays animated). */
	bool HasRoom() const;

	/**
	 * For ragdolls that are not a UGTPRagdollComponent (the pedestrians' own): takes a place
	 * under the same cap. False when the cap is reached or ragdolls are off (-Ragdoll=0):
	 * the body then stays animated. Release it when the body is given back.
	 */
	bool TryClaimExternal(AActor* Body);
	void ReleaseExternal(AActor* Body);

protected:
	UPROPERTY(Config)
	int32 MaxRagdollsM1 = 3;

	UPROPERTY(Config)
	int32 MaxRagdollsFidelity = 6;

	UPROPERTY(Config)
	int32 MaxRagdollsUltra = 10;

	/** Cap in lite mode (to confirm by measurement before it becomes the default). */
	UPROPERTY(Config)
	int32 MaxRagdollsLiteM1 = 6;

	UPROPERTY(Config)
	int32 MaxRagdollsLiteOther = 12;

private:
	void Enforce();
	int32 ChooseCap() const;

	TArray<TWeakObjectPtr<UGTPRagdollComponent>> Active;
	TArray<TWeakObjectPtr<AActor>> External;
	FTimerHandle EnforceTimer;
};
