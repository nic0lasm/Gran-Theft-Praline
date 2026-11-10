#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/PlayerController.h"
#include "GTPPlayerController.generated.h"

class UAudioComponent;
class UGTPCameraModeComponent;
class UGTPFollowCameraComponent;
class UGTPInteractionComponent;
class UGTPMenuWidget;
class UGTPVehicleInput;
class UGTPVehicleInteractionComponent;
class UInputAction;
class UInputMappingContext;
class USoundBase;

/**
 * Local player controller: owns the title screen, the pause menu and the
 * on-foot input.
 * Escape (keyboard) or Menu/Start (gamepad) opens the pause menu.
 *
 * On-foot input (Config/DefaultGame.ini, section of this class): after each
 * possession the sample's own mapping context is removed and ours are added,
 * keyboard AZERTY by default (-Qwerty on the command line or
 * bQwertyKeyboard=True for QWERTY) plus the gamepad context.
 *
 * Vehicles (docs/gameplay/vehicles.md): the enter / exit action stays active
 * on foot and at the wheel; while a vehicle is possessed the on-foot
 * contexts are swapped for the driving ones (UGTPVehicleInput, built in code).
 *
 * On foot (docs/gameplay/on-foot-feel.md): third person view from
 * UGTPFollowCameraComponent, contextual interaction (E) from
 * UGTPInteractionComponent, HUD drawn by AGTPHUD, and a street ambience
 * loop played in 2D.
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

	/** Settings screen, from the title screen or the pause menu; Close returns to that menu. */
	/** Full-screen map (M, gamepad D-pad up): pauses the game; the same key, Escape or B closes it. */
	void ToggleFullMap();
	/** True when the keyboard keys shown and bound are QWERTY (setting, command line or project default). */
	bool IsUsingQwerty() const { return UsesQwerty(); }
	bool IsFullMapOpen() const { return bFullMapOpen; }

	void OpenSettingsMenu(bool bControlsScreen = false);
	void CloseSettingsMenu();

	/** Leaves the game and returns to the desktop. */
	void QuitToDesktop();

	UGTPCameraModeComponent* GetCameraMode() const { return CameraMode; }
	UGTPFollowCameraComponent* GetFollowCamera() const { return FollowCamera; }
	UGTPInteractionComponent* GetInteraction() const { return Interaction; }

	/** True while the title screen or the pause menu is shown. */
	bool IsMenuOpen() const { return TitleMenu != nullptr || PauseMenu != nullptr || SettingsMenu != nullptr || bFullMapOpen; }

	/** Look input is scaled by the player's sensitivity setting; pitch can be inverted. */
	virtual void AddYawInput(float Val) override;
	virtual void AddPitchInput(float Val) override;

	/** Re-applies the mapping contexts of the controlled pawn: on foot or driving (keyboard layout switch, after possession). */
	void ApplyInputForPawn();

	/** Vehicle and interaction input actions, created on first use. */
	UGTPVehicleInput* GetVehicleInput();

	UGTPVehicleInteractionComponent* GetVehicleInteraction() const { return VehicleInteraction; }

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
	void OnEnterExit();
	void OnInteract();
	void OnCameraRecenter();
	void StartAmbience();
	void ApplyOnFootInput();
	void ApplyDrivingInput();
	bool UsesQwerty() const;
	void HandleSettingsChanged();

	/**
	 * Gives the view a level starting rotation (pawn yaw, StartViewPitch) and
	 * ignores look input for LookSettleTime seconds, so the mouse delta produced
	 * when the window captures the cursor cannot throw the camera to the ground.
	 */
	void SettleView(bool bResetRotation);
	void EndLookSettle();
	void ApplyViewPitchLimits();

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

	/** Street ambience loop played in 2D during play (empty = none). */
	UPROPERTY(Config)
	TSoftObjectPtr<USoundBase> AmbienceSound;

	UPROPERTY(Config)
	float AmbienceVolume = 0.5f;

	UPROPERTY(Config)
	float AmbienceFadeInTime = 3.f;

	/** Pitch of the view when gameplay starts or after a respawn, in degrees. */
	UPROPERTY(Config)
	float StartViewPitch = -10.f;

	/** Look input is ignored this long after entering gameplay, in seconds. */
	UPROPERTY(Config)
	float LookSettleTime = 0.3f;

	/** Vertical view limits, in degrees (third and first person). */
	UPROPERTY(Config)
	float MinViewPitch = -70.f;

	UPROPERTY(Config)
	float MaxViewPitch = 60.f;

	bool bLookSettling = false;
	bool bViewSettledOnce = false;
	FTimerHandle LookSettleTimer;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UGTPCameraModeComponent> CameraMode;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UGTPFollowCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<UGTPInteractionComponent> Interaction;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	TObjectPtr<UGTPVehicleInteractionComponent> VehicleInteraction;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Ambience;

	UPROPERTY(Transient)
	TObjectPtr<UGTPVehicleInput> VehicleInput;

	UPROPERTY(Transient)
	TObjectPtr<UGTPMenuWidget> TitleMenu;

	UPROPERTY(Transient)
	TObjectPtr<UGTPMenuWidget> PauseMenu;

	UPROPERTY(Transient)
	TObjectPtr<UGTPMenuWidget> SettingsMenu;

	/** The settings screen was opened from the title screen (else from the pause menu). */
	bool bSettingsFromTitle = false;
	bool bFullMapOpen = false;
};
