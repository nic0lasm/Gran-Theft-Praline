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

	/**
	 * In the package PackagePath (one of our copies), every reference to an object of a package
	 * named by a key of FromTo becomes a reference to the object of the same name in the package of
	 * its value (our copy of that package): properties, Blueprint graph pins and compiled code alike,
	 * through the engine's reference replacement (FArchiveReplaceObjectRef, as "Replace References"
	 * does), which reaches what the exported text of the properties does not show. A Blueprint is
	 * compiled again. Nothing is deleted; saving is the caller's. Editor only. JSON: replaced (count),
	 * mapped (objects matched per package), compiled (the Blueprint's status), errors.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static FString ReplacePackageReferences(const FString& PackagePath, const TMap<FString, FString>& FromTo);

	/**
	 * Which objects of the loaded package PackagePath still reference an object of one of the
	 * Watched packages, hard or soft, as the package's objects serialize them. JSON: a list of
	 * "object -> referenced" lines. Tells where a reference the replacement missed lives.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static FString ListReferencesTo(const FString& PackagePath, const TArray<FString>& Watched);

	/**
	 * Has the asset registry read these packages again from disk (after a save in the same editor
	 * session its dependencies can still be the old ones). Returns the number of files scanned.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static int32 RescanPackages(const TArray<FString>& PackagePaths);
};
