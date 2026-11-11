#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPKitchenModel.h"
#include "GTPKitchen.generated.h"

class AGTPStaffCharacter;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGTPStaffTaskSignature, AGTPStaffCharacter*, Staff, uint8, Task);

/**
 * One member of a restaurant crew (docs/gameplay/reference-interior.md section 4). Walks to the
 * slot of the station its current task needs, faces it and plays the task's montage when set
 * (TaskMontages, indexed by GTPKitchen::ETask). Persistent in the interior, not part of the
 * pedestrian pool. Threat reactions come from the ragdoll thread's UGTPThreatResponseComponent
 * when it is added to this actor (step 7 stops its tasks while threatened).
 */
UCLASS()
class GRANTHEFTPRALINE_API AGTPStaffCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGTPStaffCharacter();
	virtual void Tick(float DeltaSeconds) override;

	/** Starts a task: where to stand and what to play. */
	void BeginTask(uint8 Task, const FTransform& Slot);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kitchen")
	TMap<uint8, TObjectPtr<UAnimMontage>> TaskMontages;

	/** Fired when the staff reaches its slot and starts the task (animation, object promotion). */
	UPROPERTY(BlueprintAssignable, Category = "Kitchen")
	FGTPStaffTaskSignature OnTaskStarted;

	uint8 GetTask() const { return Task; }

private:
	FTransform Slot;
	uint8 Task = 0;
	bool bAtSlot = true;
};

/**
 * Runs the service of a restaurant interior with GTPKitchenModel.h at a fixed 30 Hz step and
 * hands each task to a staff character (step 4). Station slots are the TargetPoints tagged
 * GTP_Station=<station>_<n> by tools/interiors/unreal/build_interior_level.py.
 *
 * Off by default. Command line:
 *   -InteriorStaff=N          crew size 1..10 (0 or absent: no staff)
 *   -InteriorCustomersPerHour=N   service rate (default 120)
 * Log lines "KitchenService: ..." every minute: served, average wait, waste.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPKitchenSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Starts the service once the interior level is visible (called by the interior streaming). */
	void StartService(ULevel* InteriorLevel);
	void StopService();

	const GTPKitchen::FKitchen& GetKitchen() const { return Kitchen; }

private:
	bool FindSlot(GTPKitchen::ETask Task, int32 StaffIndex, int32 Target, FTransform& Out) const;
	void Dispatch(const GTPKitchen::FTaskEvent& Event);

	GTPKitchen::FKitchen Kitchen;
	TMap<FString, FTransform> Slots;   // "fryer_0" -> transform
	TArray<TWeakObjectPtr<AGTPStaffCharacter>> Crew;
	size_t EventsSent = 0;
	float Accumulator = 0.f;
	float SinceLog = 0.f;
	bool bRunning = false;
};
