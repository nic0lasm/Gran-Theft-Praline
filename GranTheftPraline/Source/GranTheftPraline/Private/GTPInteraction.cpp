#include "GTPInteraction.h"

#include "GTPPlayerController.h"
#include "GTPVehicle.h"
#include "GTPVehicleInteractionComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "GTPInteraction"

DEFINE_LOG_CATEGORY_STATIC(LogGTPInteraction, Log, All);

// --- Interactable --------------------------------------------------------

bool UGTPInteractableComponent::CanInteract(const APawn* Pawn) const
{
	return bInteractionEnabled && Pawn && !Prompt.IsEmpty()
		&& FVector::DistSquared(Pawn->GetActorLocation(), GetComponentLocation()) <= FMath::Square(Radius);
}

void UGTPInteractableComponent::Interact(APawn* Pawn)
{
	UE_LOG(LogGTPInteraction, Log, TEXT("%s used %s"), *GetNameSafe(Pawn), *GetNameSafe(GetOwner()));
	OnInteracted.Broadcast(Pawn);
}

void UGTPInteractableComponent::OnRegister()
{
	Super::OnRegister();
	if (UWorld* World = GetWorld())
	{
		if (UGTPInteractionSubsystem* Subsystem = World->GetSubsystem<UGTPInteractionSubsystem>())
		{
			Subsystem->Register(this);
		}
	}
}

void UGTPInteractableComponent::OnUnregister()
{
	if (UWorld* World = GetWorld())
	{
		if (UGTPInteractionSubsystem* Subsystem = World->GetSubsystem<UGTPInteractionSubsystem>())
		{
			Subsystem->Unregister(this);
		}
	}
	Super::OnUnregister();
}

// --- Registry ------------------------------------------------------------

void UGTPInteractionSubsystem::Register(UGTPInteractableComponent* Component)
{
	Interactables.AddUnique(Component);
}

void UGTPInteractionSubsystem::Unregister(UGTPInteractableComponent* Component)
{
	Interactables.RemoveAll([Component](const TWeakObjectPtr<UGTPInteractableComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Component;
	});
}

// --- Player side ---------------------------------------------------------

UGTPInteractionComponent::UGTPInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Ten checks a second are plenty for a prompt and cost nothing.
	PrimaryComponentTick.TickInterval = 0.1f;
}

APlayerController* UGTPInteractionComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

UGTPInteractableComponent* UGTPInteractionComponent::FindFocus(const APawn* Pawn) const
{
	const APlayerController* PC = GetPlayerController();
	const UWorld* World = GetWorld();
	const UGTPInteractionSubsystem* Subsystem = World ? World->GetSubsystem<UGTPInteractionSubsystem>() : nullptr;
	if (!PC || !Pawn || !Subsystem)
	{
		return nullptr;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	const FVector Eyes = Pawn->GetPawnViewLocation();

	UGTPInteractableComponent* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<UGTPInteractableComponent>& Entry : Subsystem->GetInteractables())
	{
		UGTPInteractableComponent* Candidate = Entry.Get();
		if (!Candidate || !Candidate->CanInteract(Pawn))
		{
			continue;
		}
		const FVector Target = Candidate->GetComponentLocation();
		const float Alignment = FVector::DotProduct(ViewDirection, (Target - ViewLocation).GetSafeNormal());
		if (Alignment < MinViewAlignment)
		{
			continue;
		}
		// Nothing solid between the character and the target (a wall, a closed shutter).
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPInteraction), false, Pawn);
		Params.AddIgnoredActor(Candidate->GetOwner());
		if (World->LineTraceTestByChannel(Eyes, Target, ECC_Visibility, Params))
		{
			continue;
		}
		const float Score = FVector::Dist(Pawn->GetActorLocation(), Target) * (2.f - Alignment);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

void UGTPInteractionComponent::RefreshPrompts(const APawn* Pawn)
{
	Prompts.Reset();
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character)
	{
		Focus = nullptr;
		return;
	}
	Focus = FindFocus(Pawn);
	if (const UGTPInteractableComponent* Target = Focus.Get())
	{
		Prompts.Emplace(LOCTEXT("InteractKey", "E"), Target->Prompt);
	}
	const AGTPPlayerController* PC = Cast<AGTPPlayerController>(GetPlayerController());
	const UGTPVehicleInteractionComponent* Vehicles = PC ? PC->GetVehicleInteraction() : nullptr;
	if (Vehicles && Vehicles->FindBestVehicle())
	{
		Prompts.Emplace(LOCTEXT("EnterVehicleKey", "F"), LOCTEXT("EnterVehicle", "Monter"));
	}
}

void UGTPInteractionComponent::HandleInteract()
{
	APlayerController* PC = GetPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || PC->IsPaused())
	{
		return;
	}
	// Re-check now: the prompt may be up to a tenth of a second old.
	UGTPInteractableComponent* Target = FindFocus(Pawn);
	if (!Target)
	{
		return;
	}
	if (!Target->Message.IsEmpty())
	{
		ShowMessage(Target->Message);
	}
	Target->Interact(Pawn);
}

void UGTPInteractionComponent::ShowMessage(const FText& Text, float Duration)
{
	Message = Text;
	if (const UWorld* World = GetWorld())
	{
		MessageEndTime = World->GetTimeSeconds() + (Duration > 0.f ? Duration : MessageDuration);
	}
}

FText UGTPInteractionComponent::GetMessage() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < MessageEndTime ? Message : FText::GetEmpty();
}

void UGTPInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const APlayerController* PC = GetPlayerController();
	if (PC && PC->IsLocalController())
	{
		RefreshPrompts(PC->GetPawn());
	}
}

#undef LOCTEXT_NAMESPACE
