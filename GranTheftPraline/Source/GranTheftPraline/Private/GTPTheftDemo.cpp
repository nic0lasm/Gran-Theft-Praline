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
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
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
	if (!Character || !Car || !Character->GetCapsuleComponent())
	{
		return false;
	}
	UWorld* World = Car->GetWorld();
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTheftDemoDoor), false, Character);
	Params.AddIgnoredActor(Car);
	// The driver door first, then the passenger door (the character walks round), a little
	// farther out each time. The capsule stands on the ground found under the spot: placed at the
	// car's origin height it sank into the road and the teleport was refused (package, 13/11).
	FString Causes;
	for (const EGTPVehicleSide Side : {EGTPVehicleSide::Left, EGTPVehicleSide::Right})
	{
		const FVector Door = Car->GetDoorLocation(Side);
		FVector Out = (Door - Car->GetActorLocation()).GetSafeNormal2D();
		if (Out.IsNearlyZero())
		{
			Out = Side == EGTPVehicleSide::Left ? -Car->GetActorRightVector() : Car->GetActorRightVector();
		}
		for (const float Distance : {70.f, 120.f, 180.f})
		{
			FVector Spot = Door + Out * Distance;
			FHitResult Ground;
			if (!World->LineTraceSingleByChannel(Ground, FVector(Spot.X, Spot.Y, Car->GetActorLocation().Z + 300.0),
					FVector(Spot.X, Spot.Y, Car->GetActorLocation().Z - 500.0), ECC_Visibility, Params))
			{
				Causes += FString::Printf(TEXT(" %s+%.0f: no ground;"), Side == EGTPVehicleSide::Left ? TEXT("left") : TEXT("right"), Distance);
				continue;
			}
			Spot.Z = Ground.ImpactPoint.Z + HalfHeight + 3.0;
			TArray<FOverlapResult> Blocking;
			World->OverlapMultiByChannel(Blocking, Spot, FQuat::Identity, ECC_Pawn, Shape, Params);
			const FOverlapResult* Blocker = Blocking.FindByPredicate([](const FOverlapResult& Overlap) { return Overlap.bBlockingHit; });
			if (Blocker)
			{
				Causes += FString::Printf(TEXT(" %s+%.0f: %s;"), Side == EGTPVehicleSide::Left ? TEXT("left") : TEXT("right"), Distance,
					*GetNameSafe(Blocker->GetActor()));
				continue;
			}
			const FRotator Facing(0.f, (-Out).Rotation().Yaw, 0.f);
			Character->SetActorLocationAndRotation(Spot, Facing, false, nullptr, ETeleportType::TeleportPhysics);
			if (AController* Controller = Character->GetController())
			{
				Controller->SetControlRotation(FRotator(-10.f, Facing.Yaw, 0.f));
			}
			GTP_TRACE(LogGTPTheftDemo, TEXT("theft demo: player at the %s door of %s, %.0f cm out%s%s"),
				Side == EGTPVehicleSide::Left ? TEXT("driver") : TEXT("passenger"), *Car->GetName(), Distance,
				Causes.IsEmpty() ? TEXT("") : TEXT(" (blocked before:"), Causes.IsEmpty() ? TEXT("") : *(Causes + TEXT(")")));
			return true;
		}
	}
	GTP_TRACE(LogGTPTheftDemo, TEXT("theft demo: no free spot by the doors of %s:%s"), *Car->GetName(), *Causes);
	return false;
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
