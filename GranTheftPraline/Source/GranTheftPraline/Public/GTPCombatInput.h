#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GTPCombatInput.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;
class UGTPWeaponComponent;
class UGTPCombatMeleeComponent;
struct FInputActionValue;

/**
 * The player's combat inputs (ADR-0011), built in code like the vehicle inputs so no
 * input asset is needed: aim, fire, reload, draw or holster, next weapon, swap shoulder,
 * dodge. With no gun drawn, fire punches and aim guards. On foot the context is IMC_Combat;
 * at the wheel IMC_Combat_Vehicle (aim: right mouse or left bumper, fire: left mouse, and
 * the right bumper only while aiming). The actions never consume their keys except the
 * drive-by right bumper, so driving (triggers) is untouched. Remapping is the interface
 * thread's (Enhanced Input user settings over these actions).
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPCombatInput : public UObject
{
	GENERATED_BODY()

public:
	/** Builds the actions and binds them on the controller's input component (once). */
	void Bind(APlayerController* Controller);

	/** On foot, at the wheel (drive-by), or neither (dead, cutscene). */
	enum class EMode : uint8 { None, OnFoot, Driving };

	/** Adds or removes the mapping contexts for the mode. */
	void UpdateContext(EMode Mode);

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Aim;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Fire;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Holster;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Shoulder;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DodgeAction;
	/** Next or previous weapon (mouse wheel, 2, gamepad left bumper on foot). */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NextWeapon;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PrevWeapon;
	/** Aim from the gamepad: also snaps to the best target (aim assist, GTPAimAssist). */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimPad;
	/** Drive-by fire on the gamepad's right bumper, only while aiming (it is the handbrake otherwise). */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DriveByFire;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Context;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> VehicleContext;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DriveByFireContext;

	/** Above the on-foot contexts, below nothing that matters: the actions do not consume keys. */
	int32 Priority = 2;
	/** Above the driving contexts: while aiming in a car, the right bumper fires instead of braking. */
	int32 DriveByFirePriority = 6;

private:
	UGTPWeaponComponent* GetWeapon() const;
	/** The fists, when no gun is drawn. */
	UGTPCombatMeleeComponent* GetFists() const;
	void OnDodge();
	void OnAimStarted();
	void OnAimEnded();
	void OnFireStarted();
	void OnFireEnded();
	void OnReload();
	void OnHolster();
	void OnShoulder();
	void OnNextWeapon();
	void OnPrevWeapon();
	void OnAimPadStarted();
	void SetDriveByFireContext(bool bOn);

	TWeakObjectPtr<APlayerController> Owner;
	EMode CurrentMode = EMode::None;
};
