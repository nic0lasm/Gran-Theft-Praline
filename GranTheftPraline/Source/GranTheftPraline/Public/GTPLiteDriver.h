#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GTPLiteDriver.generated.h"

class UAnimSequenceBase;

/**
 * A cheap occupant for AI cars (traffic drivers, police): an ACharacter, so a vehicle can seat
 * it, pull it out and let it run, but with none of the player character's gameplay machinery:
 * no camera, no spring arm, no animation blueprint, no actor tick, and a movement component
 * that does not tick while the occupant is seated (movement mode None). Movement is switched
 * back on by the vehicle's release (SetMovementMode(MOVE_Walking)).
 *
 * Mesh: Config MeshPath (-LiteDriverMesh=), else the player pawn's skeletal mesh at BeginPlay.
 * The seated pose is one looping animation (Config SeatedAnimation, -LiteDriverAnim=), optional.
 *
 * Wire it with DriverClass / PoliceDriverClass = /Script/GranTheftPraline.GTPLiteDriver.
 */
UCLASS(NotBlueprintable, Config = Game)
class GRANTHEFTPRALINE_API AGTPLiteDriver : public ACharacter
{
	GENERATED_BODY()

public:
	AGTPLiteDriver(const FObjectInitializer& ObjectInitializer);

	/** Skeletal mesh asset path; empty = the player's mesh. */
	UPROPERTY(Config, EditAnywhere, Category = "Driver")
	FSoftObjectPath MeshPath;

	/** Looping seated pose; empty = the mesh's reference pose. */
	UPROPERTY(Config, EditAnywhere, Category = "Driver")
	FSoftObjectPath SeatedAnimation;

	/** Yaw of the mesh inside the capsule (UE characters face +X, some meshes -Y). */
	UPROPERTY(Config, EditAnywhere, Category = "Driver")
	float MeshYawOffset = -90.f;

protected:
	virtual void BeginPlay() override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	void ApplyMovementTick();
};
