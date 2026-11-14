#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GTPAnimGraphTools.generated.h"

class UAnimBlueprint;
class UBlueprint;

/**
 * Editor tools on animation blueprints, called from Python (tools/combat/unreal/player_upper_body_layer.py).
 * They change local copies of Epic's sample content, never anything in Git; in a game build they do nothing.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPAnimGraphTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Adds an upper-body layer at the end of the blueprint's AnimGraph: the final pose is cached, then a
	 * "Layered blend per bone" (mesh-space rotation blend, from RootBone down) takes the base from the cache and
	 * the upper body from a Slot named SlotName fed by the same cache, and goes to the output. Montages played in
	 * SlotName then move the upper body only, the legs keep walking. The slot name is registered on the skeleton.
	 * Does nothing if the layer is already there. Returns a one-line report (starts with "ok", "kept" or "error").
	 * The caller saves the blueprint and the skeleton.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static FString AddUpperBodyLayer(UAnimBlueprint* AnimBlueprint, FName SlotName, FName RootBone);

	/**
	 * Sets the bone of every "SourceRoot" in the retarget ops of an IK retargeter (the Root Motion op) to Bone, in
	 * the asset itself through reflection, as the details panel does: on 5.8 the op controller's set_settings does
	 * not keep a changed bone. Takes the retargeter as a plain object (no IK Rig module needed). Returns a one-line
	 * report ("ok", "kept" or "error", with the bones before). The caller saves the retargeter.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static FString SetRetargetSourceRoot(UObject* Retargeter, FName Bone);

	/**
	 * One line per component of the blueprint's construction script, as space-separated key=value pairs (values
	 * without spaces, "-" when empty): name, class, mesh, its skeleton, anim class, the anim class's target
	 * skeleton, groom asset, binding asset, material overrides count and the mesh's own materials
	 * (slot:material;...). For Nicolito's visual (tools/character/create_nicolito_visual.py).
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static FString DescribeBlueprintComponents(UBlueprint* Blueprint);

	/**
	 * Sets an object property (SkeletalMeshAsset, GroomAsset, BindingAsset, AnimClass...) of the component template
	 * named Component in the blueprint's construction script, as the details panel does; a null Asset clears it.
	 * Property "OverrideMaterials" with a null Asset empties the material overrides. Returns "ok: ..." with the
	 * value before, "kept: ..." or "error: ...". The caller compiles and saves the blueprint.
	 */
	UFUNCTION(BlueprintCallable, Category = "GTP|Editor")
	static FString SetBlueprintComponentAsset(UBlueprint* Blueprint, FName Component, FName Property, UObject* Asset);
};
