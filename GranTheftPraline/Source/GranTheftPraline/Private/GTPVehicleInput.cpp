#include "GTPVehicleInput.h"

#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name, RF_Transient);
		Action->ValueType = Type;
		return Action;
	}

	void Negate(UObject* Outer, FEnhancedActionKeyMapping& Mapping, bool bX = true, bool bY = true)
	{
		UInputModifierNegate* Modifier = NewObject<UInputModifierNegate>(Outer);
		Modifier->bX = bX;
		Modifier->bY = bY;
		Modifier->bZ = false;
		Mapping.Modifiers.Add(Modifier);
	}

	void DeadZone(UObject* Outer, FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(Outer));
	}
}

void UGTPVehicleInput::Build(bool bQwerty)
{
	if (!EnterExit)
	{
		BuildActions();
		BuildGamepadAndInteraction();
	}
	if (!DrivingKeyboard || bQwerty != bBuiltQwerty)
	{
		BuildKeyboard(bQwerty);
	}
}

void UGTPVehicleInput::BuildActions()
{
	EnterExit = MakeAction(this, TEXT("IA_Vehicle_EnterExit"), EInputActionValueType::Boolean);
	Throttle = MakeAction(this, TEXT("IA_Vehicle_Throttle"), EInputActionValueType::Axis1D);
	Brake = MakeAction(this, TEXT("IA_Vehicle_Brake"), EInputActionValueType::Axis1D);
	Steer = MakeAction(this, TEXT("IA_Vehicle_Steer"), EInputActionValueType::Axis1D);
	Handbrake = MakeAction(this, TEXT("IA_Vehicle_Handbrake"), EInputActionValueType::Boolean);
	Look = MakeAction(this, TEXT("IA_Vehicle_Look"), EInputActionValueType::Axis2D);
	LookBehind = MakeAction(this, TEXT("IA_Vehicle_LookBehind"), EInputActionValueType::Boolean);
	ToggleView = MakeAction(this, TEXT("IA_Vehicle_ToggleView"), EInputActionValueType::Boolean);
}

void UGTPVehicleInput::BuildKeyboard(bool bQwerty)
{
	bBuiltQwerty = bQwerty;
	DrivingKeyboard = NewObject<UInputMappingContext>(this, bQwerty ? TEXT("IMC_Vehicle_KBM_QWERTY") : TEXT("IMC_Vehicle_KBM_AZERTY"), RF_Transient);
	UInputMappingContext* Context = DrivingKeyboard;

	Context->MapKey(Throttle, bQwerty ? EKeys::W : EKeys::Z);
	Context->MapKey(Throttle, EKeys::Up);
	Context->MapKey(Brake, EKeys::S);
	Context->MapKey(Brake, EKeys::Down);

	Context->MapKey(Steer, EKeys::D);
	Context->MapKey(Steer, EKeys::Right);
	Negate(Context, Context->MapKey(Steer, bQwerty ? EKeys::A : EKeys::Q));
	Negate(Context, Context->MapKey(Steer, EKeys::Left));

	Context->MapKey(Handbrake, EKeys::SpaceBar);
	// Mouse up gives +Y; the pitch input scale of the project inverts it back.
	Negate(Context, Context->MapKey(Look, EKeys::Mouse2D), false, true);
	Context->MapKey(LookBehind, EKeys::C);
	Context->MapKey(ToggleView, EKeys::V);
}

void UGTPVehicleInput::BuildGamepadAndInteraction()
{
	Interaction = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle_Interaction"), RF_Transient);
	Interaction->MapKey(EnterExit, EKeys::F);
	Interaction->MapKey(EnterExit, EKeys::Gamepad_FaceButton_Top);

	DrivingGamepad = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle_Gamepad"), RF_Transient);
	UInputMappingContext* Context = DrivingGamepad;
	Context->MapKey(Throttle, EKeys::Gamepad_RightTriggerAxis);
	Context->MapKey(Brake, EKeys::Gamepad_LeftTriggerAxis);
	DeadZone(Context, Context->MapKey(Steer, EKeys::Gamepad_LeftX));
	Context->MapKey(Handbrake, EKeys::Gamepad_RightShoulder);
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Look, EKeys::Gamepad_Right2D);
		DeadZone(Context, Mapping);
		Negate(Context, Mapping, false, true);
	}
	Context->MapKey(LookBehind, EKeys::Gamepad_RightThumbstick);
	Context->MapKey(ToggleView, EKeys::Gamepad_Special_Left);
}
