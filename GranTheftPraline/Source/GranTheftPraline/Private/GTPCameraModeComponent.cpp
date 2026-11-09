#include "GTPCameraModeComponent.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPCameraMode, Log, All);

namespace
{
	USkeletalMeshComponent* FindBodyMesh(APawn* Pawn)
	{
		if (const ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			return Character->GetMesh();
		}
		return Pawn ? Pawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	}

	/** Blueprint struct members carry a generated suffix: match on the readable prefix. */
	FProperty* FindPropertyByPrefix(const UStruct* Struct, const TCHAR* Prefix)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (It->GetName().StartsWith(Prefix))
			{
				return *It;
			}
		}
		return nullptr;
	}
}

UGTPCameraModeComponent::UGTPCameraModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

APlayerController* UGTPCameraModeComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UGTPCameraModeComponent::ToggleCameraMode()
{
	SetFirstPerson(!bFirstPerson);
}

void UGTPCameraModeComponent::HandlePawnChanged(APawn* NewPawn)
{
	if (AppliedPawn.IsValid() && AppliedPawn.Get() != NewPawn)
	{
		ApplyToPawn(AppliedPawn.Get(), false);
	}
	AppliedPawn = nullptr;
	if (bFirstPerson && NewPawn)
	{
		SetFirstPerson(true, true);
	}
}

void UGTPCameraModeComponent::SetFirstPerson(bool bEnable, bool bInstant)
{
	APlayerController* PC = GetPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	bFirstPerson = bEnable;
	if (!PC || !Pawn)
	{
		// Kept for the next pawn.
		return;
	}

	const float Blend = bInstant ? 0.f : BlendTime;
	if (bEnable)
	{
		if (!FirstPersonCamera)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transient;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			FirstPersonCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
			UCameraComponent* Camera = FirstPersonCamera->GetCameraComponent();
			Camera->SetConstraintAspectRatio(false);
			Camera->bOverrideAspectRatioAxisConstraint = false;
		}
		FirstPersonCamera->GetCameraComponent()->SetFieldOfView(FirstPersonFOV);
		ApplyToPawn(Pawn, true);
		UpdateFirstPersonCamera();
		SetComponentTickEnabled(true);
		PC->SetViewTargetWithBlend(FirstPersonCamera, Blend, VTBlend_Cubic);
	}
	else
	{
		ApplyToPawn(Pawn, false);
		PC->SetViewTargetWithBlend(Pawn, Blend, VTBlend_Cubic);
		// Keep the first person camera following the head while the blend runs.
		if (Blend <= 0.f)
		{
			SetComponentTickEnabled(false);
		}
	}
	UE_LOG(LogGTPCameraMode, Log, TEXT("Camera mode: %s"), bEnable ? TEXT("first person") : TEXT("third person"));
}

void UGTPCameraModeComponent::ApplyToPawn(APawn* Pawn, bool bEnable)
{
	if (!Pawn)
	{
		return;
	}
	if (USkeletalMeshComponent* Mesh = FindBodyMesh(Pawn))
	{
		if (bEnable)
		{
			Mesh->HideBoneByName(HeadBoneName, PBO_None);
		}
		else
		{
			Mesh->UnHideBoneByName(HeadBoneName);
		}
	}

	if (bEnable && AppliedPawn.Get() != Pawn)
	{
		SetWantsToStrafe(Pawn, true, bPreviousWantsToStrafe);
		AppliedPawn = Pawn;
	}
	else if (!bEnable && AppliedPawn.Get() == Pawn)
	{
		bool bIgnored = false;
		SetWantsToStrafe(Pawn, bPreviousWantsToStrafe, bIgnored);
		AppliedPawn = nullptr;
	}
}

void UGTPCameraModeComponent::SetWantsToStrafe(APawn* Pawn, bool bWants, bool& bOutPrevious)
{
	// Game Animation Sample character: CharacterInputState.WantsToStrafe (Blueprint struct).
	FStructProperty* InputState = CastField<FStructProperty>(Pawn->GetClass()->FindPropertyByName(TEXT("CharacterInputState")));
	if (!InputState)
	{
		return;
	}
	FBoolProperty* Strafe = CastField<FBoolProperty>(FindPropertyByPrefix(InputState->Struct, TEXT("WantsToStrafe")));
	if (!Strafe)
	{
		UE_LOG(LogGTPCameraMode, Warning, TEXT("CharacterInputState has no WantsToStrafe member"));
		return;
	}
	void* StructData = InputState->ContainerPtrToValuePtr<void>(Pawn);
	bOutPrevious = Strafe->GetPropertyValue_InContainer(StructData);
	Strafe->SetPropertyValue_InContainer(StructData, bWants);
}

void UGTPCameraModeComponent::UpdateFirstPersonCamera()
{
	APlayerController* PC = GetPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	USkeletalMeshComponent* Mesh = FindBodyMesh(Pawn);
	if (!FirstPersonCamera || !Mesh)
	{
		return;
	}
	const FRotator View = PC->GetControlRotation();
	const FVector Head = Mesh->GetSocketLocation(HeadBoneName);
	FirstPersonCamera->SetActorLocationAndRotation(Head + View.RotateVector(EyeOffset), View);
}

void UGTPCameraModeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFirstPersonCamera();

	if (!bFirstPerson)
	{
		const APlayerController* PC = GetPlayerController();
		const bool bBlending = PC && PC->PlayerCameraManager && PC->PlayerCameraManager->PendingViewTarget.Target != nullptr;
		if (!bBlending)
		{
			SetComponentTickEnabled(false);
		}
	}
}

void UGTPCameraModeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FirstPersonCamera)
	{
		FirstPersonCamera->Destroy();
		FirstPersonCamera = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
