#include "GTPVehicleInteractionComponent.h"
#include "GTPMinimap.h"

#include "GTPVehicle.h"
#include "GTPWanted.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
		// The car the player took stays marked on the maps after they get out.
		if (UGTPMinimapSubsystem* Minimap = GetWorld() ? GetWorld()->GetSubsystem<UGTPMinimapSubsystem>() : nullptr)
		{
			Minimap->AddBlip(Vehicle, FLinearColor(0.35f, 0.75f, 1.f), false);
		}
		return;
	}
	// A car of the traffic rolling past: its driver stops, then the player pulls them out (GTA).
	if (AGTPVehicle* Rolling = FindRollingCar())
	{
		Rolling->RequestAIStop(3.f);
		PendingCar = Rolling;
		PendingLeft = 3.f;
		GetWorld()->GetTimerManager().SetTimer(PendingTimer, this, &UGTPVehicleInteractionComponent::TryPendingEntry, 0.1f, true);
		UE_LOG(LogGTPVehicleInteraction, Log, TEXT("Flagging down %s at %.0f km/h"), *Rolling->GetName(), Rolling->GetSpeedKmh());
	}
}

AGTPVehicle* UGTPVehicleInteractionComponent::FindRollingCar() const
{
	const APlayerController* PC = GetPlayerController();
	const ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		return nullptr;
	}
	const FVector From = Character->GetActorLocation();
	AGTPVehicle* Best = nullptr;
	float BestDistance = FlagDownDistance;
	for (AGTPVehicle* Vehicle : TActorRange<AGTPVehicle>(GetWorld()))
	{
		// Police cars do not stop for the player.
		if (!Vehicle->HasAIDriver() || Vehicle->IsWrecked() || Vehicle->ActorHasTag(GTPPolice::PoliceTag)
			|| FMath::Abs(Vehicle->GetSpeedKmh()) > FlagDownMaxSpeedKmh)
		{
			continue;
		}
		const float Distance = FMath::Min(FVector::Dist2D(From, Vehicle->GetDoorLocation(EGTPVehicleSide::Left)),
			FVector::Dist2D(From, Vehicle->GetDoorLocation(EGTPVehicleSide::Right)));
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Vehicle;
		}
	}
	return Best;
}

void UGTPVehicleInteractionComponent::TryPendingEntry()
{
	PendingLeft -= 0.1f;
	APlayerController* PC = GetPlayerController();
	ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	AGTPVehicle* Car = PendingCar.Get();
	if (!Car || !Character || PendingLeft <= 0.f)
	{
		GetWorld()->GetTimerManager().ClearTimer(PendingTimer);
		PendingCar.Reset();
		return;
	}
	// Close enough once it has stopped: the car may have rolled a few metres on.
	const float Distance = FMath::Min(FVector::Dist2D(Character->GetActorLocation(), Car->GetDoorLocation(EGTPVehicleSide::Left)),
		FVector::Dist2D(Character->GetActorLocation(), Car->GetDoorLocation(EGTPVehicleSide::Right)));
	if (Car->CanEnter(Character) && Distance <= FlagDownDistance + 300.f)
	{
		GetWorld()->GetTimerManager().ClearTimer(PendingTimer);
		PendingCar.Reset();
		if (Car->BeginEnter(Character, PC))
		{
			if (UGTPMinimapSubsystem* Minimap = GetWorld()->GetSubsystem<UGTPMinimapSubsystem>())
			{
				Minimap->AddBlip(Car, FLinearColor(0.35f, 0.75f, 1.f), false);
			}
		}
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
