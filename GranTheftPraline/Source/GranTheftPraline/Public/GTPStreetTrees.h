#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPStreetTrees.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/**
 * Leafy street trees chosen at run time, without changing the city map: every
 * hierarchical instanced mesh drawing a bare tree mesh gets a leafy twin with
 * the same instances, and the bare one keeps only the trees the twin does not
 * show. One map and one package hold every variant, so tools/perf/bench.sh
 * compares them with game arguments (tools/perf/tree-bench-mac.sh):
 *
 *   -TreeSwap=<bare>~<leafy>[+<bare>~<leafy>...]  mesh package paths, e.g.
 *                    /Game/Environment/Props/W14-63/SM_X~/Game/BlackAlder/Geometry/SimpleWind/SM_BlackAlder_Forest_08
 *                    (the leafy meshes must be cooked: DirectoriesToAlwaysCook)
 *   -Trees=bare      (default) nothing happens, the street looks as it did
 *   -Trees=leafy     leafy mesh as it comes from the pack (wind, shadows, all LODs)
 *   -Trees=light     no wind beyond -TreeWindCm (3000), VSM pages not invalidated
 *                    by wind, LOD 0 skipped, culled beyond -TreeCullCm (20000),
 *                    Nanite fallback when the mesh is Nanite
 *   -Trees=impostor  every tree on its last LOD (the billboard), no wind
 *   -TreeCount=N     only N leafy trees, spread along the street; the others stay bare
 *   -TreeShadows=0   leafy trees cast no shadow
 *
 * The twin has no collision: the bare trunk keeps it. Cost: one pass over the
 * actors at BeginPlay, a hundred instances copied.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPStreetTreesSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Indices 0..Total-1 of Count trees spread evenly (all of them when Count >= Total). */
	static TArray<int32> SpreadIndices(int32 Total, int32 Count);

private:
	void ApplyVariant(UHierarchicalInstancedStaticMeshComponent* Leafy, const FString& Variant) const;
};
