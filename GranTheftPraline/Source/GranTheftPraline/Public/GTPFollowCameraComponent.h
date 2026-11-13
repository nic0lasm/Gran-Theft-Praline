#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPFollowCameraComponent.generated.h"

class ACameraActor;
class ACharacter;
class APawn;
class APlayerController;
class USkeletalMeshComponent;

/**
 * On-foot third person camera in the style of open-world action games, owned
 * by the player controller (docs/gameplay/on-foot-feel.md).
 *
 * The view orbits a pivot above the character's shoulders, turned by the
 * control rotation (mouse and right stick, no rotation lag), with:
 * - position lag: the pivot trails the character a little, more vertically
 *   than horizontally, so jumps, stairs and stops read as weight;
 * - over-the-right-shoulder framing, closer when crouched, farther when
 *   sprinting, closer when looking up so the camera does not dig into the
 *   ground;
 * - collision: a sphere sweep pulls the camera in at once in front of a wall
 *   and lets it out slowly;
 * - field of view widening while sprinting;
 * - automatic recentering behind the direction of travel after a few seconds
 *   without look input, and an instant recenter on request (middle mouse).
 *
 * It drives a camera actor that becomes the view target, so the sample
 * character's own camera assets stay untouched; bEnabled=False in the config
 * gives the sample camera back.
 */
UCLASS(Config = Game, ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPFollowCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPFollowCameraComponent();

	bool IsEnabled() const { return bEnabled; }

	/**
	 * Camera actor to use as the third person view target for this pawn, or
	 * null when the follow camera is disabled or the pawn is not a character.
	 * bSnap places the camera behind the character at once (spawn, respawn).
	 */
	AActor* GetViewTargetFor(APawn* Pawn, bool bSnap);

	/** Starts or stops driving the camera (third person on foot only). */
	void SetDriving(bool bDrive);

	/** Called by the controller whenever look input arrives. */
	void NotifyLookInput();

	/** Turns the view behind the character over RecenterBlendTime. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void RequestRecenter();

	/** Current camera distance from the pivot after collision, in cm (playtest checks). */
	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetCurrentDistance() const { return CurrentDistance; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** False gives the view back to the character's own camera. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera")
	bool bEnabled = true;

	/** Pivot height above the capsule centre, in cm (about the shoulders). */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera")
	float PivotHeight = 62.f;

	/** Distance behind the pivot at walking pace, in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera")
	float ArmLength = 290.f;

	/** Extra distance at full sprint, in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera")
	float SprintArmExtra = 45.f;

	/** Distance removed while crouched, in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera")
	float CrouchArmReduction = 45.f;

	/** Arm length multiplier when looking fully up (camera comes closer). */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera")
	float LookUpArmScale = 0.55f;

	/** Right shoulder offset (Y) and height offset (Z) of the view, in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera")
	FVector2D ShoulderOffset = FVector2D(38.f, 8.f);

	/** How fast the arm length follows its target, 1/s. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera")
	float ArmSpeed = 3.f;

	/** Position lag of the pivot, 1/s (higher follows tighter). 0 disables. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Lag")
	float HorizontalLagSpeed = 11.f;

	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Lag")
	float VerticalLagSpeed = 6.f;

	/** Vertical lag while airborne (jumps, falls), 1/s. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Lag")
	float AirVerticalLagSpeed = 3.5f;

	/** The pivot never trails more than this, in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Lag")
	float MaxHorizontalLag = 55.f;

	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Lag")
	float MaxVerticalLag = 70.f;

	/** Rotation smoothing, 1/s. 0 = none (mouse feels direct). */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Lag")
	float RotationLagSpeed = 0.f;

	/** Radius of the collision probe, in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Collision")
	float ProbeRadius = 14.f;

	/** How fast the camera moves back out after an obstacle, 1/s. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Collision")
	float CollisionRecoverSpeed = 4.f;

	/** Below this distance from the pivot the body is hidden for the player, in cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Collision")
	float MinDistance = 45.f;

	/** Horizontal field of view standing still and walking, in degrees. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Field of view")
	float BaseFOV = 78.f;

	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Field of view")
	float SprintFOV = 86.f;

	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Field of view")
	float FOVSpeed = 3.f;

	/** Ground speed from which the run blends towards the sprint framing, and full sprint, cm/s. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Field of view")
	float SprintBlendStartSpeed = 480.f;

	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Field of view")
	float SprintFullSpeed = 650.f;

	/**
	 * Over-the-shoulder aim (GTA-like): while the player aims a gun the camera comes in
	 * over the shoulder the weapon names (UGTPWeaponComponent::IsRightShoulder), narrows
	 * the view and stops drifting; the shoulder swap slides across.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Aim")
	float AimArmLength = 135.f;
	/** Shoulder offset (Y) and height (Z) while aiming, cm; Y flips for the left shoulder. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Aim")
	FVector2D AimShoulderOffset = FVector2D(58.f, 14.f);
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Aim")
	float AimFOV = 62.f;
	/** How fast the framing goes in and out of the aim, 1/s. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Aim")
	float AimBlendSpeed = 12.f;

	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Recenter")
	bool bAutoRecenter = true;

	/** Seconds without look input before the view drifts behind the character. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Recenter")
	float RecenterDelay = 2.5f;

	/** Drift strength at running pace, 1/s (scaled by speed, nothing below MinRecenterSpeed). */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Recenter")
	float RecenterRate = 0.8f;

	/** Fastest the drift may turn the view, degrees per second (running sideways curves gently). */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Recenter")
	float MaxRecenterTurnRate = 35.f;

	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Recenter")
	float MinRecenterSpeed = 120.f;

	/** No drift when walking towards the camera (yaw gap above this, degrees). */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Recenter")
	float MaxRecenterAngle = 100.f;

	/** Pitch the drift and the manual recenter go back to, degrees. */
	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Recenter")
	float RecenterPitch = -10.f;

	UPROPERTY(Config, EditAnywhere, Category = "Follow camera|Recenter")
	float RecenterBlendTime = 0.25f;

private:
	APlayerController* GetPlayerController() const;
	ACameraActor* EnsureCamera();
	void Snap(const ACharacter* Character);
	void UpdateRecenter(APlayerController* PC, const ACharacter* Character, float DeltaTime);
	void UpdateCamera(APlayerController* PC, const ACharacter* Character, float DeltaTime, bool bInstant);

	void SetBodyHidden(USkeletalMeshComponent* Mesh, bool bHidden);

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	/** Body hidden because the camera is pressed into the head. */
	TWeakObjectPtr<USkeletalMeshComponent> HiddenBody;

	bool bDriving = false;
	bool bHasState = false;
	FVector SmoothedPivot = FVector::ZeroVector;
	FRotator SmoothedRotation = FRotator::ZeroRotator;
	float CurrentArm = 0.f;
	float CurrentDistance = 0.f;
	float CollisionFraction = 1.f;
	float CurrentFOV = 0.f;
	/** 0 = free framing, 1 = aiming over the shoulder. */
	float AimAlpha = 0.f;
	/** +1 right shoulder, -1 left (smoothed through the swap). */
	float ShoulderSide = 1.f;
	double LastLookInputTime = -1000.0;

	bool bManualRecenter = false;
	float ManualRecenterElapsed = 0.f;
	FRotator ManualRecenterFrom = FRotator::ZeroRotator;
	FRotator ManualRecenterTo = FRotator::ZeroRotator;
};
