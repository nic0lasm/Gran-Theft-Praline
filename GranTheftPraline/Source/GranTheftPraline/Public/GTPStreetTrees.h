#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPStreetTrees.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/**
 * Picks which street trees the player sees: the bare plane trees of the city
 * import, or a leafy copy staged next to them by tools/city/unreal/swap_street_trees.py
 * (mode "stage"), with the M1 settings of a variant. One map and one package
 * hold every variant, so tools/perf/bench.sh compares them with game arguments:
 *
 *   -Trees=bare      (default) leafy copy hidden, the map looks as it did
 *   -Trees=leafy     leafy mesh as it comes from the pack (wind, shadows, all LODs)
 *   -Trees=light     no wind beyond -TreeWindCm (3000), VSM pages not invalidated
 *                    by wind, LOD 0 skipped, culled beyond -TreeCullCm (20000),
 *                    Nanite fallback when the mesh is Nanite
 *   -Trees=impostor  every tree on its last LOD (the billboard), no wind
 *   -TreeCount=N     only N leafy trees, spread along the street; the others stay bare
 *   -TreeShadows=0   leafy trees cast no shadow
 *
 * Actors: tag GTP_LeafyTrees on the leafy copy, GTP_BareTrees on the bare
 * components; both hold the same instances in the same order, so instance i of
 * one is tree i of the other. Without a leafy copy on the map, nothing happens.
 * Cost: one pass over two actors at BeginPlay.
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
