#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPInteraction.generated.h"

class AGTPVehicle;
class APawn;
class APlayerController;
class UGTPInteractableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGTPInteractedSignature, APawn*, InteractingPawn);

/** One line of the on-screen help: the key to press and what it does. */
USTRUCT(BlueprintType)
struct FGTPInteractionPrompt
{
	GENERATED_BODY()

	FGTPInteractionPrompt() = default;
	FGTPInteractionPrompt(const FText& InKey, const FText& InAction) : Key(InKey), Action(InAction) {}

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText Key;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText Action;
};

/**
 * Makes an actor usable with the interaction key (E / gamepad D-pad right):
 * a door, a counter, a vending machine. Add it to the actor, place it where
 * the hand goes, set the prompt, and react to OnInteracted in Blueprint or
 * C++. Message, when set, is shown as a subtitle on use (placeholder
 * feedback until the action exists, for example a shop interior).
 */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPInteractableComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	/** Verb shown next to the key, for example "Entrer" or "Acheter". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Prompt;

	/** Optional subtitle shown for a few seconds when used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Message;

	/** Farthest the character may stand from this component, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float Radius = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bInteractionEnabled = true;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FGTPInteractedSignature OnInteracted;

	virtual bool CanInteract(const APawn* Pawn) const;
	virtual void Interact(APawn* Pawn);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
};

/** Registry of the interactable components of a world, so the player does not scan every actor. */
UCLASS()
class GRANTHEFTPRALINE_API UGTPInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Register(UGTPInteractableComponent* Component);
	void Unregister(UGTPInteractableComponent* Component);
	const TArray<TWeakObjectPtr<UGTPInteractableComponent>>& GetInteractables() const { return Interactables; }

private:
	TArray<TWeakObjectPtr<UGTPInteractableComponent>> Interactables;
};

/**
 * Contextual interaction for the local player, owned by the player
 * controller: keeps track of what the character could use right now (an
 * interactable in reach and in view, a car to enter), feeds the HUD prompts
 * and runs the interaction key. Vehicles keep their own key (F, Y on the
 * gamepad); everything else uses E (D-pad right).
 */
UCLASS(Config = Game, ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPInteractionComponent();

	/** Bound to the interaction action. */
	void HandleInteract();

	/** Lines for the on-screen help, most relevant first. */
	const TArray<FGTPInteractionPrompt>& GetPrompts() const { return Prompts; }

	/** Subtitle to show now (empty when none). */
	FText GetMessage() const;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ShowMessage(const FText& Text, float Duration = -1.f);

	UGTPInteractableComponent* GetFocus() const { return Focus.Get(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Minimum cosine between the view and the direction to the target (0.35 is about 70 degrees). */
	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float MinViewAlignment = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category = "Interaction")
	float MessageDuration = 4.f;

private:
	APlayerController* GetPlayerController() const;
	UGTPInteractableComponent* FindFocus(const APawn* Pawn) const;
	void RefreshPrompts(const APawn* Pawn);

	TWeakObjectPtr<UGTPInteractableComponent> Focus;
	TArray<FGTPInteractionPrompt> Prompts;
	FText Message;
	double MessageEndTime = 0.0;
};
