#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GTPPlayerController.generated.h"

class UGTPCameraModeComponent;
class UGTPMenuWidget;
class UInputAction;
class UInputMappingContext;

/**
 * Local player controller: owns the title screen, the pause menu and the
 * on-foot input.
 * Escape (keyboard) or Menu/Start (gamepad) opens the pause menu.
 *
 * On-foot input (Config/DefaultGame.ini, section of this class): after each
 * possession the sample's own mapping context is removed and ours are added,
 * keyboard AZERTY by default (-Qwerty on the command line or
 * bQwertyKeyboard=True for QWERTY) plus the gamepad context.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGTPPlayerController();

	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** Called by the title screen "Play" button. */
	void StartGameFromTitle();

	void OpenPauseMenu();
	void ClosePauseMenu();

	/** Leaves the game and returns to the desktop. */
	void QuitToDesktop();

	UGTPCameraModeComponent* GetCameraMode() const { return CameraMode; }

	/** Re-applies the on-foot mapping contexts (keyboard layout switch, after possession). */
	void ApplyOnFootInput();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ShowMenu(UGTPMenuWidget* Menu);
	void HideMenu(TObjectPtr<UGTPMenuWidget>& Menu);
	void EnterGameplayInput();

	static bool IsPauseKey(const FKey& Key);
	void OnToggleCameraMode();

	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> KeyboardContextAzerty;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> KeyboardContextQwerty;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> GamepadContext;

	/** Context added by the sample character on possession, replaced by ours. */
	UPROPERTY(Config)
	TSoftObjectPtr<UInputMappingContext> SampleContext;

	UPROPERTY(Config)
	TSoftObjectPtr<UInputAction> ToggleCameraModeAction;

	UPROPERTY(Config)
	bool bQwertyKeyboard = false;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UGTPCameraModeComponent> CameraMode;

	UPROPERTY(Transient)
	TObjectPtr<UGTPMenuWidget> TitleMenu;

	UPROPERTY(Transient)
	TObjectPtr<UGTPMenuWidget> PauseMenu;
};
