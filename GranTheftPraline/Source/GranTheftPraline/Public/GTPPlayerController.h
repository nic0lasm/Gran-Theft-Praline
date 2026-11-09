#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GTPPlayerController.generated.h"

class UGTPMenuWidget;

/**
 * Local player controller: owns the title screen and the pause menu.
 * Escape (keyboard) or Menu/Start (gamepad) opens the pause menu.
 */
UCLASS()
class GRANTHEFTPRALINE_API AGTPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** Called by the title screen "Play" button. */
	void StartGameFromTitle();

	void OpenPauseMenu();
	void ClosePauseMenu();

	/** Leaves the game and returns to the desktop. */
	void QuitToDesktop();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ShowMenu(UGTPMenuWidget* Menu);
	void HideMenu(TObjectPtr<UGTPMenuWidget>& Menu);
	void EnterGameplayInput();

	static bool IsPauseKey(const FKey& Key);

	UPROPERTY(Transient)
	TObjectPtr<UGTPMenuWidget> TitleMenu;

	UPROPERTY(Transient)
	TObjectPtr<UGTPMenuWidget> PauseMenu;
};
