#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GTPCombatInput.generated.h"

class APlayerController;
class UInputAction;
class UInputMappingContext;
class UGTPWeaponComponent;
struct FInputActionValue;

/**
 * The player's combat inputs (ADR-0011), built in code like the vehicle inputs so no
 * input asset is needed: aim, fire, reload, draw or holster, swap shoulder. The mapping
 * context is present only while the player is on foot (UpdateContext), and the actions
 * never consume their keys, so driving (triggers) is untouched. Remapping is the
 * interface thread's (Enhanced Input user settings over these actions).
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPCombatInput : public UObject
{
	GENERATED_BODY()

public:
	/** Builds the actions and binds them on the controller's input component (once). */
	void Bind(APlayerController* Controller);

	/** Adds or removes the mapping context: present only when bOnFoot. */
	void UpdateContext(bool bOnFoot);

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
	TObjectPtr<UInputMappingContext> Context;

	/** Above the on-foot contexts, below nothing that matters: the actions do not consume keys. */
	int32 Priority = 2;

private:
	UGTPWeaponComponent* GetWeapon() const;
	void OnAimStarted();
	void OnAimEnded();
	void OnFireStarted();
	void OnFireEnded();
	void OnReload();
	void OnHolster();
	void OnShoulder();

	TWeakObjectPtr<APlayerController> Owner;
	bool bContextAdded = false;
};
