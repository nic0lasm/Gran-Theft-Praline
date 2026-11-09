#include "GTPVehicleInteractionComponent.h"

#include "GTPVehicle.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPVehicleInteraction, Log, All);

APlayerController* UGTPVehicleInteractionComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UGTPVehicleInteractionComponent::HandleEnterExit()
{
	APlayerController* PC = GetPlayerController();
	if (!PC || PC->IsPaused())
	{
		return;
	}
	if (AGTPVehicle* Vehicle = Cast<AGTPVehicle>(PC->GetPawn()))
	{
		Vehicle->RequestExit();
		return;
	}
	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	if (!Character)
	{
		return;
	}
	if (AGTPVehicle* Vehicle = FindBestVehicle())
	{
		Vehicle->BeginEnter(Character, PC);
	}
}

AGTPVehicle* UGTPVehicleInteractionComponent::FindBestVehicle() const
{
	const APlayerController* PC = GetPlayerController();
	const ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		return nullptr;
	}
	const FVector From = Character->GetActorLocation();
	const FVector View = PC->GetControlRotation().Vector().GetSafeNormal2D();

	// A linear scan is fine for the handful of cars of 0.3; traffic (0.7)
	// will move this to a spatial query.
	AGTPVehicle* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (AGTPVehicle* Vehicle : TActorRange<AGTPVehicle>(GetWorld()))
	{
		if (!Vehicle->CanEnter(Character))
		{
			continue;
		}
		const FVector Door = Vehicle->GetDoorLocation(EGTPVehicleSide::Left);
		const FVector DoorRight = Vehicle->GetDoorLocation(EGTPVehicleSide::Right);
		// Either door counts for reach: the character walks round to the driver side.
		const float Distance = FMath::Min(FVector::Dist2D(From, Door), FVector::Dist2D(From, DoorRight));
		if (Distance > MaxDoorDistance)
		{
			continue;
		}
		const FVector ToCar = (Vehicle->GetActorLocation() - From).GetSafeNormal2D();
		const float Misalignment = 1.f - FVector::DotProduct(View, ToCar);
		const float Score = Distance + Misalignment * ViewAlignmentWeight;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Vehicle;
		}
	}
	UE_LOG(LogGTPVehicleInteraction, Verbose, TEXT("Best vehicle: %s"), Best ? *Best->GetName() : TEXT("none"));
	return Best;
}
