#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPCameraModeComponent.generated.h"

class ACameraActor;
class APawn;
class APlayerController;

/**
 * Third person / first person switch, owned by the player controller so the
 * chosen mode survives a respawn.
 *
 * Third person is the pawn's own camera (the Game Animation Sample gameplay
 * camera), untouched. First person blends the view target to a camera actor
 * placed at the head bone and turned with the control rotation; the head is
 * hidden on the player's mesh (bone hide) and the body stays visible. While in
 * first person the character is asked to strafe, so the body faces the camera.
 */
UCLASS(Config = Game, ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPCameraModeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPCameraModeComponent();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ToggleCameraMode();

	UFUNCTION(BlueprintCallable, Category = "Camera")
	void SetFirstPerson(bool bEnable, bool bInstant = false);

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsFirstPerson() const { return bFirstPerson; }

	/** Called by the controller after it possesses a pawn: re-applies the current mode. */
	void HandlePawnChanged(APawn* NewPawn);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(Config, EditAnywhere, Category = "First person")
	float FirstPersonFOV = 90.f;

	UPROPERTY(Config, EditAnywhere, Category = "First person")
	float BlendTime = 0.2f;

	/** Bone whose geometry is hidden in first person (its children are hidden too). */
	UPROPERTY(Config, EditAnywhere, Category = "First person")
	FName HeadBoneName = TEXT("head");

	/** Offset from the head bone, in the control rotation frame (X forward, Z up), in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "First person")
	FVector EyeOffset = FVector(12.f, 0.f, 6.f);

private:
	APlayerController* GetPlayerController() const;
	void ApplyToPawn(APawn* Pawn, bool bEnable);
	void UpdateFirstPersonCamera();
	static void SetWantsToStrafe(APawn* Pawn, bool bWants, bool& bOutPrevious);

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> FirstPersonCamera;

	TWeakObjectPtr<APawn> AppliedPawn;
	bool bFirstPerson = false;
	bool bPreviousWantsToStrafe = false;
};
