#include "GTPPlayerController.h"

#include "GTPCameraModeComponent.h"
#include "GTPFollowCameraComponent.h"
#include "GTPInteraction.h"
#include "GTPGameInstance.h"
#include "GTPMenuWidgets.h"
#include "GTPMinimap.h"
#include "GTPUserSettings.h"
#include "GTPVehicle.h"
#include "GTPVehicleInput.h"
#include "GTPVehicleInteractionComponent.h"
#include "GTPWanted.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPInput, Log, All);

namespace
{
	// Gamepad mappings are added below the keyboard ones; both stay active.
	constexpr int32 KeyboardContextPriority = 1;
	constexpr int32 GamepadContextPriority = 0;
	// Enter / exit sits above everything so the on-foot contexts cannot hide it.
	constexpr int32 InteractionContextPriority = 2;
}

AGTPPlayerController::AGTPPlayerController()
{
	CameraMode = CreateDefaultSubobject<UGTPCameraModeComponent>(TEXT("CameraMode"));
	FollowCamera = CreateDefaultSubobject<UGTPFollowCameraComponent>(TEXT("FollowCamera"));
	Interaction = CreateDefaultSubobject<UGTPInteractionComponent>(TEXT("Interaction"));
	VehicleInteraction = CreateDefaultSubobject<UGTPVehicleInteractionComponent>(TEXT("VehicleInteraction"));
	Wanted = CreateDefaultSubobject<UGTPWantedComponent>(TEXT("Wanted"));
}

bool AGTPPlayerController::UsesQwerty() const
{
	if (FParse::Param(FCommandLine::Get(), TEXT("Qwerty")))
	{
		return true;
	}
	// The player's choice (settings screen) comes before the project default.
	if (const UGTPUserSettings* User = UGTPUserSettings::Find(this))
	{
		switch (User->Get().Keyboard)
		{
		case GTPUI::EKeyboardLayout::Azerty: return false;
		case GTPUI::EKeyboardLayout::Qwerty: return true;
		default: break;
		}
	}
	return bQwertyKeyboard;
}

UGTPVehicleInput* AGTPPlayerController::GetVehicleInput()
{
	if (!VehicleInput)
	{
		VehicleInput = NewObject<UGTPVehicleInput>(this, TEXT("VehicleInput"), RF_Transient);
	}
	VehicleInput->Build(UsesQwerty());
	return VehicleInput;
}

void AGTPPlayerController::ApplyInputForPawn()
{
	if (Cast<AGTPVehicle>(GetPawn()))
	{
		ApplyDrivingInput();
	}
	else
	{
		ApplyOnFootInput();
	}
}

void AGTPPlayerController::ApplyDrivingInput()
{
	UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Input)
	{
		return;
	}
	for (const TSoftObjectPtr<UInputMappingContext>& OnFoot : {SampleContext, KeyboardContextAzerty, KeyboardContextQwerty, GamepadContext})
	{
		if (UInputMappingContext* Context = OnFoot.LoadSynchronous())
		{
			Input->RemoveMappingContext(Context);
		}
	}
	UGTPVehicleInput* Vehicle = GetVehicleInput();
	if (!Input->HasMappingContext(Vehicle->Interaction))
	{
		Input->AddMappingContext(Vehicle->Interaction, InteractionContextPriority);
	}
	if (!Input->HasMappingContext(Vehicle->DrivingKeyboard))
	{
		Input->AddMappingContext(Vehicle->DrivingKeyboard, KeyboardContextPriority);
	}
	if (!Input->HasMappingContext(Vehicle->DrivingGamepad))
	{
		Input->AddMappingContext(Vehicle->DrivingGamepad, GamepadContextPriority);
	}
}

void AGTPPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!IsLocalController())
	{
		return;
	}
	ApplyInputForPawn();
	// The sample character adds its own context from its possession events,
	// which may run after this point: apply ours again on the next tick.
	GetWorldTimerManager().SetTimerForNextTick(this, &AGTPPlayerController::ApplyInputForPawn);
	CameraMode->HandlePawnChanged(InPawn);
	if (bViewSettledOnce)
	{
		// Respawn: face where the new pawn faces.
		SettleView(true);
	}
}

void AGTPPlayerController::SettleView(bool bResetRotation)
{
	if (bResetRotation)
	{
		if (const APawn* ControlledPawn = GetPawn())
		{
			SetControlRotation(FRotator(StartViewPitch, ControlledPawn->GetActorRotation().Yaw, 0.f));
			bViewSettledOnce = true;
		}
	}
	RotationInput = FRotator::ZeroRotator;
	if (!bLookSettling)
	{
		SetIgnoreLookInput(true);
		bLookSettling = true;
	}
	GetWorldTimerManager().SetTimer(LookSettleTimer, this, &AGTPPlayerController::EndLookSettle, LookSettleTime, false);
}

void AGTPPlayerController::EndLookSettle()
{
	RotationInput = FRotator::ZeroRotator;
	if (bLookSettling)
	{
		SetIgnoreLookInput(false);
		bLookSettling = false;
	}
	UE_LOG(LogGTPInput, Log, TEXT("View settled at %s"), *GetControlRotation().ToCompactString());
}

void AGTPPlayerController::ApplyViewPitchLimits()
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = MinViewPitch;
		PlayerCameraManager->ViewPitchMax = MaxViewPitch;
	}
}

void AGTPPlayerController::ApplyOnFootInput()
{
	UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!Input)
	{
		return;
	}
	const bool bQwerty = UsesQwerty();

	// Leaving a vehicle: drop the driving contexts, keep enter / exit.
	UGTPVehicleInput* Vehicle = GetVehicleInput();
	Input->RemoveMappingContext(Vehicle->DrivingKeyboard);
	Input->RemoveMappingContext(Vehicle->DrivingGamepad);
	if (!Input->HasMappingContext(Vehicle->Interaction))
	{
		Input->AddMappingContext(Vehicle->Interaction, InteractionContextPriority);
	}

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
	if (Enhanced)
	{
		UGTPVehicleInput* Vehicle = GetVehicleInput();
		Enhanced->BindAction(Vehicle->EnterExit, ETriggerEvent::Started, this, &AGTPPlayerController::OnEnterExit);
		Enhanced->BindAction(Vehicle->ToggleView, ETriggerEvent::Started, this, &AGTPPlayerController::OnToggleCameraMode);
		Enhanced->BindAction(Vehicle->Interact, ETriggerEvent::Started, this, &AGTPPlayerController::OnInteract);
		Enhanced->BindAction(Vehicle->CameraRecenter, ETriggerEvent::Started, this, &AGTPPlayerController::OnCameraRecenter);
	}
}

void AGTPPlayerController::OnInteract()
{
	if (!IsMenuOpen())
	{
		Interaction->HandleInteract();
	}
}

void AGTPPlayerController::OnCameraRecenter()
{
	if (!IsMenuOpen())
	{
		FollowCamera->RequestRecenter();
	}
}

void AGTPPlayerController::AddYawInput(float Val)
{
	const UGTPUserSettings* User = UGTPUserSettings::Find(this);
	Super::AddYawInput(User ? Val * User->Get().SensitivityMultiplier() : Val);
	if (Val != 0.f && !IsLookInputIgnored())
	{
		FollowCamera->NotifyLookInput();
	}
}

void AGTPPlayerController::AddPitchInput(float Val)
{
	const UGTPUserSettings* User = UGTPUserSettings::Find(this);
	Super::AddPitchInput(User ? Val * User->Get().SensitivityMultiplier() * (User->Get().bInvertY ? -1.f : 1.f) : Val);
	if (Val != 0.f && !IsLookInputIgnored())
	{
		FollowCamera->NotifyLookInput();
	}
}

void AGTPPlayerController::StartAmbience()
{
	if (Ambience || AmbienceSound.IsNull())
	{
		return;
	}
	USoundBase* Sound = AmbienceSound.LoadSynchronous();
	if (!Sound)
	{
		UE_LOG(LogGTPInput, Warning, TEXT("Ambience sound '%s' not found"), *AmbienceSound.ToString());
		return;
	}
	// 2D, kept across pauses (the menu sits over the street), not tied to the pawn.
	Ambience = UGameplayStatics::CreateSound2D(this, Sound, AmbienceVolume, 1.f, 0.f, nullptr, false, false);
	if (Ambience)
	{
		Ambience->bIsUISound = true;
		Ambience->FadeIn(AmbienceFadeInTime, AmbienceVolume);
	}
}

void AGTPPlayerController::OnEnterExit()
{
	if (!IsMenuOpen())
	{
		VehicleInteraction->HandleEnterExit();
	}
}

void AGTPPlayerController::OnToggleCameraMode()
{
	if (!IsMenuOpen())
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
	ApplyViewPitchLimits();
	StartAmbience();
	if (UGTPUserSettings* User = UGTPUserSettings::Find(this))
	{
		User->OnChanged.AddUObject(this, &AGTPPlayerController::HandleSettingsChanged);
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
	if (UGTPUserSettings* User = UGTPUserSettings::Find(this))
	{
		User->OnChanged.RemoveAll(this);
	}
	HideMenu(TitleMenu);
	HideMenu(PauseMenu);
	HideMenu(SettingsMenu);
	bFullMapOpen = false;
	if (Ambience)
	{
		Ambience->Stop();
		Ambience = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool AGTPPlayerController::IsPauseKey(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right;
}

bool AGTPPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Only reached while gameplay has input focus: open menus consume their own keys.
	if (Params.Event == IE_Pressed)
	{
		const bool bMapKey = Params.Key == EKeys::M || Params.Key == EKeys::Gamepad_DPad_Up;
		if (bFullMapOpen && (bMapKey || IsPauseKey(Params.Key) || Params.Key == EKeys::Gamepad_FaceButton_Right))
		{
			ToggleFullMap();
			return true;
		}
		if (bMapKey && !IsMenuOpen())
		{
			ToggleFullMap();
			return true;
		}
	}
	if (Params.Event == IE_Pressed && IsPauseKey(Params.Key) && !IsMenuOpen())
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

void AGTPPlayerController::ToggleFullMap()
{
	if (!bFullMapOpen)
	{
		// Open only when there is something to show: no pause on an empty screen.
		const UGTPMinimapSubsystem* Minimap = GetWorld() ? GetWorld()->GetSubsystem<UGTPMinimapSubsystem>() : nullptr;
		if (IsMenuOpen() || !GetPawn() || !Minimap || !Minimap->HasData())
		{
			return;
		}
	}
	bFullMapOpen = !bFullMapOpen;
	SetPause(bFullMapOpen);
}

void AGTPPlayerController::OpenSettingsMenu(bool bControlsScreen)
{
	if (SettingsMenu || (!TitleMenu && !PauseMenu))
	{
		return;
	}
	bSettingsFromTitle = TitleMenu != nullptr;
	HideMenu(TitleMenu);
	HideMenu(PauseMenu);
	SettingsMenu = bControlsScreen
		? CreateWidget<UGTPMenuWidget>(this, UGTPControlsMenuWidget::StaticClass())
		: CreateWidget<UGTPMenuWidget>(this, UGTPSettingsMenuWidget::StaticClass());
	ShowMenu(SettingsMenu);
}

void AGTPPlayerController::CloseSettingsMenu()
{
	if (!SettingsMenu)
	{
		return;
	}
	HideMenu(SettingsMenu);
	// Back to the menu that opened the screen, rebuilt so it shows the chosen language.
	if (bSettingsFromTitle)
	{
		TitleMenu = CreateWidget<UGTPTitleMenuWidget>(this, UGTPTitleMenuWidget::StaticClass());
		ShowMenu(TitleMenu);
	}
	else
	{
		PauseMenu = CreateWidget<UGTPPauseMenuWidget>(this, UGTPPauseMenuWidget::StaticClass());
		ShowMenu(PauseMenu);
	}
}

void AGTPPlayerController::HandleSettingsChanged()
{
	// Keyboard layout: put the matching mapping contexts back on the controlled pawn.
	ApplyInputForPawn();
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
	// First entry: level view facing the street. Later entries (pause menu
	// closed) keep the rotation but still swallow the cursor capture delta.
	SettleView(!bViewSettledOnce);
}
