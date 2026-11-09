#include "GTPPlayerController.h"

#include "GTPGameInstance.h"
#include "GTPMenuWidgets.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

void AGTPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	const UGTPGameInstance* GameInstance = GetGameInstance<UGTPGameInstance>();
	if (GameInstance && !GameInstance->HasDismissedTitle())
	{
		TitleMenu = CreateWidget<UGTPTitleMenuWidget>(this, UGTPTitleMenuWidget::StaticClass());
		ShowMenu(TitleMenu);
	}
	else
	{
		EnterGameplayInput();
	}
}

void AGTPPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideMenu(TitleMenu);
	HideMenu(PauseMenu);
	Super::EndPlay(EndPlayReason);
}

bool AGTPPlayerController::IsPauseKey(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right;
}

bool AGTPPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Only reached while gameplay has input focus: open menus consume their own keys.
	if (Params.Event == IE_Pressed && IsPauseKey(Params.Key) && !TitleMenu && !PauseMenu)
	{
		OpenPauseMenu();
		return true;
	}
	return Super::InputKey(Params);
}

void AGTPPlayerController::StartGameFromTitle()
{
	if (UGTPGameInstance* GameInstance = GetGameInstance<UGTPGameInstance>())
	{
		GameInstance->MarkTitleDismissed();
	}
	HideMenu(TitleMenu);
	EnterGameplayInput();
}

void AGTPPlayerController::OpenPauseMenu()
{
	if (PauseMenu || TitleMenu)
	{
		return;
	}
	PauseMenu = CreateWidget<UGTPPauseMenuWidget>(this, UGTPPauseMenuWidget::StaticClass());
	ShowMenu(PauseMenu);
}

void AGTPPlayerController::ClosePauseMenu()
{
	if (!PauseMenu)
	{
		return;
	}
	HideMenu(PauseMenu);
	EnterGameplayInput();
}

void AGTPPlayerController::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AGTPPlayerController::ShowMenu(UGTPMenuWidget* Menu)
{
	if (!Menu)
	{
		return;
	}
	Menu->AddToViewport(100);
	SetPause(true);

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (UWidget* FocusTarget = Menu->GetDefaultFocusTarget())
	{
		InputMode.SetWidgetToFocus(FocusTarget->TakeWidget());
	}
	else
	{
		InputMode.SetWidgetToFocus(Menu->TakeWidget());
	}
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AGTPPlayerController::HideMenu(TObjectPtr<UGTPMenuWidget>& Menu)
{
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
}

void AGTPPlayerController::EnterGameplayInput()
{
	SetPause(false);
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
}
