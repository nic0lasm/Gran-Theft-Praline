#include "GTPCombatInput.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GTPWeapon.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

namespace
{
	UInputAction* MakeCombatAction(UObject* Outer, const TCHAR* Name)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name, RF_Transient);
		Action->ValueType = EInputActionValueType::Boolean;
		Action->bConsumeInput = false;
		return Action;
	}
}

void UGTPCombatInput::Bind(APlayerController* Controller)
{
	UEnhancedInputComponent* Enhanced = Controller ? Cast<UEnhancedInputComponent>(Controller->InputComponent) : nullptr;
	if (!Enhanced || Owner.Get() == Controller)
	{
		return; // not ready yet (tried again by the subsystem) or already bound
	}
	Owner = Controller;
	bContextAdded = false;
	if (!Context)
	{
		Aim = MakeCombatAction(this, TEXT("IA_Combat_Aim"));
		Fire = MakeCombatAction(this, TEXT("IA_Combat_Fire"));
		ReloadAction = MakeCombatAction(this, TEXT("IA_Combat_Reload"));
		Holster = MakeCombatAction(this, TEXT("IA_Combat_Holster"));
		Shoulder = MakeCombatAction(this, TEXT("IA_Combat_Shoulder"));

		Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Combat"), RF_Transient);
		Context->MapKey(Aim, EKeys::RightMouseButton);
		Context->MapKey(Aim, EKeys::Gamepad_LeftTriggerAxis);
		Context->MapKey(Fire, EKeys::LeftMouseButton);
		Context->MapKey(Fire, EKeys::Gamepad_RightTriggerAxis);
		Context->MapKey(ReloadAction, EKeys::R);
		Context->MapKey(ReloadAction, EKeys::Gamepad_FaceButton_Left);
		Context->MapKey(Holster, EKeys::One);
		Context->MapKey(Holster, EKeys::Gamepad_DPad_Down);
		Context->MapKey(Shoulder, EKeys::X);
		Context->MapKey(Shoulder, EKeys::Gamepad_DPad_Left);
	}
	{
		Enhanced->BindAction(Aim, ETriggerEvent::Started, this, &UGTPCombatInput::OnAimStarted);
		Enhanced->BindAction(Aim, ETriggerEvent::Completed, this, &UGTPCombatInput::OnAimEnded);
		Enhanced->BindAction(Fire, ETriggerEvent::Started, this, &UGTPCombatInput::OnFireStarted);
		Enhanced->BindAction(Fire, ETriggerEvent::Completed, this, &UGTPCombatInput::OnFireEnded);
		Enhanced->BindAction(ReloadAction, ETriggerEvent::Started, this, &UGTPCombatInput::OnReload);
		Enhanced->BindAction(Holster, ETriggerEvent::Started, this, &UGTPCombatInput::OnHolster);
		Enhanced->BindAction(Shoulder, ETriggerEvent::Started, this, &UGTPCombatInput::OnShoulder);
	}
}

void UGTPCombatInput::UpdateContext(bool bOnFoot)
{
	APlayerController* Controller = Owner.Get();
	const ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !Context)
	{
		return;
	}
	if (bOnFoot && !Input->HasMappingContext(Context))
	{
		Input->AddMappingContext(Context, Priority);
		bContextAdded = true;
	}
	else if (!bOnFoot && Input->HasMappingContext(Context))
	{
		Input->RemoveMappingContext(Context);
		bContextAdded = false;
	}
}

UGTPWeaponComponent* UGTPCombatInput::GetWeapon() const
{
	const APlayerController* Controller = Owner.Get();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
}

void UGTPCombatInput::OnAimStarted()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->SetAiming(true);
	}
}

void UGTPCombatInput::OnAimEnded()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->SetAiming(false);
	}
}

void UGTPCombatInput::OnFireStarted()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->PullTrigger();
	}
}

void UGTPCombatInput::OnFireEnded()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->ReleaseTrigger();
	}
}

void UGTPCombatInput::OnReload()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->Reload();
	}
}

void UGTPCombatInput::OnHolster()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->ToggleDrawn();
	}
}

void UGTPCombatInput::OnShoulder()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->SwapShoulder();
	}
}
