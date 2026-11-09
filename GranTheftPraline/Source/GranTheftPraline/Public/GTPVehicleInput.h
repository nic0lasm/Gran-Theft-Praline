#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GTPVehicleInput.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Vehicle and interaction input, built in code so that no input asset has
 * to be authored in the editor. Owned by the player controller.
 *
 * Contexts:
 * - Interaction (always active, on foot and driving): enter / exit.
 * - DrivingKeyboard (AZERTY or QWERTY) and DrivingGamepad: added while a
 *   vehicle is possessed, in place of the on-foot contexts.
 *
 * Bindings (docs/gameplay/vehicles.md, section 5):
 *   action        AZERTY        QWERTY        gamepad
 *   enter / exit  F             F             Y (top face button)
 *   throttle      Z, Up         W, Up         right trigger
 *   brake/reverse S, Down       S, Down       left trigger
 *   steer         Q / D, arrows A / D, arrows left stick X
 *   handbrake     Space         Space         right shoulder
 *   look          mouse         mouse         right stick
 *   look behind   C             C             right stick click
 *   view          V             V             View (special left)
 *
 * On foot, in the always-active Interaction context
 * (docs/gameplay/on-foot-feel.md):
 *   interact      E             E             D-pad right
 *   recenter view middle mouse  middle mouse  -
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPVehicleInput : public UObject
{
	GENERATED_BODY()

public:
	/** Creates the actions and contexts. Calling it again rebuilds the keyboard context for a new layout. */
	void Build(bool bQwerty);

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> EnterExit;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Throttle;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Brake;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Steer;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Handbrake;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Look;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookBehind;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ToggleView;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Interact;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CameraRecenter;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Interaction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DrivingKeyboard;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> DrivingGamepad;

	bool IsQwerty() const { return bBuiltQwerty; }

private:
	void BuildActions();
	void BuildKeyboard(bool bQwerty);
	void BuildGamepadAndInteraction();

	bool bBuiltQwerty = false;
};
