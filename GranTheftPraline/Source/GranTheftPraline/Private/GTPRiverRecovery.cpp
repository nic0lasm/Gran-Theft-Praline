#include "GTPRiverRecovery.h"

#include "GTPLog.h"
#include "GTPVehicle.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPRiver, Log, All);

namespace
{
	const FName RecoveryTag(TEXT("GTP_RiverRecovery"));
	const FName ExitTag(TEXT("GTP_RiverExit"));
}

bool UGTPRiverRecovery::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UGTPRiverRecovery::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	int32 Boxes = 0;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (It->ActorHasTag(RecoveryTag))
		{
			It->OnActorBeginOverlap.AddDynamic(this, &UGTPRiverRecovery::OnEnterWater);
			++Boxes;
		}
		else if (It->ActorHasTag(ExitTag))
		{
			Exits.Add(It->GetActorTransform());
		}
	}
	if (Boxes > 0)
	{
		GTP_TRACE(LogGTPRiver, TEXT("river: %d water boxes, %d exit points"), Boxes, Exits.Num());
	}
}

bool UGTPRiverRecovery::FindExit(const FVector& Location, FTransform& OutExit) const
{
	double Best = TNumericLimits<double>::Max();
	for (const FTransform& Exit : Exits)
	{
		const double D = FVector::DistSquared(Exit.GetLocation(), Location);
		if (D < Best)
		{
			Best = D;
			OutExit = Exit;
		}
	}
	return Exits.Num() > 0;
}

void UGTPRiverRecovery::OnEnterWater(AActor* Water, AActor* Other)
{
	if (!Other || InWater.Contains(Other))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (AGTPVehicle* Car = Cast<AGTPVehicle>(Other))
	{
		InWater.Add(Car);
		Car->Drown();
		GTP_TRACE(LogGTPRiver, TEXT("river: %s fell in"), *Car->GetName());
		const bool bPlayerInside = Car->GetController() && Car->GetController()->IsPlayerController();
		if (bPlayerInside)
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &UGTPRiverRecovery::BringAshore, TWeakObjectPtr<APawn>(Car)), SinkSeconds, false);
		}
		FTimerHandle RemoveHandle;
		World->GetTimerManager().SetTimer(RemoveHandle, FTimerDelegate::CreateUObject(this, &UGTPRiverRecovery::RemoveCar, TWeakObjectPtr<AGTPVehicle>(Car)), RemoveCarSeconds, false);
		return;
	}
	ACharacter* Character = Cast<ACharacter>(Other);
	if (!Character || !Character->GetController() || !Character->GetController()->IsPlayerController())
	{
		return;
	}
	InWater.Add(Character);
	// Swim: float at the surface, slowed, for a few seconds.
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);
		Move->Velocity = FVector::ZeroVector;
	}
	GTP_TRACE(LogGTPRiver, TEXT("river: player in the water"));
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &UGTPRiverRecovery::BringAshore, TWeakObjectPtr<APawn>(Character)), SwimSeconds, false);
}

void UGTPRiverRecovery::BringAshore(TWeakObjectPtr<APawn> Pawn)
{
	APawn* Target = Pawn.Get();
	UWorld* World = GetWorld();
	if (!Target || !World)
	{
		return;
	}
	FTransform Exit;
	if (!FindExit(Target->GetActorLocation(), Exit))
	{
		GTP_TRACE(LogGTPRiver, TEXT("river: no exit point in this map"));
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Target->GetController());
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeSeconds, FLinearColor::Black, false, true);
	}
	// Teleport once the screen is black, then fade back in.
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Pawn, Exit]()
	{
		APawn* Current = Pawn.Get();
		if (!Current)
		{
			return;
		}
		const FVector Ashore = Exit.GetLocation() + FVector(0.f, 0.f, 100.f);
		const FRotator Facing(0.f, Exit.Rotator().Yaw, 0.f);
		ACharacter* Character = Cast<ACharacter>(Current);
		if (AGTPVehicle* Car = Cast<AGTPVehicle>(Current))
		{
			Character = Car->EjectDriverTo(Ashore, Facing);
		}
		if (!Character)
		{
			return;
		}
		Character->SetActorLocationAndRotation(Ashore, Facing, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_Walking);
		}
		if (AController* Controller = Character->GetController())
		{
			Controller->SetControlRotation(Facing);
		}
		InWater.Remove(Character);
		if (APlayerController* Player = Cast<APlayerController>(Character->GetController()))
		{
			if (Player->PlayerCameraManager)
			{
				Player->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeSeconds, FLinearColor::Black, false, false);
			}
		}
		GTP_TRACE(LogGTPRiver, TEXT("river: player back ashore at %s"), *Ashore.ToCompactString());
	}), FadeSeconds, false);
}

void UGTPRiverRecovery::RemoveCar(TWeakObjectPtr<AGTPVehicle> Car)
{
	AGTPVehicle* Target = Car.Get();
	if (!Target)
	{
		return;
	}
	InWater.Remove(Target);
	// A non-player driver goes with the car (the traffic network forgets destroyed cars).
	if (ACharacter* Occupant = Target->GetDriver())
	{
		if (!(Occupant->GetController() && Occupant->GetController()->IsPlayerController()))
		{
			if (AController* Controller = Occupant->GetController())
			{
				Controller->Destroy();
			}
			Occupant->Destroy();
		}
	}
	Target->Destroy();
}
