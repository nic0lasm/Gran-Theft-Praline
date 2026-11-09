#include "GTPGameMode.h"

#include "GTPHUD.h"
#include "GTPPlayerController.h"
#include "GTPVehicle.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPGameMode, Log, All);

AGTPGameMode::AGTPGameMode()
{
	PlayerControllerClass = AGTPPlayerController::StaticClass();
	HUDClass = AGTPHUD::StaticClass();
}

void AGTPGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (PlayerPawnClass.IsNull())
	{
		return;
	}

	if (UClass* PawnClass = PlayerPawnClass.TryLoadClass<APawn>())
	{
		DefaultPawnClass = PawnClass;
	}
	else
	{
		UE_LOG(LogGTPGameMode, Warning, TEXT("PlayerPawnClass '%s' could not be loaded, using the default pawn."),
			*PlayerPawnClass.ToString());
	}
}

void AGTPGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (bTestVehicleSpawned || !FParse::Param(FCommandLine::Get(), TEXT("TestVehicle")))
	{
		return;
	}
	const APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	// -TestVehicleClass=/Game/... overrides the config (one car per playtest run).
	FString ClassOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("TestVehicleClass="), ClassOverride))
	{
		TestVehicleClass = FSoftClassPath(ClassOverride);
	}
	UClass* VehicleClass = TestVehicleClass.TryLoadClass<AGTPVehicle>();
	if (!Pawn || !VehicleClass)
	{
		UE_LOG(LogGTPGameMode, Warning, TEXT("-TestVehicle: no pawn yet or TestVehicleClass '%s' not loadable"), *TestVehicleClass.ToString());
		return;
	}
	bTestVehicleSpawned = true;
	const FRotator Yaw(0.f, Pawn->GetActorRotation().Yaw, 0.f);
	const FVector Location = Pawn->GetActorLocation() + Yaw.RotateVector(TestVehicleOffset);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	// Parked parallel to the street, as the player faces down it.
	const AActor* Car = GetWorld()->SpawnActor<AGTPVehicle>(VehicleClass, Location, Yaw, Params);
	UE_LOG(LogGTPGameMode, Log, TEXT("-TestVehicle: %s parked at %s"), Car ? *Car->GetName() : TEXT("nothing"), *Location.ToCompactString());
}
