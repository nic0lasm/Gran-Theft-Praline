#pragma once

#include "CoreMinimal.h"
#include "GTPLiteDriver.h"
#include "GTPPoliceTactics.h"
#include "GTPPoliceOfficer.generated.h"

class UGTPWeaponComponent;
class UMeshComponent;

/** One rigid piece of the uniform (a cap, a vest, a belt): a static mesh hung on a bone of the body. */
USTRUCT()
struct FGTPOfficerAccessory
{
	GENERATED_BODY()

	/** Static mesh asset. */
	UPROPERTY()
	FSoftObjectPath Mesh;

	/** Bone of the body it follows (head, pelvis, ...). A bone the body does not have: the piece is left out. */
	UPROPERTY()
	FName Bone;

	/** Place, turn and size of the piece in the bone's own space (cm, degrees). */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	float Scale = 1.f;
};

/**
 * A police officer on foot (ADR-0004, tactics in GTPPoliceTactics.h). Put on the ground by the
 * AGTPPoliceDirector when the player is on foot near a police car (or the car is stuck).
 * It runs at the player while the level is high enough and the police see them, goes to the last
 * known position and then sweeps a widening circle around it while searching, and shoots at three
 * stars or more (two once the player has fired or threatened), through the weapon of the combat
 * layer (UGTPWeaponComponent::FireAt). No navigation mesh: it steers straight and slides along
 * walls; the real path comes with the alleys (traboules) scenario.
 *
 * A pawn of its own with a light body (the AGTPLiteDriver mesh logic: the player's mesh unless
 * -LiteDriverMesh= says otherwise), one actor tick, no camera.
 *
 * The uniform (video of candidate 59: the officers were the player's orange mannequin with a pistol): when
 * UniformBody is set and loads, the officer is an assembled character of City Sample Crowds, like the people of
 * the street: a body, garments and a face that follow its pose, a colour on the garments, and rigid pieces hung on
 * its bones. Everything is read from the config, nothing is made here. -PoliceUniform=0 keeps the old body.
 */
UCLASS(NotBlueprintable, Config = Game)
class GRANTHEFTPRALINE_API AGTPPoliceOfficer : public AGTPLiteDriver
{
	GENERATED_BODY()

public:
	AGTPPoliceOfficer(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	/** Slot among the officers of the director, for the search pattern. */
	void SetSlot(int32 InIndex, int32 InCount)
	{
		SlotIndex = InIndex;
		SlotCount = FMath::Max(1, InCount);
	}

	/** Points chosen by EQS (UGTPPoliceQueries), best first: the officer visits them in turn while searching; empty = the circle sweep. */
	void SetSearchPoints(const TArray<FVector>& Points) { SearchPoints = Points; }

	/** Looping run animation; empty = the mesh's reference pose (slides). -PoliceRunAnim= overrides. */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	FSoftObjectPath RunAnimation;

	/** The uniform's body (a skeletal mesh of City Sample Crowds); empty or not loadable = no uniform. */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	FSoftObjectPath UniformBody;

	/** Garments made for that body (skeletal meshes on its skeleton): they follow its pose. */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	TArray<FSoftObjectPath> UniformClothes;

	/** The head (the crowd's bodies have none of their own); it follows the body's pose. */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	FSoftObjectPath UniformFace;

	/** Looping run animation made for the uniform's body (another skeleton than the mannequin's). */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	FSoftObjectPath UniformRunAnimation;

	/** Yaw of the uniform's body inside the capsule. */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	float UniformYawOffset = -90.f;

	/** The colour of the garments (navy blue). Given to the materials in the two ways below; both empty: no colour. */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	FLinearColor UniformColour = FLinearColor(0.012f, 0.02f, 0.06f, 1.f);

	/**
	 * Folder of the garments' own materials: for slot N of the garment G, MI_GTP_Uniform_G_N when it exists (navy
	 * blue children of Epic's materials, made in the editor by tools/police/unreal/make_police_look.py, out of Git).
	 * Cooked materials: nothing is made at run time, unlike the two ways below.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	FString UniformMaterialFolder = TEXT("/Game/Crowd/GTP/Police");

	/** Vector parameters of the garments' materials that take the colour (a dynamic material instance per slot). */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	TArray<FName> UniformColourParameters;

	/** First of the four custom primitive data floats that take the colour, for materials that read it there; -1 = none. */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	int32 UniformColourDataIndex = -1;

	/** Cap, vest, belt: rigid pieces on the bones of the body. */
	UPROPERTY(Config, EditAnywhere, Category = "Police|Uniform")
	TArray<FGTPOfficerAccessory> UniformAccessories;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool HasLineOfSight(const FVector& From, const AActor& Target) const;
	/** Puts the uniform on; false when there is none to put on (the body the lite driver chose stays). */
	bool WearUniform();

	/** The garments, the face and the rigid pieces. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMeshComponent>> UniformParts;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UGTPWeaponComponent> Weapon;

	TArray<FVector> SearchPoints;
	GTPPoliceTactics::FSettings Tactics;
	int32 SlotIndex = 0;
	int32 SlotCount = 1;
	float FireCooldown = 0.f;
	float SpawnTime = 0.f;
	float SearchSeconds = 0.f;
	float NextLosTime = 0.f;
	bool bLosCached = false;
};
