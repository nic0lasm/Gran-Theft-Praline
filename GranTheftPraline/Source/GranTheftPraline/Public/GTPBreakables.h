#pragma once

#include "CoreMinimal.h"
#include "GTPBreakableLogic.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPBreakables.generated.h"

class AActor;
class AStaticMeshActor;
class UHierarchicalInstancedStaticMeshComponent;

GRANTHEFTPRALINE_API DECLARE_LOG_CATEGORY_EXTERN(LogGTPBreakables, Log, All);

/**
 * Street furniture that tips over or breaks when a car or the player hits it (0.4).
 *
 * The street props are instanced (one HISM actor per asset), so there is no actor per
 * object: this subsystem finds the instanced components tagged by the street's
 * placement script and, twenty times a second, tests vehicles and the player against
 * the instances near them. A hit instance is removed and replaced by a physics actor
 * thrown by the hit; GTPBreakableLogic.h decides (checked offline).
 *
 * Component tags (set by tools/city place_props.py from the placement tables):
 *   GTP_Breakable_R   tips over (bins, tables, chairs, bikes, pots)
 *   GTP_Breakable_C   breaks above an energy threshold (lamps, signs, hydrants)
 *   GTP_MassKg=<n>    mass of one object, kilograms (default 20 for R, 100 for C)
 *   GTP_BrokenMesh=<asset path>   optional: what stays in place once broken (stump, bent pole), fixed
 *   GTP_BrokenTopMesh=<asset path> optional: the part that falls, thrown as a physics body
 *   (with BrokenMesh alone, e.g. a bent sign, the instance is only replaced, no body)
 * Nothing runs when no component carries a tag. -Breakables=0 turns it off.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPBreakableSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	int32 GetThrownCount() const { return Thrown.Num(); }
	int32 GetTotalBroken() const { return TotalHit; }

protected:
	/** Physics bodies alive at once; the oldest is removed beyond this. */
	UPROPERTY(Config)
	int32 MaxLiveM1 = 6;

	UPROPERTY(Config)
	int32 MaxLiveOther = 16;

	/** A thrown body disappears after this long (s). */
	UPROPERTY(Config)
	float ThrownLifetime = 25.f;

	UPROPERTY(Config)
	float CheckInterval = 0.05f;

	/** Impactors slower than this are ignored (km/h). */
	UPROPERTY(Config)
	float MinImpactorKmh = 3.f;

	UPROPERTY(Config)
	float PlayerMassKg = 80.f;

private:
	struct FGroup
	{
		TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
		GTPBreak::EKind Kind = GTPBreak::EKind::Tippable;
		float MassKg = 20.f;
		FSoftObjectPath BrokenMesh;
	FSoftObjectPath TopMesh;
	};

	void Collect();
	void RefreshImpactors();
	void Check();
	void SpawnFixed(UHierarchicalInstancedStaticMeshComponent* Source, UStaticMesh* Mesh, const FTransform& Transform);
	void Hit(FGroup& Group, int32 InstanceIndex, const FVector& Direction, float ClosingCmS, float ImpactorMassKg, AActor* Impactor);
	int32 MaxLive() const;

	TArray<FGroup> Groups;
	TArray<TWeakObjectPtr<AActor>> Impactors;
	TArray<TWeakObjectPtr<AStaticMeshActor>> Thrown;
	FTimerHandle CheckTimer;
	float NextRefresh = 0.f;
	int32 TotalHit = 0;
};
