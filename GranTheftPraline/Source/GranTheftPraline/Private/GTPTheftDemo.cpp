#include "GTPTheftDemo.h"

#include "GTPLog.h"
#include "GTPTraffic.h"
#include "GTPVehicle.h"
#include "GTPVehicleInteractionComponent.h"
#include "GTPWanted.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPTheftDemo, Log, All);

namespace
{
	AGTPTrafficNetwork* FindNetwork(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		TActorIterator<AGTPTrafficNetwork> It(World);
		return It ? *It : nullptr;
	}
}

AGTPVehicle* UGTPTheftDemoLibrary::PlaceAlarmCar(UObject* WorldContext, FVector Near, float RadiusCm)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	AGTPTrafficNetwork* Network = FindNetwork(World);
	AGTPVehicle* Car = Network ? Network->SpawnStreetParkedNear(Near, RadiusCm, true, true) : nullptr;
	GTP_TRACE(LogGTPTheftDemo, TEXT("theft demo: alarm car %s (%s)"), *GetNameSafe(Car),
		Car ? *Car->GetActorLocation().ToCompactString() : (Network ? TEXT("no free kerb spot") : TEXT("no traffic network")));
	return Car;
}

AGTPVehicle* UGTPTheftDemoLibrary::FindTrafficCar(UObject* WorldContext, FVector Near, float MaxDistanceCm)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	AGTPVehicle* Best = nullptr;
	double BestDistance = MaxDistanceCm;
	for (AGTPVehicle* Car : TActorRange<AGTPVehicle>(World))
	{
		if (!Car->HasAIDriver() || Car->IsWrecked() || Car->ActorHasTag(GTPPolice::PoliceTag))
		{
			continue;
		}
		const double Distance = FVector::Dist2D(Car->GetActorLocation(), Near);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Car;
		}
	}
	GTP_TRACE(LogGTPTheftDemo, TEXT("theft demo: traffic car %s (%.0f m, %.0f km/h)"), *GetNameSafe(Best), BestDistance / 100.0,
		Best ? Best->GetSpeedKmh() : 0.f);
	return Best;
}

void UGTPTheftDemoLibrary::StopTrafficCar(AGTPVehicle* Car, float Seconds)
{
	if (Car)
	{
		Car->RequestAIStop(Seconds);
		GTP_TRACE(LogGTPTheftDemo, TEXT("theft demo: %s asked to stop (%.0f km/h)"), *Car->GetName(), Car->GetSpeedKmh());
	}
}

bool UGTPTheftDemoLibrary::IsStopped(const AGTPVehicle* Car)
{
	return Car && FMath::Abs(Car->GetSpeedKmh()) < 4.f;
}

bool UGTPTheftDemoLibrary::PlaceAtDriverDoor(ACharacter* Character, AGTPVehicle* Car)
{
	if (!Character || !Car)
	{
		return false;
	}
	// A metre out from the door, on the driver's side, facing the car.
	const FVector Door = Car->GetDoorLocation(EGTPVehicleSide::Left);
	FVector Out = (Door - Car->GetActorLocation()).GetSafeNormal2D();
	if (Out.IsNearlyZero())
	{
		Out = -Car->GetActorRightVector();
	}
	const FVector Spot = Door + Out * 100.f + FVector(0.f, 0.f, 20.f);
	const bool bMoved = Character->TeleportTo(Spot, FRotator(0.f, (-Out).Rotation().Yaw, 0.f));
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(FRotator(-10.f, (-Out).Rotation().Yaw, 0.f));
	}
	GTP_TRACE(LogGTPTheftDemo, TEXT("theft demo: player at the door of %s (%s)"), *Car->GetName(), bMoved ? TEXT("placed") : TEXT("blocked"));
	return bMoved;
}

bool UGTPTheftDemoLibrary::PressEnter(UObject* WorldContext)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UGTPVehicleInteractionComponent* Interaction = PC ? PC->FindComponentByClass<UGTPVehicleInteractionComponent>() : nullptr;
	if (!Interaction)
	{
		GTP_TRACE(LogGTPTheftDemo, TEXT("theft demo: no enter key component on the player controller"));
		return false;
	}
	const AGTPVehicle* Best = Interaction->FindBestVehicle();
	GTP_TRACE(LogGTPTheftDemo, TEXT("theft demo: enter key (car in reach: %s)"), *GetNameSafe(Best));
	Interaction->HandleEnterExit();
	return true;
}

bool UGTPTheftDemoLibrary::IsDriving(const ACharacter* Character, const AGTPVehicle* Car)
{
	return Character && Car && Car->GetDriver() == Character && Car->GetVehicleState() == EGTPVehicleState::Driven;
}
