#include "GTPPlayerController.h"

#include "GTPCameraModeComponent.h"
#include "GTPGameInstance.h"
#include "GTPMenuWidgets.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPInput, Log, All);

namespace
{
	// Gamepad mappings are added below the keyboard ones; both stay active.
	constexpr int32 KeyboardContextPriority = 1;
	constexpr int32 GamepadContextPriority = 0;
}

AGTPPlayerController::AGTPPlayerController()
{
	CameraMode = CreateDefaultSubobject<UGTPCameraModeComponent>(TEXT("CameraMode"));
}

void AGTPPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!IsLocalController())
	{
		return;
	}
	ApplyOnFootInput();
	// The sample character adds its own context from its possession events,
	// which may run after this point: apply ours again on the next tick.
	GetWorldTimerManager().SetTimerForNextTick(this, &AGTPPlayerController::ApplyOnFootInput);
	CameraMode->HandlePawnChanged(InPawn);
}

void AGTPPlayerController::ApplyOnFootInput()
{
	UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Input)
	{
		return;
	}
	const bool bQwerty = bQwertyKeyboard || FParse::Param(FCommandLine::Get(), TEXT("Qwerty"));
	UInputMappingContext* Sample = SampleContext.LoadSynchronous();
	UInputMappingContext* Azerty = KeyboardContextAzerty.LoadSynchronous();
	UInputMappingContext* Qwerty = KeyboardContextQwerty.LoadSynchronous();
	UInputMappingContext* Gamepad = GamepadContext.LoadSynchronous();

	if (Sample)
	{
		Input->RemoveMappingContext(Sample);
	}
	UInputMappingContext* Keyboard = bQwerty ? Qwerty : Azerty;
	UInputMappingContext* OtherKeyboard = bQwerty ? Azerty : Qwerty;
	if (OtherKeyboard)
	{
		Input->RemoveMappingContext(OtherKeyboard);
	}
	if (Keyboard && !Input->HasMappingContext(Keyboard))
	{
		Input->AddMappingContext(Keyboard, KeyboardContextPriority);
	}
	if (Gamepad && !Input->HasMappingContext(Gamepad))
	{
		Input->AddMappingContext(Gamepad, GamepadContextPriority);
	}
	if (!Keyboard || !Gamepad)
	{
		UE_LOG(LogGTPInput, Warning, TEXT("On-foot mapping contexts missing (keyboard %s, gamepad %s)"),
			Keyboard ? TEXT("ok") : TEXT("missing"), Gamepad ? TEXT("ok") : TEXT("missing"));
	}
}

void AGTPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
	UInputAction* Toggle = ToggleCameraModeAction.LoadSynchronous();
	if (Enhanced && Toggle)
	{
		Enhanced->BindAction(Toggle, ETriggerEvent::Started, this, &AGTPPlayerController::OnToggleCameraMode);
	}
}

void AGTPPlayerController::OnToggleCameraMode()
{
	if (!TitleMenu && !PauseMenu)
	{
		CameraMode->ToggleCameraMode();
	}
}

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
