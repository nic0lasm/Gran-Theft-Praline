#include "GTPCombatInput.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GTPCombat.h"
#include "GTPCombatMelee.h"
#include "GTPWeapon.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

namespace
{
	UInputAction* MakeCombatAction(UObject* Outer, const TCHAR* Name, bool bConsume = false)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name, RF_Transient);
		Action->ValueType = EInputActionValueType::Boolean;
		Action->bConsumeInput = bConsume;
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
	CurrentMode = EMode::None;
	if (!Context)
	{
		Aim = MakeCombatAction(this, TEXT("IA_Combat_Aim"));
		Fire = MakeCombatAction(this, TEXT("IA_Combat_Fire"));
		ReloadAction = MakeCombatAction(this, TEXT("IA_Combat_Reload"));
		Holster = MakeCombatAction(this, TEXT("IA_Combat_Holster"));
		Shoulder = MakeCombatAction(this, TEXT("IA_Combat_Shoulder"));
		DodgeAction = MakeCombatAction(this, TEXT("IA_Combat_Dodge"));
		NextWeapon = MakeCombatAction(this, TEXT("IA_Combat_NextWeapon"));
		PrevWeapon = MakeCombatAction(this, TEXT("IA_Combat_PrevWeapon"));
		AimPad = MakeCombatAction(this, TEXT("IA_Combat_AimPad"));
		DriveByFire = MakeCombatAction(this, TEXT("IA_Combat_DriveByFire"), true);

		Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Combat"), RF_Transient);
		Context->MapKey(Aim, EKeys::RightMouseButton);
		Context->MapKey(AimPad, EKeys::Gamepad_LeftTriggerAxis);
		Context->MapKey(Fire, EKeys::LeftMouseButton);
		Context->MapKey(Fire, EKeys::Gamepad_RightTriggerAxis);
		Context->MapKey(ReloadAction, EKeys::R);
		Context->MapKey(ReloadAction, EKeys::Gamepad_FaceButton_Left);
		Context->MapKey(Holster, EKeys::One);
		Context->MapKey(Holster, EKeys::Gamepad_DPad_Down);
		Context->MapKey(Shoulder, EKeys::X);
		Context->MapKey(Shoulder, EKeys::Gamepad_DPad_Left);
		Context->MapKey(DodgeAction, EKeys::LeftAlt);
		Context->MapKey(DodgeAction, EKeys::Gamepad_FaceButton_Right);
		Context->MapKey(NextWeapon, EKeys::MouseScrollDown);
		Context->MapKey(NextWeapon, EKeys::Two);
		Context->MapKey(NextWeapon, EKeys::Gamepad_LeftShoulder);
		Context->MapKey(PrevWeapon, EKeys::MouseScrollUp);

		// At the wheel: the triggers drive, so the gamepad aims with the left bumper.
		VehicleContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Combat_Vehicle"), RF_Transient);
		VehicleContext->MapKey(Aim, EKeys::RightMouseButton);
		VehicleContext->MapKey(AimPad, EKeys::Gamepad_LeftShoulder);
		VehicleContext->MapKey(Fire, EKeys::LeftMouseButton);
		VehicleContext->MapKey(ReloadAction, EKeys::R);
		VehicleContext->MapKey(ReloadAction, EKeys::Gamepad_FaceButton_Left);
		VehicleContext->MapKey(NextWeapon, EKeys::MouseScrollDown);
		VehicleContext->MapKey(PrevWeapon, EKeys::MouseScrollUp);

		DriveByFireContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Combat_DriveByFire"), RF_Transient);
		DriveByFireContext->MapKey(DriveByFire, EKeys::Gamepad_RightShoulder);
	}
	{
		Enhanced->BindAction(Aim, ETriggerEvent::Started, this, &UGTPCombatInput::OnAimStarted);
		Enhanced->BindAction(Aim, ETriggerEvent::Completed, this, &UGTPCombatInput::OnAimEnded);
		Enhanced->BindAction(Fire, ETriggerEvent::Started, this, &UGTPCombatInput::OnFireStarted);
		Enhanced->BindAction(Fire, ETriggerEvent::Completed, this, &UGTPCombatInput::OnFireEnded);
		Enhanced->BindAction(ReloadAction, ETriggerEvent::Started, this, &UGTPCombatInput::OnReload);
		Enhanced->BindAction(Holster, ETriggerEvent::Started, this, &UGTPCombatInput::OnHolster);
		Enhanced->BindAction(Shoulder, ETriggerEvent::Started, this, &UGTPCombatInput::OnShoulder);
		Enhanced->BindAction(DodgeAction, ETriggerEvent::Started, this, &UGTPCombatInput::OnDodge);
		Enhanced->BindAction(NextWeapon, ETriggerEvent::Started, this, &UGTPCombatInput::OnNextWeapon);
		Enhanced->BindAction(PrevWeapon, ETriggerEvent::Started, this, &UGTPCombatInput::OnPrevWeapon);
		Enhanced->BindAction(AimPad, ETriggerEvent::Started, this, &UGTPCombatInput::OnAimPadStarted);
		Enhanced->BindAction(AimPad, ETriggerEvent::Completed, this, &UGTPCombatInput::OnAimEnded);
		Enhanced->BindAction(DriveByFire, ETriggerEvent::Started, this, &UGTPCombatInput::OnFireStarted);
		Enhanced->BindAction(DriveByFire, ETriggerEvent::Completed, this, &UGTPCombatInput::OnFireEnded);
	}
}

void UGTPCombatInput::UpdateContext(EMode Mode)
{
	APlayerController* Controller = Owner.Get();
	const ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !Context)
	{
		return;
	}
	// Checked every time (another system may have cleared the contexts on possession).
	CurrentMode = Mode;
	auto Set = [Input](UInputMappingContext* Mapping, bool bOn, int32 InPriority)
	{
		if (bOn && !Input->HasMappingContext(Mapping))
		{
			Input->AddMappingContext(Mapping, InPriority);
		}
		else if (!bOn && Input->HasMappingContext(Mapping))
		{
			Input->RemoveMappingContext(Mapping);
		}
	};
	Set(Context, Mode == EMode::OnFoot, Priority);
	Set(VehicleContext, Mode == EMode::Driving, Priority);
	if (Mode != EMode::Driving)
	{
		SetDriveByFireContext(false);
	}
}

void UGTPCombatInput::SetDriveByFireContext(bool bOn)
{
	APlayerController* Controller = Owner.Get();
	const ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !DriveByFireContext)
	{
		return;
	}
	if (bOn && !Input->HasMappingContext(DriveByFireContext))
	{
		Input->AddMappingContext(DriveByFireContext, DriveByFirePriority);
	}
	else if (!bOn && Input->HasMappingContext(DriveByFireContext))
	{
		Input->RemoveMappingContext(DriveByFireContext);
	}
}

UGTPWeaponComponent* UGTPCombatInput::GetWeapon() const
{
	const APlayerController* Controller = Owner.Get();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return UGTPWeaponComponent::FindHeld(Pawn); // at the wheel, the seated driver's
}

UGTPCombatMeleeComponent* UGTPCombatInput::GetFists() const
{
	const APlayerController* Controller = Owner.Get();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UGTPCombatMeleeComponent>() : nullptr;
}

void UGTPCombatInput::OnAimStarted()
{
	UGTPWeaponComponent* Weapon = GetWeapon();
	if (Weapon && Weapon->IsDriveBy())
	{
		// GTA-like: aiming at the wheel takes the gun out at once.
		Weapon->SetDrawn(true);
		Weapon->SetAiming(true);
		SetDriveByFireContext(true);
		return;
	}
	if (Weapon && Weapon->IsDrawn())
	{
		Weapon->SetAiming(true);
	}
	else if (UGTPCombatMeleeComponent* Fists = GetFists())
	{
		Fists->SetGuard(true);
	}
}

void UGTPCombatInput::OnAimEnded()
{
	SetDriveByFireContext(false);
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->SetAiming(false);
		if (Weapon->IsDriveBy())
		{
			Weapon->ReleaseTrigger();
			Weapon->SetDrawn(false); // both hands back on the wheel
		}
	}
	if (UGTPCombatMeleeComponent* Fists = GetFists())
	{
		Fists->SetGuard(false);
	}
}

void UGTPCombatInput::OnFireStarted()
{
	UGTPWeaponComponent* Weapon = GetWeapon();
	if (Weapon && Weapon->IsDrawn())
	{
		Weapon->PullTrigger();
	}
	else if (UGTPCombatMeleeComponent* Fists = GetFists())
	{
		Fists->Attack();
	}
}

void UGTPCombatInput::OnDodge()
{
	UGTPCombatMeleeComponent* Fists = GetFists();
	const APlayerController* Controller = Owner.Get();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Fists || !Pawn)
	{
		return;
	}
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->CancelReload();
	}
	Fists->Dodge(Pawn->GetLastMovementInputVector());
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

void UGTPCombatInput::OnAimPadStarted()
{
	OnAimStarted();
	UGTPWeaponComponent* Weapon = GetWeapon();
	if (Weapon && Weapon->IsAiming())
	{
		if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(Owner.Get()))
		{
			Combat->SnapPlayerAim(Weapon);
		}
	}
}

void UGTPCombatInput::OnNextWeapon()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->CycleWeapon(1);
	}
}

void UGTPCombatInput::OnPrevWeapon()
{
	if (UGTPWeaponComponent* Weapon = GetWeapon())
	{
		Weapon->CycleWeapon(-1);
	}
}
