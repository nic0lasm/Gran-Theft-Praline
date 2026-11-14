#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GTPAssetReflection.generated.h"

/**
 * Reads and sets any property of an asset or of its instanced subobjects (Mass entity config
 * traits, spawner data generators) by name, through the engine's reflection and text export, for
 * the editor scripts: most of MassTraffic's classes are not exposed to Python in 5.8.
 * tools/city/unreal/place_masstraffic.py uses it to see what City Sample's traffic configs
 * reference, and to point copies of them at cookable assets (docs/gameplay/traffic-mass.md).
 */
UCLASS()
class GTPMASS_API UGTPAssetReflection : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * JSON of Object: its path, class, and every property as the engine exports it to text, then
	 * the same for each subobject it owns (traits of a Mass entity config, generators of a spawner),
	 * down to MaxDepth levels. Values longer than 16 000 characters are cut (and end with "...").
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static FString DescribeObject(UObject* Object, int32 MaxDepth = 3);

	/**
	 * Sets one property of the object at ObjectPath (an asset or a subobject path as DescribeObject
	 * gives it) from text, as the details panel would: PropertyPath is a property name, or
	 * struct members joined by dots ("Params.LODRepresentation"). Returns "" when done, else the
	 * reason. The package is marked dirty; saving is the caller's.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static FString SetPropertyText(const FString& ObjectPath, const FString& PropertyPath, const FString& Text);
};
